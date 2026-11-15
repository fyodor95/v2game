#pragma once

#include "mp_save.h"
#include "../../creatinecore/gpc/runtime_status.h"
#include <set>

// Статически квалифицированный кандидат. Runtime-проверка трёх переходов
// обязательна: docs/research/gpc-sp-return-menu-contracts.md.
// Только owner thread. Ticket живёт до завершения одного возврата в меню;
// после этого мост уничтожает его и сбрасывает захваченные объекты кампании.
namespace vic2_sp_frontend {

struct Ticket {
    DWORD world = 0, idler = 0, game = 0, date = 0;
};

struct State {
    bool campaign = false, confirmation = false, summary = false, main_menu = false;
    bool resign_queued = false;
};

namespace detail {
using vic2_mp_lobby::Copy;
using vic2_mp_lobby::Field;
using vic2_mp_lobby::Control;
using vic2_mp_lobby::Method;
using vic2_mp_lobby::Press;
using vic2_mp_save::detail::Flag;
using vic2_mp_save::detail::Signature;

static void Signatures(DWORD base) {
    vic2_mp_save::detail::Signatures(base);
    const unsigned char resign[] = {0x55,0x8b,0xec,0x6a,0xff,0x68};
    const unsigned char confirm[] = {0x56,0x8b,0xf1,0x83,0x7e,0x7c,0x10,0x8d,0x46,0x68};
    const unsigned char request[] = {0xc6,0x81,0x21,0x1d,0,0,1,0xc3};
    const unsigned char agree[] = {0x8b,0x01,0xff,0x60,0x18};
    const unsigned char exit[] = {0x55,0x8b,0xec,0x83,0xe4,0xf8,0x64,0xa1,0,0};
    const unsigned char binding[] = {0x8d,0x48,0x54,0x8b,0x01,0x8b,0x40,0x04,
        0x8d,0x97,0xb0,0x05,0,0,0x52,0xff,0xd0};
    const unsigned char activate[] = {0x8b,0xbe,0xe0,0,0,0,0x89,0x9e,0xe0,0,0,0,
        0x8b,0x8e,0x84,0,0,0,0x3b,0xf9,0x74,0x0c,0x3b,0xcb,0x74,0x08,
        0x8b,0x11,0x8b,0x02,0x6a,0x01,0xff,0xd0,0x89,0xbe,0x84,0,0,0};
    Signature(base,0x2766a0,resign);
    Signature(base,0x276500,confirm);
    Signature(base,0x24edc0,request);
    Signature(base,0x240b20,agree);
    Signature(base,0x1fb560,exit);
    Signature(base,0x1f935c,binding);
    Signature(base,0x5df4db,activate);
    if (Field(base+0xa0a34c+0x114) != base+0x24edc0 ||
        Field(base+0xa0a7e4+0x18) != base+0x276500 ||
        Field(base+0x1fb61a)+base+0x1fb61e != base+0x354a00)
        throw std::runtime_error("E_SP_FRONTEND_SIGNATURE");
}

static void Completed(DWORD date) {
    HMODULE module = GetModuleHandleA("ccore_great_power_culture_cores.dll");
    if (!module) return;
    auto getter = (V2GpcGetRuntimeStatusFn)GetProcAddress(module,V2_GPC_RUNTIME_STATUS_EXPORT);
    V2GpcRuntimeStatus status = {};
    if (!getter || !getter(&status,sizeof(status)) || status.size != sizeof(status) ||
        status.version != V2_GPC_RUNTIME_STATUS_VERSION)
        throw std::runtime_error("E_GPC_STATUS_API");
    if (status.busy || (status.enabled && (status.failed || status.last_completed_raw_date != date)))
        throw std::runtime_error("E_SP_FRONTEND_GPC_PENDING");
}

static vic2_mp::State Require(DWORD base) {
    Signatures(base);
    vic2_mp::State state = vic2_mp::Inspect(base);
    if (state.mode != 0) throw std::runtime_error("E_SP_ONLY");
    if (!state.paused || state.executing) throw std::runtime_error("E_SP_FRONTEND_REQUIRES_IDLE_PAUSE");
    Completed(Field(state.world+0xb0c));
    return state;
}

// Зарегистрированные диалоги принадлежат idler; +60 head, node+0 value,
// node+8 next. Идентичность, владелец, тип RESIGN и видимость проверяются
// вместе: одно совпадение vtable не разрешает подтвердить посторонний диалог.
static DWORD Confirmation(DWORD base, DWORD idler) {
    DWORD found = 0;
    std::set<DWORD> seen;
    for (DWORD node=Field(idler+0x60); node; node=Field(node+8)) {
        if (seen.size() >= 256 || !seen.insert(node).second)
            throw std::runtime_error("E_SP_FRONTEND_DIALOG_LIST");
        DWORD dialog=Field(node);
        if (!dialog || Field(dialog) != base+0xa0a7e4) continue;
        if (Field(dialog+8) != idler || Flag(dialog+0x64)) continue;
        if (Field(dialog+0x78) != 6 || Field(dialog+0x7c) < 6)
            throw std::runtime_error("E_SP_FRONTEND_DIALOG_TEXT");
        DWORD text=Field(dialog+0x7c) >= 16 ? Field(dialog+0x68) : dialog+0x68;
        char name[7] = {};
        if (!Copy(text,name,sizeof(name)) || memcmp(name,"RESIGN",7)) continue;
        if (!Flag(Field(dialog+4)+0x67)) continue;
        if (found) throw std::runtime_error("E_SP_FRONTEND_AMBIGUOUS_DIALOG");
        found=dialog;
    }
    return found;
}

static void Same(const Ticket& ticket, const vic2_mp::State& state) {
    if (!ticket.world || state.world != ticket.world || state.idler != ticket.idler ||
        Field(state.idler+0x17e8) != ticket.game || Field(state.world+0xb0c) != ticket.date)
        throw std::runtime_error("E_SP_FRONTEND_STALE_TICKET");
}
} // namespace detail

// Не вызывает действий. На первом main_menu=true ticket больше не читать.
static State Inspect(DWORD base, const Ticket& ticket, DWORD capturedMainMenu) {
    using namespace detail;
    Signatures(base);
    State result;
    if (capturedMainMenu && Field(capturedMainMenu) == base+0xa13dbc) {
        // Старый frontend не считается ACK: текущий idler game+84 должен
        // совпадать с новым объектом, захваченным constructor hook.
        if (ticket.game && Field(ticket.game+0x84) == capturedMainMenu) {
            result.main_menu=true;
            return result;
        }
    }
    DWORD world=Field(base+0xe588e8);
    if (!world || world != ticket.world) throw std::runtime_error("E_SP_FRONTEND_STALE_TICKET");
    DWORD idler=Field(world+0xb24);
    if (!idler) return result;
    DWORD table=Field(idler);
    if (table == base+0xa0a34c) {
        auto state=Require(base);
        Same(ticket,state);
        result.campaign=true;
        result.resign_queued=Flag(idler+0x1d21);
        result.confirmation=Confirmation(base,idler) != 0;
    } else if (table == base+0xa04cfc) {
        if (Field(idler+0xa0) != ticket.game)
            throw std::runtime_error("E_SP_FRONTEND_STALE_TICKET");
        // Constructor уже обновил world+B24, но owner-loop ещё мог не
        // перенести pending +E0 в active +84. В таком случае только poll.
        if (Field(ticket.game+0x84) != idler) return result;
        DWORD panel=Field(idler+0xa8);
        result.summary=panel && Flag(panel+0x67);
    }
    return result;
}

// Вызывающий заранее запрещает любые pending advance/event/save запросы.
// После начала native-вызова ошибка означает неизвестный исход; повторять
// мутацию нельзя. Следующий запрос — только Inspect.
static Ticket RequestResign(DWORD base) {
    using namespace detail;
    const auto state=Require(base);
    Ticket ticket{state.world,state.idler,Field(state.idler+0x17e8),Field(state.world+0xb0c)};
    // Контекст держит active idler в +84; +E0 — pending замена, которую
    // штатный owner-loop переносит в +84 и обнуляет.
    if (Field(ticket.game) != base+0xa0845c || Field(ticket.game+0x84) != state.idler ||
        Field(state.idler+0x4ac) != state.idler || Field(state.idler+0x4b0) != base+0x2766a0)
        throw std::runtime_error("E_SP_FRONTEND_CALLBACK_LAYOUT");
    if (Confirmation(base,state.idler) || Flag(state.idler+0x1d21))
        throw std::runtime_error("E_SP_FRONTEND_ALREADY_PENDING");
    DWORD menu=vic2_mp_save::detail::Menu(base,state.idler);
    DWORD panel=Control(base,menu,0x6c,"menu_panel");
    DWORD button=Control(base,panel,0x34,"menu_resign_button");
    try {
        if (!Flag(menu+0x67) &&
            !vic2_mp_save::detail::InvokeMenu(base+0x26eee0,state.idler))
            throw std::runtime_error("E_NATIVE_MENU");
        if (!Flag(menu+0x67)) throw std::runtime_error("E_NATIVE_MENU");
        Press(base,button);
        return ticket;
    } catch (...) { throw std::runtime_error("E_SP_FRONTEND_OUTCOME_UNKNOWN"); }
}

static void ConfirmResign(DWORD base, const Ticket& ticket) {
    using namespace detail;
    auto state=Require(base);
    Same(ticket,state);
    if (Flag(state.idler+0x1d21)) throw std::runtime_error("E_SP_FRONTEND_ALREADY_PENDING");
    DWORD dialog=Confirmation(base,state.idler);
    if (!dialog) throw std::runtime_error("E_SP_FRONTEND_CONFIRMATION_REQUIRED");
    if (Field(dialog+0x10) != dialog || Field(dialog+0x14) != base+0x240b20)
        throw std::runtime_error("E_SP_FRONTEND_CALLBACK_LAYOUT");
    DWORD button=Control(base,Field(dialog+4),0x34,"AgreeButton");
    try { Press(base,button); }
    catch (...) { throw std::runtime_error("E_SP_FRONTEND_OUTCOME_UNKNOWN"); }
}

static void ExitSummary(DWORD base, const Ticket& ticket, bool afterSummaryUpdate = false) {
    using namespace detail;
    // Синхронное создание CFrontEnd меняет общий GUI registry. Callback
    // разрешён только после возврата исходного CBackEndIdler::Update.
    if (!afterSummaryUpdate) throw std::runtime_error("E_SP_FRONTEND_UNSAFE_PHASE");
    if (!Inspect(base,ticket,0).summary) throw std::runtime_error("E_SP_FRONTEND_SUMMARY_REQUIRED");
    DWORD idler=Field(ticket.world+0xb24);
    if (Field(idler+0x5b4) != idler || Field(idler+0x5b8) != base+0x1fb560)
        throw std::runtime_error("E_SP_FRONTEND_CALLBACK_LAYOUT");
    DWORD button=Control(base,Field(idler+0xa8),0x34,"Exit");
    // Callback может освободить текущий idler: никаких чтений старых
    // объектов после Signal. ACK — отдельный Inspect с новым capturedMainMenu.
    try { Press(base,button); }
    catch (...) { throw std::runtime_error("E_SP_FRONTEND_OUTCOME_UNKNOWN"); }
}

} // namespace vic2_sp_frontend

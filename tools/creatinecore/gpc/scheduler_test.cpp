// Проверки чистого планировщика без Victoria II, WinAPI и нативных hooks.
#include "scheduler.h"
#include <iostream>
#include <vector>

using namespace gpc;

static unsigned checks = 0;

static void Require(bool condition, const char* description) {
    ++checks;
    if (!condition) throw std::runtime_error(description);
}

template<class Exception, class Fn>
static void Throws(Fn fn, const char* description) {
    bool thrown = false;
    try { fn(); } catch (const Exception&) { thrown = true; }
    Require(thrown, description);
}

static void RandomVectors() {
    const std::array<std::pair<std::uint32_t, std::array<std::uint32_t, 5>>, 3> cases{{
        {1, {270369u, 67634689u, 2647435461u, 307599695u, 2398689233u}},
        {0x12345678u, {2274908837u, 358294691u, 1210119364u, 2176035992u, 1882851208u}},
        {0xFFFFFFFFu, {253983u, 4228382207u, 1958451267u, 4056713434u, 2049502865u}}
    }};
    for (const auto& test : cases) {
        auto state = test.first;
        for (auto expected : test.second) {
            state = XorShift32(state);
            Require(state == expected, "Контрольная последовательность xorshift32");
        }
    }
    Throws<std::invalid_argument>([] { XorShift32(0); }, "Нулевой RNG отвергается");
    const auto zero = InitializeSchedule(0, 0);
    Require(zero.seed == 0x6D2B79F5u && zero.rng == zero.seed, "Замена только начального нулевого seed");
}

static void BoundedRanges() {
    auto sequence = [](std::vector<std::uint32_t> values, std::uint32_t bound) {
        std::size_t next = 0;
        const auto result = DrawNonzeroBounded([&]() { return values.at(next++); }, bound);
        Require(result.draws == next, "Счётчик учитывает отвергнутые числа");
        return result;
    };
    // Для 2^32-1 исходных значений лимит равен 4294000000, не 4294967296.
    auto draw = sequence({0, 4294967295u, 4294000001u, 4294000000u}, 1000000);
    Require(draw.value == 999999 && draw.draws == 4, "Ноль и неполный хвост отвергаются");
    draw = sequence({1}, 1000000);
    Require(draw.value == 0, "Нижняя граница диапазона");
    draw = sequence({1000000}, 1000000);
    Require(draw.value == 999999, "Верхняя граница диапазона");
    draw = sequence({0xFFFFFFFFu}, 1);
    Require(draw.value == 0, "Диапазон из одного значения");
    draw = sequence({0xFFFFFFFFu}, 0xFFFFFFFFu);
    Require(draw.value == 0xFFFFFFFEu, "Максимальный uint32 диапазон");
    draw = sequence({0x80000001u, 1u}, 0x80000000u);
    Require(draw.value == 0 && draw.draws == 2, "Широкий диапазон не переполняется");
    Throws<std::invalid_argument>([&] { sequence({}, 0); }, "Нулевой диапазон отвергается до вызова Next");
}

static void DatesAndTicks() {
    Require(ScheduleDayFromRaw(0x029C55C0) == 0, "Эпоха дат");
    Require(ScheduleDayFromRaw(0x029C55C0 + 24) == 1, "Часы преобразуются в дни");
    Require(kScheduleMaximumDay == 87653485u, "Предел положительной даты движка");
    Require(ScheduleDayFromRaw(kScheduleDateEpoch + 24ll * kScheduleMaximumDay) == kScheduleMaximumDay,
            "Последняя допустимая граница дня");
    Throws<std::invalid_argument>([] { ScheduleDayFromRaw(kScheduleDateEpoch - 1); }, "Дата до эпохи");
    Throws<std::invalid_argument>([] { ScheduleDayFromRaw(kScheduleDateEpoch + 1); }, "Неполный день");
    Throws<std::invalid_argument>([] { ScheduleDayFromRaw(0x80000000ll); }, "Дата больше int32");
    Throws<std::invalid_argument>([] { InitializeSchedule(1, kScheduleMaximumDay + 1); }, "Слишком большой день");
    Require(!IsCountryEventDay(0) && IsCountryEventDay(1) && !IsCountryEventDay(2), "Страновые дни нечётные");

    const auto initial = InitializeSchedule(1, 100);
    auto tick = TickSchedule(initial, 100, true, 1000000);
    Require(tick.next == initial && !tick.attempted && !tick.due && !tick.draws,
            "Инициализация и повтор callback не создают попытки");
    tick = TickSchedule(initial, 101, true, 1000000);
    Require(tick.attempted && tick.due && tick.draws == 1 && tick.next.rng == 270369 &&
            tick.next.pass_sequence == 1 && tick.next.last_day == 101, "Полный шанс на нечётном дне");
    const auto once = tick.next;
    tick = TickSchedule(once, 101, true, 1000000);
    Require(tick.next == once && !tick.attempted && !tick.draws, "Повторное срабатывание даты исключено");
    tick = TickSchedule(once, 102, true, 1000000);
    Require(!tick.attempted && !tick.due && !tick.draws && tick.next.last_day == 102 &&
            tick.next.rng == once.rng && tick.next.pass_sequence == once.pass_sequence,
            "Чётный день обновляет дату без RNG");
    tick = TickSchedule(initial, 101, true, 0);
    Require(tick.attempted && !tick.due && tick.next.pass_sequence == 0 && tick.next.rng == 270369,
            "Нулевой шанс не выдаёт проход, но попытка расходует RNG");
    Require(!TickSchedule(initial, 101, true, 270368).due, "Строгое сравнение с порогом");
    Require(TickSchedule(initial, 101, true, 270369).due, "Значение меньше порога срабатывает");
    tick = TickSchedule(initial, 101, false, 1000000);
    Require(!tick.attempted && !tick.due && !tick.draws && tick.next.rng == initial.rng &&
            tick.next.pass_sequence == 0 && tick.next.last_day == 101, "Выключение меняет только дату");
    const auto disabled = tick.next;
    Require(TickSchedule(disabled, 101, true, 1000000).next == disabled, "Включение в тот же день без повторной попытки");
    Require(TickSchedule(disabled, 103, true, 1000000).next.rng == 270369, "Включение продолжает прежний RNG");
    Require(TickSchedule(initial, 10001, true, 1000000).next.rng == 270369, "Нет воспроизведения пропущенных дней");
    Throws<std::invalid_argument>([&] { TickSchedule(initial, 99, true, 555); }, "Дата назад отвергается");
    Throws<std::invalid_argument>([&] { TickSchedule(initial, 101, true, 1000001); }, "Неверный порог");
    Require(initial == InitializeSchedule(1, 100), "Входное состояние неизменно");

    auto overflow = initial;
    overflow.pass_sequence = 0xFFFFFFFFu;
    Throws<std::overflow_error>([&] { TickSchedule(overflow, 101, true, 1000000); }, "Переполнение номера прохода");
    Require(overflow.rng == 1 && overflow.last_day == 100 && overflow.pass_sequence == 0xFFFFFFFFu,
            "Переполнение не изменяет исходное состояние");
    Require(!TickSchedule(overflow, 101, true, 0).due, "Полный счётчик допустим без нового прохода");
}

static void ReplaceWord(ScheduleFlags& flags, const std::string& field, std::uint32_t word) {
    // Имена строятся независимо от helper кодека: проверяем публичный формат.
    for (unsigned bit = 0; bit < 32; ++bit) {
        const auto stem = "cc_gpc_v1_" + field + "_b" +
            static_cast<char>('0' + bit / 10) + static_cast<char>('0' + bit % 10);
        const bool one = ((word >> bit) & 1u) != 0;
        flags[stem + "_0"] = !one;
        flags[stem + "_1"] = one;
    }
}

static void Codec() {
    const auto initial = InitializeSchedule(1, 100);
    Require(ScheduleChecksum(initial) == 0xF77E80F0u, "Контрольная сумма LE FNV-1a");
    const auto encoded = EncodeSchedule(initial);
    Require(encoded.size() == 386, "Фиксированный словарь из 386 имён");
    unsigned active = 0;
    for (const auto& flag : encoded) if (flag.second) ++active;
    Require(active == 193, "Ровно 193 активных флага завершённого состояния");
    Require(encoded.at("cc_gpc_v1_seed_b00_1") && !encoded.at("cc_gpc_v1_seed_b00_0"), "Младший бит seed");
    Require(encoded.at("cc_gpc_v1_last_day_b06_1") && encoded.at("cc_gpc_v1_last_day_b00_0"), "Биты даты");
    Require(DecodeSchedule(encoded).value() == initial, "Полный образ декодируется");
    auto compact = encoded;
    for (auto it = compact.begin(); it != compact.end();) {
        if (!it->second) it = compact.erase(it); else ++it;
    }
    compact["unrelated_game_flag"] = true;
    Require(DecodeSchedule(compact).value() == initial, "Неактивные узлы могут отсутствовать, чужие флаги игнорируются");
    Require(!DecodeSchedule({}), "Пустое сохранение");
    Require(!DecodeSchedule(EmptyScheduleFlagDictionary()), "Все очищенные узлы означают отсутствие состояния");
    Require(!DecodeSchedule({{"unrelated_game_flag", true}}), "Только чужой флаг не является GPC");

    auto malformed = encoded;
    malformed["cc_gpc_v1_rng_b00_0"] = true;
    Throws<std::invalid_argument>([&] { DecodeSchedule(malformed); }, "Обе половины бита активны");
    malformed = encoded;
    malformed["cc_gpc_v1_rng_b00_1"] = false;
    Throws<std::invalid_argument>([&] { DecodeSchedule(malformed); }, "Обе половины бита неактивны");
    malformed = encoded;
    ReplaceWord(malformed, "rng", 2);
    Throws<std::invalid_argument>([&] { DecodeSchedule(malformed); }, "Корректные пары с повреждёнными данными обнаруживаются checksum");
    malformed = encoded;
    malformed[kScheduleReadyFlag] = false;
    Throws<std::invalid_argument>([&] { DecodeSchedule(malformed); }, "Нет маркера готовности");
    malformed = encoded;
    malformed[kScheduleWritingFlag] = true;
    Throws<std::invalid_argument>([&] { DecodeSchedule(malformed); }, "Незавершённая запись");
    Throws<std::invalid_argument>([] { DecodeSchedule({{kScheduleWritingFlag, true}}); }, "Один маркер записи не позволяет переинициализацию");
    Throws<std::invalid_argument>([] { DecodeSchedule({{"cc_gpc_v1_seed_b00_0", true}}); }, "Активный фрагмент не является новым сейвом");
    Throws<std::invalid_argument>([] { DecodeSchedule({{"cc_gpc_v2_ready", true}}); }, "Неизвестное пространство версии");
    malformed = encoded;
    malformed["cc_gpc_v1_typo"] = true;
    Throws<std::invalid_argument>([&] { DecodeSchedule(malformed); }, "Неизвестный активный флаг");
    for (const auto& field : {"schema", "seed", "rng", "last_day"}) {
        malformed = encoded;
        const std::uint32_t value = std::string(field) == "schema" ? 2u :
            (std::string(field) == "last_day" ? kScheduleMaximumDay + 1 : 0u);
        ReplaceWord(malformed, field, value);
        Throws<std::invalid_argument>([&] { DecodeSchedule(malformed); }, "Неверное значение поля отвергается");
    }
    auto bad = initial;
    bad.rng = 0;
    Throws<std::invalid_argument>([&] { EncodeSchedule(bad); }, "Недопустимое состояние не кодируется");
    Throws<std::invalid_argument>([&] { TickSchedule(bad, 101, true, 555); }, "Недопустимое состояние не исполняется");
}

static void Replay() {
    for (auto seed : {1u, 0x12345678u, 0xFFFFFFFFu}) {
        auto uninterrupted = InitializeSchedule(seed, 0);
        auto reloaded = uninterrupted;
        unsigned events = 0;
        for (Day day = 1; day <= 10000; ++day) {
            const bool enabled = day < 1000 || day > 2000;
            const auto a = TickSchedule(uninterrupted, day, enabled, 10000);
            const auto b = TickSchedule(reloaded, day, enabled, 10000);
            Require(a.next == b.next && a.attempted == b.attempted && a.due == b.due && a.draws == b.draws,
                    "Перезагрузка сохраняет точную последовательность");
            uninterrupted = a.next;
            reloaded = b.next;
            if (day % 29 == 0) {
                const auto flags = EncodeSchedule(reloaded);
                reloaded = DecodeSchedule(flags).value();
                Require(EncodeSchedule(reloaded) == flags, "Повторное кодирование детерминировано");
                Require(flags.size() == 386, "Словарь не растёт с числом проходов");
            }
            if (a.due) ++events;
        }
        Require(events > 0 && uninterrupted.pass_sequence == events, "Прогон включает действительные проходы");
    }
}

int main() {
    try {
        RandomVectors();
        BoundedRanges();
        DatesAndTicks();
        Codec();
        Replay();
        std::cout << "Планировщик: пройдено проверок — " << checks << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Ошибка после " << checks << " проверок: " << error.what() << '\n';
        return 1;
    }
}

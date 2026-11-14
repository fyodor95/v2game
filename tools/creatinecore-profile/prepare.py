"""Подготовка профиля и русских текстов закреплённого CreatineCore."""
from __future__ import annotations

import argparse
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[2]
UPSTREAM = ROOT / ".work/creatinecore/upstream/CREATINECORE_LATEST_BUILD"
ENABLED = set("""
ARMY_FIX_BAD_REINFORCEMENT NAVY_FIX_BAD_REINFORCEMENT
ARMY_BETTER_REINFORCE_ON_ALLIED_OWNED_LAND NAVY_ALLOW_USE_ALLY_TRANSPORTS
COMBAT_SECOND_LINE_SUPPORT_UNIT_PRIORITY
TRADE_PRICE_DELTA_EXPONENTIAL TRADE_MAX_PRICE_20X_MULTIPLIER FACTORY_AI_FACTORY_PRIORITY_RULESWAP
POP_CARD_NEEDS_AFFECTED_BY_NEEDS_MODIFIERS_FIX POP_BUDGET_PAYOUT_NEEDS_SCALED
POP_FIX_NEEDS_MODIFIERS ENABLE_VERSION_LABEL ENABLE_VASSAL_COLORS
FIX_BUDGET_TOTAL_INCOME_LABEL FACTORY_MAINTENANCE_DISPLAY_FIX PATCH_NEEDS_HONEST_UI
PATCH_CIVILIZE_NULL_CHECK FIX_MEMORY_MISS_CRASHES MUSIC_FAIR_RANDOM
PATCH_TECH_NULL_CHECK_FIXES PATCH_SUPPLY_SOURCE_NULL_CHECK LOAD_DLL_FROM_MOD_FOLDER
ENABLE_ASSIMILATE_BLOCK BUILDING_GOODS_CONSUMPTION POP_SALARY_ADDITIONAL_MULTIPLIER
ENABLE_MOVE_POP_BLOCK ENABLE_IF_ELSE_OPERATORS FIX_TECH_FACTORY_MAINTENANCE
FIX_PROVINCE_FLAGS NEW_COUNTRY_SCOPE_CONDITIONS NEW_POP_MODIFIERS
ACTIVATE_DEACTIVATE_UNIT CREATE_SUBSTATE JOIN_WAR_EFFECT
PATCH_COMPRESS_FLAG_CACHE PATCH_PREFETCH_POPS PATCH_PREFETCH_RELATIONS
PATCH_PREFETCH_AI_RELATIONS ENABLE_LOG ENABLE_CRASH_LOG
""".split())
SPECIAL = {
    "LOCAL_MOD_CONFIG": "0", "COMBAT_ROLL_MIN": "0", "COMBAT_ROLL_MAX": "9",
    "VASSAL_COLOR_PERCENT": "65", "FACTORY_LACK_ICON_DEBOUNCE_MS": "150",
    "STATE_SAVINGS_CAP_POUNDS": "60000", "FACTORY_COLONIAL_EXTRA_WHITELIST": "",
    "SP_WINDOW_FROM": "", "SP_WINDOW_TO": "", "MONEY_WATCH_TAGS": "",
}
EXPLICIT_EXTRA = (
    "PATCH_NEEDS_HONEST_UI", "PATCH_FACTORY_DUMP_SCAN",
    "PATCH_WAR_ON_PUPPET_ALLOWED", "PATCH_WAR_ON_PUPPET_TOOLTIP_1",
    "PATCH_WAR_ON_PUPPET_TOOLTIP_2",
)
TEXT = {
    "TRUE_EXIT": "Перезапустить игру",
    "PRIMARY_CULTURE_ONLY_RECRUITMENT": "Только основная культура",
    "ACCEPTED_CULTURE_ONLY_RECRUITMENT": "Только признанные культуры",
    "NON_ACCEPTED_ONLY_RECRUITMENT": "Только непризнанные культуры",
    "COLONIAL_ONLY_RECRUITMENT": "Только колонии",
    "NON_COLONIAL_ONLY_RECRUITMENT": "Только неколониальные области",
    "MOVE_POP_AMOUNT_EFFECT": "Переместить §Y$AMOUNT$§! населения в провинцию §Y$WHERE$§W.",
    "PROV_NON_ACC_MIL_MODIFIER": "Агрессивность непризнанных культур в провинции",
    "PROV_NON_ACC_CON_MODIFIER": "Сознательность непризнанных культур в провинции",
    "PROV_PRIMARY_MIL_MODIFIER": "Агрессивность основной культуры в провинции",
    "PROV_PRIMARY_CON_MODIFIER": "Сознательность основной культуры в провинции",
    "PROV_ACC_MIL_MODIFIER": "Агрессивность признанных культур в провинции",
    "PROV_ACC_CON_MODIFIER": "Сознательность признанных культур в провинции",
    "PRIMARY_POP_MILITANCY_MODIFIER": "Агрессивность основной культуры",
    "PRIMARY_POP_CONSCIOUSNESS_MODIFIER": "Сознательность основной культуры",
    "ACCEPTED_POP_MILITANCY_MODIFIER": "Агрессивность признанных культур",
    "ACCEPTED_POP_CONSCIOUSNESS_MODIFIER": "Сознательность признанных культур",
    "COLONIAL_POP_MILITANCY_MODIFIER": "Агрессивность населения колоний",
    "COLONIAL_POP_CONSCIOUSNESS_MODIFIER": "Сознательность населения колоний",
    "CC_COUNTRY_RELIGION_IS": "Государственная религия: ",
    "CC_COUNTRY_RELIGION_IS_NOT": "Государственная религия не является: ",
    "ASSIMILATE_CULTURE_EFFECT": "Изменить культуру §Y$AMOUNT$§! населения на $CULTURE$.",
    "ASSIMILATE_RELIGION_EFFECT": "Обратить §Y$AMOUNT$§! населения в $RELIGION$.",
    "DISABLE_UNIT_TECH": "Запрещает формирование",
    "ACTIVATE_UNIT_EFFECT": "Разрешить формирование: $UNIT$",
    "DEACTIVATE_UNIT_EFFECT": "Запретить формирование: $UNIT$",
    "UNIT_ENABLED_IS": "Доступен тип войск: ",
    "UNIT_ENABLED_NOT": "Недоступен тип войск: ",
    "UNIT_WILL_BE_DEACTIVATED_BY_SOURCE": "§Y$UNIT$§! станет недоступен для формирования после открытия §Y$SOURCE$§!",
    "CREATE_SUBSTATE_EFFECT": "§Y$TARGET_COUNTRY$§W становится автономным подчинённым государством страны §Y$COUNTRY$§W.\\n",
    "JOIN_WAR_SIDE_LINE": "Вступить в войну на стороне страны $COUNTRY$",
    "JOIN_WAR_AGAINST_LINE": "Вступить в войну против страны $COUNTRY$",
    "BUDGET_GOODS_CONS_HEADER": "Товары для содержания зданий",
    "BUDGET_GOODS_CONS_LEVELS": "уровней",
    "BUDGET_GOODS_CONS_TOTAL": "Всего в день",
    "BUDGET_GOODS_CONS_BOUGHT": "куплено",
    "BUDGET_GOODS_CONS_NEED": "требуется",
    "BUDGET_GOODS_CONS_FILL": "Закупки выполнены",
    "BUDGET_GOODS_CONS_LABEL": "Содержание зданий",
    "GOODS_CONS_NAME_naval_base": "Морские базы",
    "GOODS_CONS_NAME_railroad": "Железные дороги",
    "GOODS_CONS_NAME_fort": "Крепости",
    "TECH_FACTORY_MAINTENANCE": "Содержание фабрик:",
    "CC_PROVINCE_FLAG_IS": "В провинции установлен флаг ",
    "CC_PROVINCE_FLAG_IS_NOT": "В провинции не установлен флаг ",
    "CC_DATE_AT_LEAST": "Дата не ранее ",
    "CC_DATE_BEFORE": "Дата раньше ",
    "CC_INVALID_MODIFIER": "Неизвестный модификатор",
}
EXISTING_TEXT = {
    "MODIFIER_RGO_INPUT": "Дополнительный доход населения",
    "RGO_INPUT_TECH": "Дополнительный доход населения",
}
DEFINES = """# Дополнительные настройки CreatineCore 6.43.
# Параметры отключённых функций сохранены для предотвращения автодополнения.
factory_unprofitable_close_days = 60
minting_formula =
ai_factory_expand_min_staffing = 90
factory_closed_delete_days = 90
ai_naval_base_max_provinces_per_state = 1
factory_min_wage_per_10000 = 7
# Нулевая глобальная прибавка; pop_income_modifier остаётся доступен скриптам.
pop_salary_additional_multiplier = 0
price_delta_exempt_goods =
"""


def assignments(text: str) -> dict[str, str]:
    """Читает настройки, пропуская комментарии."""
    return dict(re.findall(r"^([A-Z][A-Z_0-9]*)[ \t]*=[ \t]*([^\r\n;]*)", text, re.M))


def profile() -> str:
    upstream = (UPSTREAM / "z_CreatineCore/creatinecore_settings.ini").read_text(encoding="utf-8-sig")
    keys = list(assignments(upstream))
    for key in EXPLICIT_EXTRA:
        if key not in keys:
            keys.append(key)
    if ENABLED - set(keys):
        raise ValueError(f"Неизвестные настройки: {sorted(ENABLED - set(keys))}")
    lines = ["; Профиль v2game для CreatineCore 6.43.",
             "; Все параметры заданы явно. Дополнительные DLL-оптимизаторы не устанавливаются.",
             "; Перенаправление урона второй линии отложено: COMBAT_SECOND_LINE_DAMAGE_REDIRECT=0.",
             "; Расходы зданий не заданы; глобальная прибавка дохода POP равна нулю в defines_ccore.txt."]
    lines.extend(f"{key}={SPECIAL.get(key, '1' if key in ENABLED else '0')}" for key in keys)
    return "\n".join(lines) + "\n"


def validate_translation() -> None:
    upstream = (UPSTREAM / "localisation/creatinecore.csv").read_text(encoding="cp1252")
    for line in upstream.splitlines():
        if not line or line.startswith("#"):
            continue
        key, english = line.split(";", 2)[:2]
        if key not in TEXT:
            raise ValueError(f"Нет перевода: {key}")
        placeholders = lambda value: sorted(re.findall(r"\$[A-Z_]+\$", value))
        if placeholders(english) != placeholders(TEXT[key]):
            raise ValueError(f"Повреждены подстановки: {key}")


def output_files() -> dict[Path, bytes]:
    validate_translation()
    result = {
        ROOT / "tools/creatinecore-profile/creatinecore_settings.ini": profile().encode("cp1251"),
        ROOT / "common/defines_ccore.txt": DEFINES.replace("\n", "\r\n").encode("cp1251"),
        ROOT / "localisation/creatinecore.csv": ("#CODE;ENGLISH;X\r\n" + "".join(f"{key};{value};X\r\n" for key, value in TEXT.items())).encode("cp1251"),
    }
    original = ROOT / "localisation/all_orig.csv"
    data = original.read_bytes().decode("cp1251")
    for key, value in EXISTING_TEXT.items():
        data, count = re.subn(rf"^{key};[^;]*;", f"{key};{value};", data, flags=re.M)
        if count != 1:
            raise ValueError(f"Ожидается одна строка {key}, найдено {count}")
    result[original] = data.encode("cp1251")
    defines_path = ROOT / "common/defines.lua"
    data, count = re.subn(rb"(MAX_FACTORY_MONEY_SAVE\s*=\s*)(?:1000|3000)(\s*,)", rb"\g<1>3000\2", defines_path.read_bytes())
    if count != 1:
        raise ValueError("Не найден единственный MAX_FACTORY_MONEY_SAVE")
    result[defines_path] = data
    gui_path = ROOT / "interface/country_budget.gui"
    gui = gui_path.read_bytes()
    if b'name = "naval_base_expense"' not in gui:
        anchor = b'\t\tinstantTextBoxType = { \r\n\t\t\tname = "total_exp"'
        if gui.count(anchor) != 1:
            raise ValueError("Не найдено место для строки расходов зданий")
        addition = '''\t\tinstantTextBoxType = {
\t\t\tname = "building_maintenance_desc"
\t\t\tposition = { x = 590 y = 432 }
\t\t\tfont = "Arial12_black"
\t\t\ttext = "BUDGET_GOODS_CONS_LABEL"
\t\t\tmaxWidth = 160
\t\t\tmaxHeight = 14
\t\t\tformat = left
\t\t}
\t\tinstantTextBoxType = {
\t\t\tname = "naval_base_expense"
\t\t\tposition = { x = 760 y = 432 }
\t\t\tfont = "Arial12_black"
\t\t\ttext = "0¤"
\t\t\tmaxWidth = 70
\t\t\tmaxHeight = 14
\t\t\tformat = right
\t\t}

'''.replace("\n", "\r\n").encode("cp1251")
        gui = gui.replace(anchor, addition + anchor)
    for name, x, width in (("building_maintenance_desc", 590, 160), ("naval_base_expense", 760, 70)):
        pattern = rb'(name = "' + name.encode("ascii") + rb'"\r?\n)(.*?)(\r?\n\t\t})'
        def move_field(match: re.Match[bytes]) -> bytes:
            content = re.sub(rb'position = \{ x = \d+ y = \d+ }', f"position = {{ x = {x} y = 432 }}".encode("ascii"), match[2])
            content = re.sub(rb'maxWidth = \d+', f"maxWidth = {width}".encode("ascii"), content)
            content = re.sub(rb'maxHeight = \d+', b"maxHeight = 14", content)
            return match[1] + content + match[3]
        gui, count = re.subn(pattern, move_field, gui, flags=re.S)
        if count != 1:
            raise ValueError(f"Не найдено единственное поле {name}")
    result[gui_path] = gui
    return result


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--write", action="store_true", help="записать файлы; без флага только проверить")
    args = parser.parse_args()
    result = output_files()
    if args.write:
        for path, data in result.items():
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(data)
    mismatches = [str(path.relative_to(ROOT)) for path, data in result.items() if not path.exists() or path.read_bytes() != data]
    if mismatches:
        raise SystemExit("Требуется подготовка: " + ", ".join(mismatches))
    print(f"Проверены профиль и локализация: {len(result)} файлов, {len(TEXT)} новых строк.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

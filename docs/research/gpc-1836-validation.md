# Проверка национальных провинций на старте 1836 года

Проверка 15–16 ноября 2026 года подтвердила выдачу **35 новых корок Пруссии и
21 Австрии** после точечного получения `nationalism_n_imperialism`. До получения
технологии кандидатов и новых корок не было. Отдельный проход на неизменной дате
дал строгое совпадение всех 56 пар без пропусков, лишних добавлений или удалений.

Это дополнительная проверка исходного старта, а не замена остальных SP/MP
сценариев [основного плана](../plans/great-power-culture-cores.md).
Контракты диагностики описаны в [плане debug API](../plans/gpc-debug-api.md) и
[README модуля](../../tools/creatinecore/gpc/README.md).

## Сборка и изоляция

Victoria II: Heart of Darkness 3.04 x86, CreatineCore 6.43. Проверенные хеши:

| Объект | SHA-256 |
|---|---|
| `v2game.exe` | `62d48c204364dd706584777c2e2b3c7ab3c5f1dd0170872554943575d53d6648` |
| Производственная GPC DLL | `f7ac480679a5008b8be3add48f385e1cf9e706e805d07d1235647dd639cba32b` |
| `tools/creatinecore/gpc/runtime.cpp` | `67131e2986bdb76adb2cd2de9e6a67b974e2d581d592a90ead804da469fc3a65` |

Хеш DLL в `.work/gpc-build` и в запущенной тестовой копии совпал. Полный набор
хешей исходников, включая headers, зафиксирован в `.work/gpc-build/manifest.json`;
один Git HEAD не используется как доказательство тождественности исходников.

Игровая копия: `.work/gpc-acceptance/.work/gpc-probe2/game`, собственный userdata —
соседний каталог `userdata`. Исходные пользовательские сейвы не использовались
как цели записи. Новая партия создана через GUI за PRU на первом bookmark
`1836.1.1`; после этого операции выполнялись существующим CLI.

Только для тестового процесса включён `VIC2_GPC_TEST_ALWAYS=1`. В сочетании с
`VIC2_TEST_SESSION` он задаёт порог `1000000`, что видно в журнале. Настройка
`GREAT_POWER_CULTURE_CORES_MTTH_MONTHS=60` осталась прежней. Это ускоряет один
глобальный процесс с прежними допустимыми днями; отдельные процессы или броски
по числу стран не создаются. Производственный MTTH этим испытанием не заменяется.

## Исходные условия и технологический фильтр

В [истории Пруссии](../../history/countries/PRU%20-%20Prussia.txt) основная культура
`north_german`, признанная — `south_german`. У
[Австрии](../../history/countries/AUS%20-%20Austria.txt) основная культура
`south_german`, признанных в исходном состоянии нет. Эти значения сохранились
во всех шести проверенных сейвах.

В [culture_tech.txt](../../technologies/culture_tech.txt) технология
`nationalism_n_imperialism` существует в базе, но имеет `year=1850`. В обычном
исследовании она недоступна до 1850 года; на старте 1836 её нет у обеих стран.
Ускоренный шанс GPC не обходит технологический фильтр. До искусственной выдачи
ни один из восьми квалифицированных GP не был eligible.

GP заданы независимому oracle по настоящему нативному списку:
`ENG RUS FRA PRU AUS USA SPA TUR`. После fixture eligible стали только AUS и PRU.

Для точечного положительного контроля использовано
[событие 3997210](../../tools/vic2test/fixtures/vic2test/events/gpc_1836.txt) и его
[отдельная русская локализация](../../tools/vic2test/fixtures/vic2test/localisation/gpc_1836.csv).
Оно установлено только в тестовую копию. Выбранная опция выполняет по одному
`activate_technology = nationalism_n_imperialism` в scopes PRU и AUS.
Ни корок, ни денег, ни пользовательских флагов fixture не выдаёт.

Сравнение всех technology-полей между `gpc36_noNI.v2` и `gpc36_pre.v2` показало
ровно одно изменение у каждой страны: появился `nationalism_n_imperialism={1 0.000}`.
Это также квалифицирует прямую выдачу данной технологии раньше её исследовательского
года через штатный script effect. Основные и признанные культуры не менялись.

## Периодический проход с движением времени

| Сейв | Дата | NI у PRU/AUS | Результат |
|---|---|---|---|
| `gpc36_start.v2` | 1836.1.1 | Отсутствует | Oracle: 0 кандидатов |
| `gpc36_noNI.v2` | 1836.1.4 | Отсутствует | Новых корок: 0; preview: 0 |
| `gpc36_pre.v2` | 1836.1.4 | Уровень 1 | Oracle и native preview: 56 кандидатов |
| `gpc36_post.v2` | 1836.1.6 | Уровень 1 | Добавлены ровно 56 ожидаемых пар; preview: 0 |

В [журнале периода 1836 года](../../.work/gpc-artifacts/sp-1836-runtime.log):

- Проходы 1 и 2 без NI: по 0 добавлений.
- Проход 3, день расписания 670145: 56 добавлений за 218 мс,
  `rng=1300821048`, `cores=9848C8A0`, `state=2ABE11A3`.
- Следующий ускоренный проход 4: 0 добавлений, `cores=9848C8A0` сохранился,
  `rng=854878815`. Новое окно уведомления не появилось.

Все 35 добавлений PRU соответствовали единственной крупнейшей культуре
`north_german`, все 21 добавление AUS — `south_german`. В каждой паре юридический
владелец отличался от получателя. Например, PRU получила 533 Heligoland у ENG,
а AUS — 409 Strasbourg и 410 Colmar у FRA, 594 Sigmaringen у PRU и 599 Munich у BAV.
Признанная `south_german` у Пруссии не породила южногерманских добавлений PRU.

Во всех 56 целевых провинциях во всех проверенных сейвах `controller==owner`;
контроля REB нет. Таким образом, существующие события для захваченных повстанцами
земель не объясняют проверенное множество новых корок.

### Полный мировой delta и фоновые удаления

Строгий `check_delta.py` сравнивает все корки мира, включая изменения старых
событий. Поэтому результаты проходов с движением дней **не объявляются полностью
успешными мировыми delta**:

| Сравнение | Ожидалось / добавлено | Пропуски / лишние | Удалено | `ok` |
|---|---:|---:|---:|---|
| start → noNI | 0 / 0 | 0 / 0 | 10 | false |
| pre → post | 56 / 56 | 0 / 0 | 1 | false |

В отрицательном интервале исчезли `(232, MAN)` и `(241…249, NEN)`. Все эти
провинции принадлежат USA, крупнейшая культура — yankee. Существующее событие
151058 в [after_union.txt](../../events/after_union.txt) удаляет именно MAN/NEN
из корок земель USA; оно допускается при существующей USA с primary yankee и
`ai=yes` в scope события. В положительном интервале исчезла `(945, PLC)`:
владелец RUS, крупнейшая культура byelorussian. Событие 148170 в
[removing_cores.txt](../../events/removing_cores.txt) удаляет PLC при подходящих
условиях вне polish/lithuanian. Оба файла в основной и тестовой копиях идентичны.

Это скриптовые соответствия наблюдаемого фона. Прямой записи dispatch этих ID
в журнале не найдено, поэтому конкретный вызов не выдаётся за доказанный.
Отчёты с `ok=false` сохранены без редактирования и без исключения этих 11 удалений
из строгого сравнения. Пропавших или новых ID провинций, оставшихся кандидатов
и неожиданных новых корок ни в одном сравнении нет.

## Отдельный проход на паузе: строгий exact delta

Для устранения движения времени выполнен второй положительный контроль:
`gpc36_dbgpre.v2` → `culture-cores run` → `gpc36_dbgpost.v2`.
Оба сейва имеют дату **1836.1.5**. Сначала был загружен подготовленный сейв до
выдачи и завершён следующий непроходной день для подтверждения дневной фазы.

[Независимый oracle](../../.work/gpc-artifacts/oracle1836paused.json) предсказал
56 пар: PRU 35, AUS 21. [Строгий delta](../../.work/gpc-artifacts/delta1836paused.json)
с `--require-no-remaining` дал:

```text
ok=true
expected=56 additions=56
missing=0 unexpected=0 removed=0
missing_provinces=0 additional_provinces=0 remaining_candidates=0
```

Независимо декодированы сохранённые глобальные flags; в каждом сейве 193 активных
служебных флага. Для каждого бита есть ровно одна активная альтернатива 0/1,
`ready` присутствует, `writing` отсутствует. Checksum пересчитан FNV-1a по пяти
little-endian uint32 и совпал с сохранённым значением:

| Поле | До run | После run |
|---|---:|---:|
| schema | 1 | 1 |
| seed | 2005878528 | 2005878528 |
| rng | 2580576341 | 2580576341 |
| last_day | 670144 | 670144 |
| pass_sequence | 2 | 3 |
| checksum | `DE5D7E5A` | `2E89BD6B` |

Следовательно, принудительный проход увеличил только номер прохода и связанные
контрольные биты; RNG, seed и дата расписания сохранились. Повторный preview дал 0.
В этих двух сейвах также сохранились primary/accepted, NI и отсутствие REB.

## Уведомление, кнопка и повторная загрузка

Главный агент проверил реальный GUI за PRU на 6 января 1836 года:
[окно уведомления](../../.work/gpc-acceptance/.work/gpc1836-notice.png) было одно,
заголовок — «Расширение национальных притязаний», кнопка — «Принято». Текст:

> Новые территории, где наша основная культура является одной из наиболее
> многочисленных, получили статус национальных провинций. Наши притязания уже
> вступили в силу.

После разрешённого Python-нажатия кнопка закрыла окно:
[снимок после принятия](../../.work/gpc-acceptance/.work/gpc1836-accepted.png).
Проверка использовала координаты `(681,599)` с поправкой ввода `(0,-18)` именно
для этого окна; эти координаты не являются универсальным интерфейсом запуска.
До и после нажатия неизменны `date_raw=59883480`, `pass=3`, `rng=1300821048`,
`core_digest=2554906784`, `state_digest=717099427`. Дополнительный проход после
двух дней дал 0 добавлений и `event_windows=0`.

После повторной загрузки `gpc36_post.v2` preview остался пустым; восстановились
`pass=3`, `rng=1300821048`. После одного непроходного дня завершённая фаза имела
`date_raw=59883504`, `state_digest=823599400`. Нулевой кэш `core_digest` до первого
нового полного прохода после загрузки не трактуется как потеря корок: read-only
preview вычислил фактический digest `2554906784` и 0 кандидатов.

Затем новый bridge прошёл цикл загрузок 1836 → 1931 → 1841 → 1836 в одном
PID 12536. [Отчёт переходов](../../.work/gpc-artifacts/sp-return-qualification-02.json)
подтверждает возвраты в меню и загрузки с тем же PID, успешные дневные барьеры,
preview 0 после загруженного результата 1836 и отдельный paused run на исходном
состоянии до выдачи. Это проверка переходов между кампаниями, без закрытия процесса
и без использования старых указателей как подтверждения новой кампании.

## Воспроизведение

Нужна новая изолированная среда с указанными проверенными DLL и нативным мостом
по [руководству CLI](../../tools/vic2test/README.md). Установить событие и CSV
fixture только в её `game/events` и `game/localisation`, до запуска процесса.
Для GUI использовать `tools/gui_check.py` с `--environment` и PID этой среды;
экран 3200×2000, масштаб 200%, ввод прекращается при потере целевого окна.

Ниже `$testEnvironment` — уже подготовленная новая копия, а не пользовательская
игра. `VIC2_GPC_TEST_ALWAYS=1` передаётся только запуску тестового процесса.

```powershell
$testEnvironment = '.work/gpc-1836-reproduction'
$previousGpcChance = $env:VIC2_GPC_TEST_ALWAYS
try {
    $env:VIC2_GPC_TEST_ALWAYS = '1'
    python tools/vic2test/cli.py start --environment $testEnvironment
} finally {
    $env:VIC2_GPC_TEST_ALWAYS = $previousGpcChance
}
# Один GUI-старт: bookmark 1836.1.1, страна PRU, начало партии.
python tools/vic2test/cli.py pause --environment $testEnvironment
# До первого OnDay обычный save ждёт GPC-барьер, которого ещё нет.
# Однократный исходный снимок создаётся уже квалифицированным native writer
# при подтверждённой паузе, без запуска планировщика и с новым именем файла.
@'
import json, sys
from pathlib import Path
sys.path.insert(0, str(Path('tools/vic2test').resolve()))
import cli
environment = Path(sys.argv[1]).resolve()
metadata = json.loads((environment / 'environment.json').read_text(encoding='utf-8'))
assert not (Path(metadata['userdata']) / 'save games/gpc36_start.v2').exists()
result = cli.request(environment, 'save-native gpc36_start', 180)
assert result.get('ok'), result
'@ | python -B - $testEnvironment
python tools/vic2test/cli.py advance --environment $testEnvironment --days 3 --auto-events
python tools/vic2test/cli.py culture-cores preview --environment $testEnvironment --name negative1836
python tools/vic2test/cli.py save --environment $testEnvironment --name gpc36_noNI
python tools/vic2test/cli.py event --environment $testEnvironment --tag PRU --id 3997210 --option 0
python tools/vic2test/cli.py culture-cores preview --environment $testEnvironment --name positive1836
python tools/vic2test/cli.py save --environment $testEnvironment --name gpc36_pre
python tools/vic2test/cli.py advance --environment $testEnvironment --days 2 --auto-events
python tools/vic2test/cli.py save --environment $testEnvironment --name gpc36_post
python tools/vic2test/cli.py culture-cores preview --environment $testEnvironment --name repeat1836
```

Если дневной callback ещё не завершён, дождаться `culture-cores status` с
`busy=0`, `has_schedule=1` и обеими датами, равными остановленной дате мира.
При неизвестном исходе команды повтор не выполняется. Существующие имена сейвов
и JSON не перезаписываются; для нового прогона используется новая среда.

Для точного paused-контроля вернуться в меню через `return-menu`, загрузить
`gpc36_pre.v2` через `load`, завершить один день до 1836.1.5 и сохранить
`gpc36_dbgpre`. Выполнить `culture-cores run --name paused1836run`, сохранить
`gpc36_dbgpost` и проверить повторный preview. Автоматическое ускорение здесь
не является причиной выдачи: run выполняется на неизменной дате и не делает RNG draw.

Независимая проверка выполнялась инструментами основного дерева:

```powershell
$saveDirectory = Join-Path $testEnvironment 'userdata/save games'
$gameDirectory = Join-Path $testEnvironment 'game'
python tools/creatinecore/gpc/save_oracle.py --save "$saveDirectory/gpc36_start.v2" --game-root $gameDirectory --gp ENG RUS FRA PRU AUS USA SPA TUR --output .work/gpc-artifacts/oracle1836negative.json
python tools/creatinecore/gpc/check_delta.py --before-oracle .work/gpc-artifacts/oracle1836negative.json --after "$saveDirectory/gpc36_noNI.v2" --game-root $gameDirectory --gp ENG RUS FRA PRU AUS USA SPA TUR --output .work/gpc-artifacts/delta1836negative.json --require-no-remaining
python tools/creatinecore/gpc/save_oracle.py --save "$saveDirectory/gpc36_pre.v2" --game-root $gameDirectory --gp ENG RUS FRA PRU AUS USA SPA TUR --output .work/gpc-artifacts/oracle1836positive.json
python tools/creatinecore/gpc/check_delta.py --before-oracle .work/gpc-artifacts/oracle1836positive.json --after "$saveDirectory/gpc36_post.v2" --game-root $gameDirectory --gp ENG RUS FRA PRU AUS USA SPA TUR --output .work/gpc-artifacts/delta1836positive.json --require-no-remaining
python tools/creatinecore/gpc/save_oracle.py --save "$saveDirectory/gpc36_dbgpre.v2" --game-root $gameDirectory --gp ENG RUS FRA PRU AUS USA SPA TUR --output .work/gpc-artifacts/oracle1836paused.json
python tools/creatinecore/gpc/check_delta.py --before-oracle .work/gpc-artifacts/oracle1836paused.json --after "$saveDirectory/gpc36_dbgpost.v2" --game-root $gameDirectory --gp ENG RUS FRA PRU AUS USA SPA TUR --output .work/gpc-artifacts/delta1836paused.json --require-no-remaining
```

Эти команды создают новые отчёты. Для повторного воспроизведения выбрать новые
имена output, не заменять уже сохранённые доказательства. При движении времени
мировой delta может снова обнаружить фоновую механику; это фиксируется отдельно.

## Хеши сохранённых доказательств

SHA-256 сейвов совпали до и после независимого чтения:

| Сейв | SHA-256 |
|---|---|
| `gpc36_start.v2` | `9e39994108f046dc3f58992435ef990ebf5953ae89df9f4f2c5f0af511105bbb` |
| `gpc36_noNI.v2` | `259f9bfef3a361421587c4d26094b8991e9e2d5663270d433ceab24c084fe8e5` |
| `gpc36_pre.v2` | `2a1a6807fd496fefa370cdbc8070b8fdbd1e2c83ec2616e35830bbb211abd57e` |
| `gpc36_post.v2` | `4773075f22ae2a5061083c7358842a81559e6800a437a1ca54971b01c48ac9dc` |
| `gpc36_dbgpre.v2` | `38b8bcaa579b644b95277a40f44baf851f35fe01a5096b0b391dd1c06f0a82d7` |
| `gpc36_dbgpost.v2` | `b5f52e87f6f2e9721351b8a90c598972652c3daa584ea38dd47fc5a40a82cbe7` |

Отчёты находятся в `.work/gpc-artifacts`, вне Git:

| Отчёт | SHA-256 |
|---|---|
| `oracle1836negative.json` | `5ecfd002ab36292b588cd38a0fde84230123ec435a7515d60c9b543680e88a7f` |
| `oracle1836positive.json` | `7173c753cec948376eb1e02623f56fe64d447e31e94b770fd370dac6b38cb330` |
| `oracle1836paused.json` | `9e5ef796e9737c0b5af241705134d20e931e59453c21866a9c6be234818bea71` |
| `delta1836negative.json` | `5d9bf6cd2df8c3b8ba473a8dc27ef4f9664147d6e9383c0d5ce630035e33eaae` |
| `delta1836positive.json` | `46579e272f55530207beaa3d6487a3921a8bbc0efa9305854537839b4eebbe3f` |
| `delta1836paused.json` | `702ad526bd0a14187b3c8f09010343a87689a64196c5a8a8ce0999256444f500` |

Обычное срабатывание и принудительный paused-контроль проверяют одинаковую
реализацию выдачи. Проверенный результат относится к одному глобальному проходу,
основным культурам и квалифицированному технологическому фильтру; он не изменяет
штатный год технологии и не включает fixture в обычную игру.

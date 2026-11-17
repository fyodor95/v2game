# Проверки культур и государственных религий

Инструменты исполнения [плана](../../docs/plans/culture-religion.md).
Все игровые запуски выполняет главный агент последовательно, в изолированной
бесшумной среде. Исходные пользовательские сейвы не изменяются.

```powershell
python -m unittest discover -s tools/culture_religion -p 'test_*.py'
python tools/culture_religion/generate.py prepare --destination .work/culture-religion/generation
python tools/culture_religion/generate.py run --destination .work/culture-religion/generation --generator <проверенный-run_program.exe>
python tools/culture_religion/validate.py --root .work/culture-religion/generation/game --output .work/culture-religion/validation.json
python tools/culture_religion/generate.py install --destination .work/culture-religion/generation
```

`generate` дважды запускает настоящий autogen, сравнивает все результаты и перед
установкой сверяет исходные файлы. Изменение любого входа другим агентом блокирует
установку до первой записи. Файлы, созданные генератором, также проверяются на
коллизии с рабочим деревом.

`validate.py` самостоятельно восстанавливает пары из положительных POP1836,
основные страны и статические знаменатели. Проверяет религию провинции,
исходную/текущую религию обычной страны, цивилизованность владельца, повторный
полный guard, основную культуру/религию Д-тэга и сохранность распространения.
Это структурная проверка; исполнение движком квалифицируется отдельно.

```powershell
python tools/culture_religion/engine.py prepare --game-root .work/culture-religion/engine/game --source-save .work/culture-religion/engine/userdata/'save games'/chr_fresh.v2 --output .work/culture-religion/fixtures
python tools/culture_religion/engine.py run --environment .work/culture-religion/engine --fixtures .work/culture-religion/fixtures --output .work/culture-religion/engine-result.json
python tools/culture_religion/natural.py run --environment .work/culture-religion/engine --fixtures .work/culture-religion/fixtures --output .work/culture-religion/natural-result
```

Перед `run` установить сгенерированные данные, проверенные DLL, тестовый мост и
файлы подготовленной фикстуры согласно её манифесту. Условия событий проверяются
настоящим движком, эффекты — через независимый разбор сохранений. Адаптер добавляет
отрицательные проверки религии провинции/страны и цивилизованности, фиксирует
основную культуру и государственную религию Д-тэгов после выполнения.

`snapshot.py` только читает память подтверждённого собственного процесса. Снимок
не атомарный: применять на подтверждённой паузе, сверяя дату до и после. Поддержан
только квалифицированный EXE3.04. Ни команд исполнения, ни произвольных записей
в память модуль не предоставляет.

При холодной загрузке дождаться `Total loadtime after app init` в собственном
`userdata/logs/time.log` до первой команды: загрузка ресурсов может превысить
180-секундный срок нативного обработчика. После его тайм-аута требуется новый
процесс; повтор команды в повреждённой сессии запрещён.

Дополнительные сценарии и настоящее решение Д-страны:

```powershell
python tools/culture_religion/lifecycle.py prepare --game-root .work/culture-religion/engine/game --source-save '.work/culture-religion/engine/userdata/save games/chr_fresh.v2' --output .work/culture-religion/lifecycle-fixtures
python tools/culture_religion/lifecycle.py run --environment .work/culture-religion/engine --fixtures .work/culture-religion/lifecycle-fixtures --output .work/culture-religion/lifecycle-result.json
python tools/culture_religion/decision_fixture.py prepare --game-root .work/culture-religion/engine/game --source-save '.work/culture-religion/engine/userdata/save games/chr_fresh.v2' --output .work/culture-religion/decision-fixtures
python tools/culture_religion/decision_acceptance.py --environment .work/culture-religion/engine --output .work/culture-religion/decision-result.json --gui
```

Перед запуском установить файлы нужного пакета по его manifest; исходные сейвы
не подменять. `lifecycle` проверяет PHI/PAN, занятый Д-тэг, настоящий release6119,
сброс145000, повторное использование и `british_decision`. Последний создаёт
новую копию нативного сейва с заменой только заголовка `player`, чтобы принять
решение за образовавшуюся страну. `decision_acceptance` ожидает отдельную
`cr_d_primary.v2` и проверяет взвешенный выбор при противоположных религиях
столицы, страны и мира. `native_acceptance.py --winner catholic` с `cr_tie.v2`
проверяет обе разновидности ничьей; обычный прогон — отсутствие POP и повтор.

```powershell
python tools/culture_religion/culture_gui.py --environment .work/culture-religion/engine --save cr36_start.v2 --output .work/culture-religion/culture-map --dry-run
```

Без `--dry-run` этот инструмент запускает собственную игру, нажимает штатную
кнопку культурной карты, выбирает Африку и делает снимки. `ok` подтверждает
завершение ввода; изображения нужно осмотреть. Координаты квалифицированы для
клиента1366×768. Helper учитывает DPI192 на экране3200×2000, полностью помещает
собственное окно в рабочую область и проверяет foreground, PID и перекрытие
через `WindowFromPoint` непосредственно перед вводом. При чужом перекрытии или
потере фокуса ввод прекращается.

`registry.capture` сопоставляет все страновые и провинциальные ID из `game/events`
с реестрами собственного подтверждённого процесса после `Loading Events`.
Он не выполняет события и помогает обнаружить ошибки загрузки, которые
структурный AST-парсер не замечает. `snapshot.capture` отдельно читает культуру
и религию на паузе.

Для сети подготовить две среды существующим `tools/vic2test/cli.py prepare`,
установить одинаковые production DLL и обновлённый bridge, затем подготовить
пакет:

```powershell
python tools/culture_religion/mp_acceptance.py prepare --game-root D:/v2game --source-save '.work/culture-religion/engine/userdata/save games/chr_fresh.v2' --output .work/culture-religion/mp-package
python tools/culture_religion/mp_acceptance.py run --host .work/culture-religion/mp-host --client .work/culture-religion/mp-client --package .work/culture-religion/mp-package --output .work/culture-religion/mp-result.json --visible
```

Установить четыре файла пакета в обе среды согласно manifest, предварительно
проверив хеши. Передавать первичный PRU-сейв, а не уже изменённый `cr_mp_base`:
пакет самостоятельно создаёт D50. В обеих приватных копиях нужны
`ENABLE_LOG=1`, `ENABLE_OOS_LOG=1` и включённый GPC. Для гарантированного
прохода GPC в коротком тесте передать дочерним процессам
`VIC2_GPC_TEST_ALWAYS=1` и восстановить переменную после запуска; gate требует
также `VIC2_TEST_SESSION`, который устанавливает CLI. Основной профиль не менять.

MP-runner использует только сетевые изменяющие команды и read-only наблюдения,
проверяет оба процесса, сохраняет партию и журналы. Требует одинаковые корки,
primary/religion, полный парный SYNC и хотя бы один одинаковый GPC-проход.
`--visible` сохраняет окна, но не добавляет ввод мышью/клавиатурой. Повторять
команду с неизвестным исходом запрещено. Полные результаты и ограничения —
в [отчёте приёмки](../../docs/research/culture-religion.md).

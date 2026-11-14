# CreatineCore в этой сборке

Используется CreatineCore 6.43, upstream `7d9f9df9b356096cde475ebd550fbb0102fa1042`, с локальными исправлениями. Русская сборка игры и EXE версии 3.04 сохраняются. Состав установленных DLL и хеши записаны в `z_CreatineCore/install-manifest.json`.

Включены расширенные условия и эффекты событий/решений, исправления отложенного FROM, флагов провинций, модификаторов POP, технологий, правил приоритета фабрик, пополнения войск, союзных транспортов, размещения поддержки во второй линии, музыка и цвета вассалов. Точный явный профиль хранится в `tools/creatinecore-profile/creatinecore_settings.ini`.

Перенаправление урона во вторую линию отложено по согласованию с пользователем: upstream принудительно отключает его из-за неверного адреса функции. Одного выставления флага недостаточно. Многопоточные оптимизации и квантование POP не включены.

Цены экспоненциальные; расширение потолка выключено. Фабричный резерв 3000 на уровень увеличивает запас денег и откладывает выплаты владельцам, но не является универсальной защитой от переполнений. Поддержка содержания зданий включена, реальные ставки отложены. Глобальная прибавка доходов POP равна нулю; возможность скриптового `pop_income_modifier` доступна.

## Сборка и установка

```powershell
python tools/creatinecore/build.py
python tools/creatinecore/test_config_alias.py
python tools/creatinecore/validate_native_sites.py
python tools/creatinecore-profile/prepare.py
python tools/creatinecore/install.py --destination D:/v2game
```

Требуются установленный MSVC 2026 Win32, Git и Python 3.12. Для первой загрузки исходников нужен доступ к GitHub. В среде Codex MSBuild может требовать разрешённого запуска вне песочницы. Установщик проверяет хеши EXE и DLL, сохраняет прежние файлы в `.work/install-backups` и не устанавливает отладочный канал в обычную игру.

Исходный план и пользовательские дополнения находятся в `docs/plans`. Полный откат сборки включает согласованные коммиты игровых данных и DLL: удаление одного загрузчика не отменяет изменения defines, переводов или добавленный расширенный синтаксис скриптов.

## Тестирование из командной строки

```powershell
tools/vic2test/build.cmd
python tools/vic2test/cli.py prepare --destination .work/test-run --save "D:/Documents/Paradox Interactive/Victoria II/save games/isolation_baseline.v2"
```

В тестовую копию устанавливаются ядро, профиль и `tools/vic2test/.build/vic2_test_bridge.dll` в `game/z_CreatineCore/dll_Submods`. Обычная игра не получает этот тестовый плагин. Затем:

```powershell
python tools/creatinecore/install.py --destination .work/test-run/game
Copy-Item tools/vic2test/.build/vic2_test_bridge.dll .work/test-run/game/z_CreatineCore/dll_Submods/
Copy-Item tools/vic2test/fixtures/* .work/test-run/game/mod/ -Recurse -Force
```

Для сценариев из `fixtures` добавлять к `start` параметр `--mod vic2test.mod`.

```powershell
python tools/vic2test/cli.py start --environment .work/test-run --save isolation_baseline.v2 --hidden
python tools/vic2test/cli.py status --environment .work/test-run
python tools/vic2test/cli.py decision check --environment .work/test-run --tag HAW --id decision_name
python tools/vic2test/cli.py event --environment .work/test-run --tag HAW --id 123 --option 0
python tools/vic2test/cli.py advance --environment .work/test-run --days 30
python tools/vic2test/cli.py save --environment .work/test-run --name new_checkpoint
python tools/vic2test/cli.py stop --environment .work/test-run
```

Команды загрузки, выбора опции, решений, сохранения и хода времени исполняются внутри настоящего движка без синтетических кликов и клавиатуры. Опции нумеруются с нуля. Выполнение решения и выбранной опции поддерживается для страны игрока; `event --province <ID> --id <event> --option <N>` выполняет провинциальное событие в принадлежащей игроку провинции. Провинциальный путь использует настоящий `CEvent::run`: у консольного обработчика обнаружена неверная область выполнения. Произвольные снимки всех полей POP не реализованы; условия флагов и модификаторов проверяются нативными условиями тестовых решений. Для другой исходной кампании или изменённых скриптов запускать новый процесс. После неопределённого результата или нативного исключения сессию перезапускать.

Пример воспроизводимого сценария и русский тестовый мод находятся в `tools/vic2test/fixtures`. Сценарий проверяет однократный immediate, выбранную опцию, точные изменения казны и запрет повторного решения. Статическая команда `check` проверяет структуру и дубли ID; она не вычисляет условия движка.

Обычный `advance` останавливается при требующем выбора событии. Для автономного длительного регрессионного прогона можно явно добавить `--auto-events`: движок выбирает первые опции фоновых событий только на время этого прогона и возвращает их количество. Такой прогон проверяет работоспособность, а не повторяет решения человека в исходной кампании. Явно вызванное событие, ожидающее выбора теста, этим режимом не перезаписывается.

## GUI

Пользователь разрешил Python для снимков, мыши и клавиатуры. `tools/gui_check.py --pid <PID> --output .work/screen.png` работает только с окном Victoria II. `--show` показывает скрытое окно; `--click X Y` использует физические координаты клиентской области, `--key` — код клавиши Windows. При отсутствии фокуса ввод отменяется. Скрипт учитывает DPI; экран 3200×2000 при масштабе 200% не трактуется как 1600×1000.

Исходные пользовательские сейвы не изменяются. Тестовые сейвы, логи, снимки и сборки остаются в `.work` вне Git. Результаты фактически выполненных проверок записываются в `docs/research/validation.json`.

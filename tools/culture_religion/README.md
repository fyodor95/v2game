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

Итоговая игровая квалификация и точные результаты будут записаны после завершения
испытаний; автономные тесты не являются подтверждением SP/MP или GUI.

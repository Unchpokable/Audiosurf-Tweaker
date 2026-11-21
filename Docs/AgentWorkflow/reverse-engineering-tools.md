# Инструменты реверса и исследовательские данные

## Выбор инструмента

**CgrPy** — граф каналов/CGR, готовые query-команды, массовые проверки DLL и
воспроизводимые числа. **IDA Pro 9.3 + сторонний mrexodia ida-pro-mcp**, сервер
`idalib`, — нативный код, decompile/xrefs, чтение данных и накопление типов,
имён, комментариев в базе. **Ghidra** — резервный headless-путь/экспорт.
Инструменты дополняют друг друга. Отсутствие IDA допускает подходящий
резервный анализ, но не означает, что перенос обязательного IDA MCP завершён.

`Audiosurf_Steam/` — исследовательская копия; **существующие .i64 содержат
результаты крупных исследований**. Открывай и сохраняй их, не пересоздавай
как пустые. Эксперимент записи для приёмки делай в отдельной тестовой базе.
Закрывай `idb_close` с сохранением; точные аргументы сверяй со схемой сервера.
Имя функции IDA не доказывает принадлежность классу: ICF может склеить разные
методы в один адрес, у которого в базе только одно имя.

## CgrPy

Справочник — [Tools/CgrPy/README.md](../../Tools/CgrPy/README.md). Из его каталога:
PowerShell `.\cgrpy.cmd`, cmd `.\cgrpy`, Git Bash `./cgrpy`; из других мест —
полный путь. Launcher сам задаёт imports и UTF-8, uv ставит зависимости.
`AUDIOSURF_DIR` задаёт игру явно либо работает автопоиск; для исследований
предпочитай явную исследовательскую копию, чтобы не затронуть Steam-игру.

Начни вопрос о графе с `query find`, `dec`, `value`, `writers`, `switch`,
`parents`, `users`, прежде чем писать одноразовый парсер. Для DLL доступны
chtypes, vtdump, findvt, symbols, disasm; для геометрии — meshes.
Записывай точную команду, группу/индекс и результат в тематический журнал.

## Личная установка IDA MCP

Выбран **mrexodia/ida-pro-mcp**, не официальный Hex-Rays MCP. Обновление IDA
до 9.4 сейчас недоступно, поэтому менять сервер на Hex-Rays нельзя. Рабочая
установка должна находиться **вне кеша Claude**, под
`%LOCALAPPDATA%/AudiosurfAgentTools/ida-pro-mcp`.

Исходный проверяемый снимок: запись установщика revision
`c133c3853faa111a9b00ee615c013b720d0c4acd`; wrapper 0.1.0, Python-пакет 2.0.0,
Python 3.11, idapro 0.0.9. Это разные уровни версий. SHA-256 исходного `uv.lock`:
`3CA9BA7FBF103CD3D0254D67A21D3EFAFE765E70F68A4E6DFD87D91E06C41B94`.
Зафиксированный manifest фактических файлов важнее одного имени каталога кеша.
Не совмещай перенос клиента с обновлением пакета или IDA.

Личный Codex config (реальный путь подставляется при установке):

```toml
[mcp_servers.idalib]
command = "<absolute-path-to-uv.exe>"
args = ["run", "--locked", "--no-sync", "--project", "<personal-tools>/ida-pro-mcp", "idalib-mcp", "--stdio"]
cwd = "<personal-tools>/ida-pro-mcp"
startup_timeout_sec = 60
tool_timeout_sec = 180
```

`--no-sync` предполагает предварительное `uv sync --locked` в личной установке.
Не копируй старую venv: она содержит привязки к кешу Claude. Используй Python
3.11 и существующую активацию idalib для IDA 9.3; лицензия и
`%APPDATA%/Hex-Rays/IDA Pro/ida-config.json` остаются личными данными машины.
Не публикуй их. При переносе сохраняются lock, provenance и профиль инструментов.

Сервер должен уметь открыть/закрыть базу, декомпилировать x86, показать xrefs,
читать get_int/read_struct и сохранять rename/comments/types. Исторические
«66 инструментов» и отсутствие py_eval в старом списке — не критерий
самостоятельно: проверяй текущий список и схемы. Не включай отсутствующие
расширенные инструменты ради совпадения числа.

Проверка включает выделенную тестовую базу, rename/comment/type, сохранение,
переоткрытие и проверку аннотаций, затем закрытие worker. Для чтения существующей
исследовательской базы сначала сохрани её резервную копию/отпечаток; не меняй
аннотации ради smoke-теста. Итоги установки и фактические ограничения записаны
в [приёмке миграции](../Internal/agents-migration.md).

По [официальной документации Codex](https://learn.chatgpt.com/docs/extend/mcp?surface=cli)
STDIO MCP настраивается через `config.toml`. Изменение конфигурации не
обновляет автоматически набор инструментов уже начатой сессии: после установки
проверь обнаружение `idalib` новой сессией клиента. Протокольный тест отдельно
отличай от вызова инструмента самой новой сессией.

## Ghidra

Переносимый набор сохраняется в
`%LOCALAPPDATA%/AudiosurfAgentTools/ghidra-scripts`, включая `ghidra_scripts/ExportAll.java`.
Запуск не должен зависеть от `.claude/skills`. Путь Ghidra и JAVA_HOME —
в [личном окружении](local-environment.example.md), переменные только процессу.

```powershell
$env:JAVA_HOME = '<installed-jdk>'
$env:GHIDRA_OUTPUT_DIR = '<output-directory>'
& '<ghidra>/support/analyzeHeadless.bat' '<project-directory>' ASChannels `
  -import '<research-binary.dll>' `
  -scriptPath "$env:LOCALAPPDATA/AudiosurfAgentTools/ghidra-scripts/ghidra_scripts" `
  -postScript ExportAll.java
```

ASChannels можно переиспользовать. Исторические выводы и ловушки кодировок —
в reversing-journal-lua через [карту](documentation-map.md).
Успешное копирование скриптов не выдавай за повторную проверку декомпилятора.

## Остальные зависимости

Native harness живёт в приватном сабмодуле `TweakerPlugin/harness` организации
(`update = none`); с доступом — `git submodule update --init TweakerPlugin/harness`.
UiStand и smoke_test — репозиторные стенды; команды в [development](development.md).
ilspycmd в старой работе был временно установлен в scratchpad: постоянную
установку не предполагаем. Ценные снимки и storyboard сохраняй обычными
файлами с привязкой к проверке, а не только облачной страницей Claude Artifact.

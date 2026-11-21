# Локальное окружение: шаблон

Скопируй при необходимости в `Docs/AgentWorkflow/local-environment.local.md`.
Личная копия исключена из Git и не импортируется автоматически как инструкции.
Храни только пути/версии и статус проверки, без токенов, лицензий и дампов бесед.

| Значение | Пример / что заполнить |
|---|---|
| Репозиторий | `<repo>` |
| Исследовательская игра | `<repo>/Audiosurf_Steam` |
| Рабочая Steam-игра | `<steam-library>/steamapps/common/Audiosurf` |
| Tweaker для ручных проверок | `<отдельная установка>` |
| AUDIOSURF_DIR для исследований | абсолютный путь **исследовательской** копии |
| IDA | `<Program Files>/IDA Professional 9.3` |
| Личный инструментарий | `%LOCALAPPDATA%/AudiosurfAgentTools` |
| IDA MCP | `<личный инструментарий>/ida-pro-mcp`, snapshot/lock/revision |
| Ghidra scripts | `<личный инструментарий>/ghidra-scripts` |
| Ghidra headless / JDK | локальные пути; JAVA_HOME задаётся процессу |
| MSVC/MSBuild/SDK/fxc/clang-format | обнаруженные пути, версии и архитектура |
| Harness | `<repo>/TweakerPlugin/harness` — приватный сабмодуль организации (`update = none`); инициализируется явно при наличии доступа |
| Результаты проверок | дата, passed/failed/unavailable, фактический бинарь |

Проверяй разделение Steam-игры и исследовательской копии перед любой командой
установки/патча. Базы IDA в исследовательской копии содержат ценные результаты.
Глобальные изменения PATH/JAVA_HOME не нужны; настрой переменные процесса.
MCP настраивается в личном `~/.codex/config.toml`; рабочее подключение не должно
ссылаться на кеш Claude. Подробности — [инструменты](reverse-engineering-tools.md).

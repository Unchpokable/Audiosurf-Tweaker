# Инструменты однократной миграции

Это обслуживающие скрипты перехода, не часть приложения/AgentKit/поставки.
Правила проекта — [AgentWorkflow](../../Docs/AgentWorkflow/README.md), результаты —
[agents-migration](../../Docs/Internal/agents-migration.md).

## Личные инструменты

`Install-LocalTools.ps1` переносит зафиксированный mrexodia IDA MCP из кеша
Claude в `%LOCALAPPDATA%/AudiosurfAgentTools`, создаёт новую venv через
`uv sync --locked --no-dev`, сохраняет Ghidra-экспортёры и настраивает личный
`~/.codex/config.toml`. Исходный lock проверяется по SHA-256; файлы сравниваются
при копировании. Venv и runtime workers не копируются. Старый сервер остаётся.

Также создаётся приватная резервная копия всех Markdown-файлов памяти Claude,
автоматический индекс заменяется ссылками на общий проект, тематические записи
явно помечаются историческими. JSONL не читаются. Перед записью Codex config
делается backup и проверяется TOML.

Скрипт **отказывается перезаписывать существующие каталоги установки или
существующий `mcp_servers.idalib`**. Это одноразовый bootstrap; для повторного
развёртывания сначала исследуй сохранённый provenance и текущее окружение,
не удаляй каталог автоматически ради повторного запуска.

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\Tools\AgentMigration\Install-LocalTools.ps1
```

ExecutionPolicy применяется только дочернему процессу, не системе. Запись
в личные каталоги требует разрешения среды выполнения. Скрипт параметризован;
перед использованием на другой машине проверь source paths и lock.
Резервные config/memory находятся в `migration-backup-<UTC>` внутри личного
каталога инструментов. Для отката восстанови выбранные файлы из backup,
сначала сохрани появившиеся после миграции изменения; автоматического
разрушительного rollback нет.

## Приёмка IDA MCP

Запускай `check_ida_mcp.py` установленным Python 3.11 из новой venv. Он читает
реальный раздел `mcp_servers.idalib`, делает STDIO handshake/tools-list,
а затем работает только с **копиями** DLL/баз в новом output-каталоге.

```powershell
& "$env:LOCALAPPDATA/AudiosurfAgentTools/ida-pro-mcp/.venv/Scripts/python.exe" `
  .\Tools\AgentMigration\check_ida_mcp.py `
  --config "$env:USERPROFILE/.codex/config.toml" `
  --output .\Temp\agent-migration\new-acceptance `
  --source-engine .\Audiosurf_Steam\engine
```

`--discover-only` проверяет handshake и необходимые инструменты без открытия
баз. Полная проверка читает таблицу указателей, сверяет с PE, сохраняет и
повторно читает rename/comment/type в копии HighPoly, декомпилирует x86/x64 и
закрывает собственные workers. Системная version.dll тоже копируется в fixture.
SHA-256 исходного HighPoly и его базы сравнивается до/после. Содержимое базы
и transcript остаются в gitignored `Temp`, а не публичном репозитории.

При ошибке смотри `server.log`/`transcript.json`. Не принимай простое получение
ответа за успех: важны итоговый acceptance.json и неизменность source hashes.
Старые import slots IDA может нормализовать; для сверки используются реальные
указатели таблицы, не IAT. Протокольная проверка не подменяет обнаружение
инструментов новой пользовательской сессией Codex после её перезапуска.

# Сборка, проверки и поставка

## Выбор цели

Команды выполняются от корня репозитория, если cwd не указан. Полный билд
определяют **скрипты Deploy**, не dotnet-only сборка `.sln`. Команды ниже
сверены со скриптами/пресетами; сама миграция документов не означает, что
полный билд приложения повторно прогнан.

| Цель | PowerShell | Git Bash |
|---|---|---|
| Полная release-поставка | `& .\Scripts\Deploy.ps1` | `./Scripts/Deploy.sh` |
| Полная отладочная поставка | `& .\Scripts\DeployDebug.ps1` | `./Scripts/DeployDebug.sh` |
| Локальный UI и связанные модели | `dotnet build TweakerUI/TweakerUI.csproj` | то же |
| Запуск UI, когда нужен проверке | `dotnet run --project TweakerUI/TweakerUI.csproj` | то же |
| Отдельная managed-модель | `dotnet build TweakerCore/TweakerCore.csproj` | то же, подставь нужный проект |

Deploy пересоздаёт `distr/`: перед запуском проверь отсутствие нужных локальных
материалов в этой папке. Release — Native AOT win-x64 UI и twllink;
DeployDebug — managed UI с CoreCLR и PDB, twllink остаётся AOT. LegacyDataConverter
в обоих случаях Release через полный MSBuild, найденный vswhere. Не называй
dotnet build всей `.sln` рабочей заменой: LegacyDataConverter требует полного MSBuild.
Локальные build/run, NUnit и UiStand используют managed-сборки.

`QuickPlayerCore` через `TweakerNative.targets` собирает обязательную x64 helper DLL
и переносит её вместе с лицензиями в выход потребителя, включая publish и тесты.
Первый CMake configure скачивает закреплённые TagLib 2.3.2/utfcpp и zlib 1.4;
дерево `TweakerNative/build/msbuild` не связано с x86 plugin/bridge targets.

Managed: .NET 10, UI — Windows TFM; LegacyDataConverter — net481. Нативная
сборка требует MSVC C++ tools, Windows SDK, CMake/Ninja; DirectX June 2010 SDK,
ImGui, Detours и LuaJIT — сабмодули; Quest3D SDK — приватный сабмодуль или
локальная копия (см. `Tools/ReleaseChannel/README.md`). LunaSVG/plutovg
подтягиваются FetchContent; первый configure под каждый генератор требует сети.
`fxc` ищется в SDK/dev-окружении; можно задать `TWEAKER_FXC`. SDK и бинарники
игры не добавляй в публичный Git. Редакторские tasks и старый GitHub workflow
не являются эталоном: workflow требует отдельного аудита.

## Native-пресеты и оболочки

В `TweakerPlugin/` окружение для Ninja нужно подготовить **в том же процессе
cmd** через `vcvarsall.bat amd64_x86`: host x64, target x86. Пресеты объявляют
toolset/architecture как external. Не используй `x86` вместо `amd64_x86`.
С локальным harness вспомогательная команда выглядит так:

```bat
@echo off
cd /d "<repo>\TweakerPlugin"
call harness\vcenv.bat amd64_x86 || exit /b 1
cmake --preset x86-release || exit /b 1
cmake --build --preset x86-release
```

Запиши это в временный `.cmd` с CRLF. Из PowerShell запускай `cmd /c <путь>`,
из Git Bash — `cmd //c <путь>`; не пытайся импортировать окружение batch в Bash.
Если harness не инициализирован (`git submodule update --init TweakerPlugin/harness`),
найди vcvarsall через Visual Studio Installer/vswhere и вызови его напрямую;
не делай `vcenv.bat` обязательной частью чистого клона. Пресеты `x86-debug`/`x86-release`;
`cmake --build --preset smoke` собирает исключённый из обычного build target `smoke_test`.

Если `x86-release` показывает `/Od`/`RTC1` или Debug в CMakeCache, имя каталога
не доказывает Release: повтори configure правильным пресетом/окружением.
Не смешивай developer Ninja-деревья с `build/msbuild` pre-build hooks UI.
ASBridge/InjectHelper обязательны при сборке UI; TweakerPlugin — best-effort,
поэтому успешный UI build сам по себе не проверяет DLL.

При AOT-ошибке `vswhere.exe not recognized` проверь
`NoDefaultCurrentDirectoryInExePath`. Временно сними переменную **только для
процесса проверки**, восстанови прежнее значение в finally; не меняй систему.
Из Bash можно `env -u NoDefaultCurrentDirectoryInExePath dotnet publish ...`.
Для cmd программ из cwd используй явный `.\`; для PowerShell CgrPy —
`.\cgrpy.cmd`, для Git Bash — `./cgrpy`.

## Матрица проверок

| Изменение | Проверка |
|---|---|
| Модели/протоколы .NET | NUnit соответствующего проекта: `TweakerCore.Tests`, `QuickPlayerCore.Tests`, `AudiosurfInterface.Tests`, `TweakerLuaLinker.Tests` |
| Формат скинов | Дополнительно путь конвертации из legacy-формата через обязательный LegacyDataConverter |
| Lua/engine/shim/поставляемые скрипты | Native harness: lua (ljtest, shimtest, arraytest, famtest), lifecycle, sched, remote и относящиеся HUD/forms/storage сюиты |
| sky_* | Профильные native harness сюиты (skyload/skylua/skyfill/config и др.), при изменении шейдера fxc и визуальная проверка |
| Оверлей UI | smoke_test снимки до/после, pixel compare и просмотр изменённых областей |
| Avalonia UI | UiStand с настоящими стилями; подходящие режимы, снимки/сравнение, transitions; для памяти mem/tabs |
| API/AgentKit/публичные мануалы | ljtest, TweakerLuaLinker.Tests (DocsTests, AgentKitTests, ShippedProjectsTests по теме) |
| Игра end-to-end | Ручная проверка владельца; в отчёте отделить от офлайн-результатов |

Пример NUnit: `dotnet test TweakerCore.Tests/TweakerCore.Tests.csproj`;
отбор — `--filter "FullyQualifiedName~ClassName.MethodName"`. Не запускай весь
набор для косметического изменения; расширяй проверки при новых рисках/ошибках.
Twllink-тесты с minilua могут пропускаться без собранного LuaJIT; skipped не
считать успехом исполнения Lua.

**`TweakerPlugin/harness/` — приватный сабмодуль организации** (`update = none`;
план — [repo-layout-plan](../Internal/repo-layout-plan.md)). У внешнего клона он
пропускается; с доступом — `git submodule update --init TweakerPlugin/harness`.
Не публикуй его и не меняй подключение сам. Локальный `run.bat` запускает suites,
`run.bat build` добавляет инструменты. Пробы живут здесь же (`scripts/dev/`,
`projects/dev/`), а не в репозитории приложения. Lua-сюитам может требоваться
`build/x86-release/luajit.lib`: сначала собери плагин. Отмечай локальные
изменения harness, которых не будет в основном diff. Старый случай с
`FAILED: lua/ljtest lua/shimtest` и `ALL CHECKS PASSED` — диагностическая история,
не разрешение игнорировать FAILED: проверь вывод/exit codes конкретной сюиты.

Снимочный бинарь плагина:
`TweakerPlugin/build/x86-release/bin/tweaker_ui_smoke.exe --shots <dir>` и
`--compare <before> <after> --diff <diff-dir>`. Без аргументов он открывает
интерактивное окно; для агента обычно нужны снимочные режимы.
UiStand: сначала собери UI, затем
`dotnet build Tools/UiStand/UiStand.csproj`; запускай `UiStand.exe`, **не**
`dotnet UiStand.dll` (AppPaths считается от процесса). Подробности, реальные
fixtures и ограничения headless — [README](../../Tools/UiStand/README.md).
Headless-замер не доказывает расход памяти реального окна/GPU.
Любые timing-замеры сопровождай конфигурацией и условиями; известная машина
владельца — Ryzen 7 9800X3D, её результаты не универсальный бюджет других ПК.

## Изменение публичного Lua API и бандлов

1. Правь `src/lua/prelude/*.lua` и/или C ABI. Порядок prelude задаёт `k_sections`
   в `lua_prelude.cxx`; новая секция — файл плюс запись, sandbox последняя.
   Единственный список C entry points — `k_entry_points` по именам, не позициям.
2. При добавлении имени подними `tw.api_version` в `tw.core`; это единственное
   определение версии. Согласуй `@api` поставляемых проектов.
3. Выполни `ljtest` с `TW_API_UPDATE=1` для `Docs/scripting/api.json`; обнови
   `api-reference.md` и тематический публичный мануал.
4. Выполни `TweakerLuaLinker.Tests` с `TWLLINK_UPDATE_DEFS=1` для `tw.def.lua`;
   пересобери twllink: описания в него вшиты. Переменные обновления задавай
   только на время генерации, затем запускай обычную проверку без них.
5. После изменения Lua-проекта запусти
   `twllink --project <project> --out <bundle.lua>`; сохрани bundle и `.lua.map`.
   ShippedProjectsTests проверяет согласованность с исходниками.
6. Исходник, генерацию и документацию передай на review вместе. Сборка с
   ошибкой не устанавливается; `--install` подчиняется правилам записи в игру.

Пролог встроен через `cmake/EmbedText.cmake`; FFI entry points — не
`lua_CFunction`. Хендлы/семейства/соглашения вызова не менять по предположению.
Детали — [карта Lua/engine](documentation-map.md).

## Native-правила

Горячий путь — EngineLoop, draw/Present/EndScene, CallChannel и любой
покадровый код. **Не создавай тяжёлые syscalls/I/O каждый кадр**: загрузка
файлов, компиляция шейдеров и похожие операции идут в `plugin::bg_work`, кадр
проверяет готовность без блокирующего wait. Игровой WndProc/IPC выполняется
синхронно перед расчётом графа, а не на отдельном игровом IPC worker.
Инициализация/Reset/Detours bootstrap — холодные пути; здесь допустимы нужные
системные вызовы. Не превращай это в «только атомики» для всего render thread.

Минимизируй аллокации, виртуальные вызовы, логирование и блокировки в кадре;
используй ранние return. В горячем пути приоритет у производительности,
при сохранении обоснованной и понятной реализации. Exceptions/RTTI и бросающие
API в горячем пути запрещены. `[[likely]]`/`[[unlikely]]` допустимы для очевидных
путей, когда дают пользу; на cold-path предпочтительна более читаемая логика.

Под loader lock запрещены ожидания, оконные функции и приостановка потоков.
DllMain — минимальный bootstrap через plugin::on_process_attach;
ранние Detours-транзакции идут без suspend потоков.

Полные правила layout/naming, PCH/include, разделения публичного API и
реализации, ошибок и паттернов хуков находятся в
[code-style.md](code-style.md). Прочитай их до изменения соответствующего кода;
разделение на style и runtime-правила не делает первые необязательными.

LuaJIT x86 требует SSE2 из-за D3D без FPU_PRESERVE.
PUBLIC `MINIZ_NO_*` менять осторожно: они влияют на layout публичных структур.

## Состав поставки

`CopyTweakerPlugin` в `TweakerUI.csproj` задаёт единственную раскладку
`PluginPayload`: DLL в `channels`, Lua в `TweakerStuff/Scripts`, `.sky` в
`TweakerStuff/SkyboxReplacer/Skyboxes`. Deploy переносит дерево целиком и
**проверяет источники до сборки** (`Scripts/DeployPreflight.*`): сабмодули
`Skyboxes/Canvas.sky` и `ScriptProjects/puzzlepro_hud` должны быть
инициализированы, иначе deploy останавливается с командой
`git submodule update --init --recursive`. Источники — `TweakerPlugin/assets/scripts/`
и `Skyboxes/` (сабмодуль); `ScriptProjects/` — проекты, из которых собран бандл;
`dev/` не поставляется. Скины — `BundledSkins/` (сабмодуль): deploy копирует их в
`Skins/` рядом с `TweakerUI.exe`, а preflight требует хотя бы один `.askin2`/`.tasp`.
В пейлоад не попадают `.git`, `.github`, `.tweaker` и README сабмодулей — это тоже
проверяет preflight.

`CopyDevTools` задаёт `DevToolsPayload`: twllink отдельно в Binaries,
CgrPy/публичные docs/AgentKit. Деплой выбирает свежий payload по stamp, не
mtime скопированных исходников. AgentKit дев-сборки заменяется целиком,
остальной DevTools дописывается по существующему механизму. `Docs/Internal`,
`Docs/AgentWorkflow` и `Docs/readme` не относятся к публичным manuals.
Из CgrPy не поставляются `.venv`, `.cache`, out, `__pycache__`, uv.lock.
Меняешь `.ps1` — меняй парный `.sh`, и наоборот.

При редактировании сохраняй UTF-8, CRLF и escape-последовательности.
Проверяй `git diff`/`git ls-files --eol`. Название инструмента Claude не
является требованием: используй безопасный patch/editor. Windows-пути,
кавычки и `@` не пропускай через неэкранированные shell substitutions.

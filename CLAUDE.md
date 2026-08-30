# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Что это

Audiosurf Tweaker — сторонний инструмент для игры Audiosurf: смена текстурных скинов, кастомные
цветовые пресеты, живые твики (invisible road, hidden song title, banking camera и т.п.) и
встроенный плеер плейлистов. Управляет запущенной игрой снаружи через её debug-протокол
(`WM_COPYDATA`), не встраиваясь в неё как библиотека.

Живая архитектурная сводка и обоснование ключевых решений (зачем `asbridge` как отдельный процесс,
зачем `LegacyDataConverter` заморожен на .NET Framework, и т.п.) — `Docs/Internal/overview.md`.
План дальнейшей разработки (Фаза 6 — Quick Player QoL, Фаза 7 — внутриигровой оверлей) —
`Docs/Internal/roadmap.md`. Протокол оверлея (`TW_OVL`, host ↔ asbridge ↔ TweakerPlugin) —
`Docs/Internal/overlay-protocol.md`; его Quick Player-половина (операции `QP_*`, вкладка Player) —
`Docs/Internal/overlay-quickplayer.md`. Skybox Replacer (подмена скайсферы игры на cube map, плюс
весь реверс её загрузки и отрисовки) — `Docs/Internal/skybox-replacer.md`; ресёрч и план
процедурного (шейдерного) неба — `Docs/Internal/skybox-procedural.md`. Проект встраивания LuaJIT в
`TweakerPlugin` (скрипты на пути обработки данных движка, выбор технологии, механика перехвата) —
`Docs/Internal/lua-scripting.md`.

**Пользовательская** документация по скриптовому API (в отличие от всего вышеперечисленного —
на английском, для тех, кто пишет скрипты и делится ими) — `Docs/scripting.md` и `Docs/scripting/`:
getting-started, game-model (как устроен граф каналов игры и как в нём искать), channels, hooks,
drawing, api-reference, limits. При изменении публичного API `tw.*` обновлять их обязательно —
`api-reference.md` перечисляет каждую функцию поимённо.

Накопительный полевой журнал реверса самой игры — три файла, все описывают **чужой** код, игру, и
служат источником для остальных документов: `Docs/Internal/reversing-journal-lua.md` (формат
`.cgr`, Lua-движок `Aco_Lua` и его API, дамп скриптов, настройка Ghidra);
`Docs/Internal/reversing-journal-gameplay.md` — про **игровую логику** (граф каналов Quest3D как
язык, `StatCollector.cgr` и вся статистика заезда, 18 персонажей/режимов в `SpecialPurpose.cgr`,
сетка `Puzzle.cgr`); `Docs/Internal/reversing-journal-engine.md` — про **нативное ядро**
(`HighPoly.dll`, ABI каналов и vtable, раскладка объекта `A3d_Channel`, как исполняется кадр, цена
операций, сводка экспортов для хуков). Читай эти файлы для полного контекста прежде, чем начинать
что-то нетривиальное в соответствующей области — этот CLAUDE.md даёт только ориентацию.

Внимание: §7.2 `reversing-journal-gameplay.md` (событийный поток через детур на
`A3d_Channel::CallChannel`) **отменена** — см. `reversing-journal-engine.md` §7.

## Устройство решения

9 проектов в `Audiosurf SkinChanger.sln`, три платформенных слоя снизу вверх:

- **`TweakerCore`** — модельный/файловый слой без UI-зависимостей: формат скинов (zip +
  `manifest.json` + PNG), цветовые пресеты, `FolderChecker` (дрейф папки текстур), кэш превью.
- **`AudiosurfInterface`** — управляемый клиент протокола игры: `GameProtocol`, `GameConfigState`
  (LIFO-оверрайды `asconfig`), `GameReportListener`, `AudiosurfHandle` (публичный фасад с
  событиями подключения) и `Bridge/` — тонкий pipe-клиент `asbridge.exe`.
- **`QuickPlayerCore`** — модель плейлистов, каталог тэгов песен, драйвер воспроизведения.
  Зависит от `TweakerCore` + `AudiosurfInterface`.
- **`TweakerUI`** — единственный UI, AvaloniaUI + `CommunityToolkit.Mvvm`. MVVM: view models
  наследуют `ViewModelBase : ObservableObject`, `ViewLocator` резолвит View по имени типа
  (`FooViewModel` → `FooView`) через рефлексию — при добавлении новой пары ViewModel/View
  достаточно совпадающих имён и неймспейсов, регистрировать их вручную нигде не нужно. Каждая
  вкладка — своя пара `<Prefix>Styles.axaml` в `Themes/`; общее (акцентные/нейтральные цвета,
  тёмная тема, шаблоны контролов) — в отдельных словарях, подключено через `DynamicResource`,
  чтобы тема переключалась без перезапуска.
- **`LegacyDataConverter`** — отдельный консольный `.exe`, намеренно застрявший на .NET
  Framework 4.8.1 (единственное место, где ещё жив `BinaryFormatter`, удалённый из рантайма
  начиная с .NET 9). Конвертирует старые `.tasp`/`.askin2`/`.pltc` в новые форматы; вызывается
  основным приложением как внешний процесс.
- **`ASBridge`** (`asbridge.exe`) — нативный C++23/CMake субпроцесс, единственный, кто трогает
  Win32 (`WM_COPYDATA`, поиск окна игры). Держит именованный пайп с текстовым протоколом наружу.
  Собирается CMake-хуком из pre-build шага `TweakerUI.csproj`, в `.sln` не участвует.
- **`InjectHelper`** — обобщённый `CreateRemoteThread`-DLL-инжектор (x86 exe, для `TweakerPlugin`),
  заморожен на `vcxproj`, собирается тем же pre-build-хуком.
- **`TweakerPlugin`** — нативный x86 C++23/CMake DLL-плагин внутриигрового оверлея (см. отдельный
  раздел ниже). Собирается аналогично `ASBridge`, но **best-effort**: неудачная сборка (нет
  DirectX/Quest3D SDK, ImGui или Detours submodule) не ломает сборку остального решения.
- **`Installer`** — COM-интероп для ярлыков, собирается только полным MSBuild (не `dotnet build`).
- Тесты (NUnit): `TweakerCore.Tests`, `QuickPlayerCore.Tests`, `AudiosurfInterface.Tests`.

**Почему так** (детали и обоснование — `Docs/Internal/overview.md`): игра — внешний процесс, а не
библиотека, поэтому весь Win32/`WM_COPYDATA` изолирован в `asbridge`, а не встроен P/Invoke-ом в
Avalonia-приложение напрямую; `TweakerCore`/`AudiosurfInterface`/`QuickPlayerCore` не знают про UI
(кидают простые C#-события, не Avalonia-объекты) — иначе граф зависимостей переворачивается.

## Сборка

Управляемая сторона — .NET 10 (`net10.0-windows` / `net10.0-windows10.0.19041.0` для `TweakerUI`,
см. `Directory.Build.props`), `PlatformTarget=x64`, `Nullable=disable`, `ImplicitUsings=disable`
проектной политикой (не включать точечно без причины). `LegacyDataConverter` — единственное
исключение (net481).

```
dotnet build "Audiosurf SkinChanger.sln"     # вся управляемая сторона
dotnet build TweakerUI/TweakerUI.csproj      # только UI (+ триггерит нативные pre-build хуки ниже)
dotnet run --project TweakerUI/TweakerUI.csproj
```

`LegacyDataConverter` — старый `vcxproj`-стиль csproj (net481), полноценно собирается только
классическим MSBuild (`vswhere` → `MSBuild.exe`), не `dotnet` CLI — см. `Scripts/Deploy.ps1`.
`Installer` тоже требует полного MSBuild.

**Нативные pre-build хуки в `TweakerUI.csproj`** — при любой сборке `TweakerUI` (включая внутри
`dotnet publish`) на Windows автоматически идёт `cmake --fresh -S/-B` + `cmake --build` для:
- `ASBridge` — обязателен, падение ломает сборку.
- `InjectHelper` — обязателен, собирается с явным `-A Win32` (нужен настоящий x86-бинарник —
  инжектор работает только при совпадении битности с 32-бит `QuestViewer.exe`).
- `TweakerPlugin` — best-effort, `-A Win32`; неудачный configure/build превращается в `<Warning>`,
  не в ошибку сборки (см. комментарии в `TweakerUI.csproj` за деталями).

Каждый хук использует свою собственную `build/msbuild` директорию — отдельно от Ninja-пресетов
разработчика (`TweakerPlugin/build/x86-debug`, `x86-release`), чтобы не смешивать деревья сборки
и не форсировать пересборку Detours/DirectX/Quest3D/ImGui на каждый `dotnet build`.

## Тесты

NUnit, на проект:

```
dotnet test TweakerCore.Tests/TweakerCore.Tests.csproj
dotnet test QuickPlayerCore.Tests/QuickPlayerCore.Tests.csproj
dotnet test AudiosurfInterface.Tests/AudiosurfInterface.Tests.csproj

# один тест:
dotnet test TweakerCore.Tests/TweakerCore.Tests.csproj --filter "FullyQualifiedName~ClassName.MethodName"
```

Всё, что требует реально запущенной игры (bridge-подключение, оверлей end-to-end) — юнит-тестами
не покрывается и не может быть проверено в песочнице; это явно за пользователем (см.
«Текущее состояние» в `overview.md`).

## Деплой / дистрибуция

`Scripts/Deploy.ps1` — release-бандл: self-contained single-file win-x64 `dotnet publish`
`TweakerUI` + release-сборка `LegacyDataConverter` (IL-merged через `ILRepack.Lib.MSBuild.Task`) +
`asbridge.exe`/`InjectHelper.exe`/(опционально) `TweakerPlugin.dll`, подобранные из-под
`TweakerUI/bin` после pre-build хуков — всё в `/distr/TweakerUI/`.

`Scripts/DeployDebug.ps1` — то же самое, но multi-file publish с PDB (managed и нативные
SkiaSharp/HarfBuzzSharp), чтобы можно было приаттачиться дебаггером к `TweakerUI.exe`.
`LegacyDataConverter` в обоих скриптах всегда собирается Release (коллизия сборок при Debug —
см. комментарий в `DeployDebug.ps1`).

Оба скрипта — PowerShell (`.ps1`); `.sh`-версии в `Scripts/` — заготовки, не поддерживаются активно.

## Нативные компоненты

Оба нативных субпроекта — C++23, CMake, `.hxx`/`.cxx` расширения, PCH, без exceptions в
проектном коде (см. правила `TweakerPlugin` ниже — тот же дух применяется и к `ASBridge`, хоть
явно и не задокументирован там отдельным файлом).

- **`ASBridge`** (`ASBridge/src/`) — `pipes/` (именованный пайп к managed-стороне), `proto/`
  (`message.hxx`/`.cxx` — текстовый протокол `CCOMMAND`/`SREPORT`, включая `OVERLAY_*`-расширение
  для `TW_OVL`), `service/` (`service.cxx` — `process_wm_copydata`, форвардинг `asreport`/оверлея),
  `system/`, `window/`. Использует vcpkg toolchain, если задан `VCPKG_ROOT`.
- **`InjectHelper`** — один `main.cpp`, `CreateRemoteThread`-инжектор общего назначения
  (`InjectHelper.exe <PID> <dllPath>`), не завязан на конкретную DLL-полезную нагрузку.

### `TweakerPlugin` — внутриигровой оверлей

Внедряемая **x86 DLL** для Audiosurf (32-bit процесс). Стек: **C++23**, Win32 API, **DirectX 9**
(June 2010 SDK), **Microsoft Detours**, **Quest3D SDK**, **ImGui**, **LunaSVG** (+ её сабмодуль
plutovg). Сборка: CMake + Ninja, MSVC, PCH.

```
dllmain.cxx      — минимальный DllMain, инициализация в отдельном потоке
src/framework/    — хуки (Detours, D3D9, dinput8, Quest3D channel + texture_hook на
                    Aco_DX8_Texture::LoadTextureFromMemory), channel_shim (перехват вызова
                    ОДНОГО канала подменой vptr на копию vtable; подписчиков на канал может быть
                    НЕСКОЛЬКО — все before отрабатывают до отмены, отмена это ИЛИ, у подавленного
                    вызова нет after; оригинальная vtable возвращается с уходом последнего
                    подписчика — см. lua-scripting.md §8.5 и Ф3), wndproc_hub (общая точка
                    подписки на WndProc игры: IPC, D3D9 WM_ACTIVATEAPP, будущий ImGui-инпут)
src/ipc/          — overlay_ipc: разбор/сборка L3-протокола TW_OVL (см. overlay-protocol.md);
                    операции с префиксом QP_ он не разбирает, а форвардит в src/ui/qp/
src/ui/           — overlay_state (кэш состояния, generation-counter, lock-free read по try_lock),
                    pending_actions (optimistic UI + таймаут-подтверждение для reverse-sync),
                    wire_text (общий percent-кодек L3), ui_main/theme, gpu_texture (единственная
                    точка подключения рендер-бэкенда для текстур), texture_cache (растр, лениво),
                    ui/image/ (SVG-иконки через LunaSVG, запекаются заранее — см.
                    tweaker-plugin-widgets.md), ui/plugins/, ui/widgets/
src/ui/qp/        — Quick Player: qp_catalog (зеркала теги/персонажи/режимы), qp_state (модель +
                    generation-кэш, как overlay_state), qp_wire (грамматика QP_* в обе стороны),
                    qp_pending (optimistic reverse-sync). См. overlay-quickplayer.md
src/skybox/       — Skybox Replacer: перехват draw-call скайсферы игры и отрисовка cube map на
                    собственном кубе (skybox, sky_renderer, sky_cubemap, sky_paths, sky_catalog,
                    sky_math, skybox_config) + sky_ui — вкладка оверлея, регистрируемая через
                    menu::add_extra_tab (в tweaker_ui её быть не может: он общий со smoke_test),
                    + sky_panel — окно параметров шейдера, пристыкованное сбоку к меню
                    (вкладки по группам @sky, рисуется из ui_main после menu::update).
                    Плюс процедурное небо — второй вид источника, равноправный с cube map:
                    sky_program (реестр программ — вшитые + скомпилированные с диска),
                    sky_compile (HLSL→ps_3_0 через d3dcompiler_47, #include из ресурсов),
                    sky_bytecode (разбор CTAB: какие константные регистры шейдер объявляет и как
                    их зовут), sky_params (аннотации `@sky` в исходнике шейдера → ползунки),
                    sky_shader (кэш VS/PS на программу + invalidate для горячей перезагрузки),
                    sky_target (рендер неба в долю разрешения), renderer::draw_program,
                    sky_timer (время draw-call'а на GPU), sky_caps (одноразовый дамп
                    D3DCAPS9/BehaviorFlags). См.
                    skybox-replacer.md и skybox-procedural.md; sky_math существует затем, чтобы
                    не линковать d3dx9
src/lua/          — LuaJIT-скриптинг: lua_channels (доступ к графу каналов через vtable: слот 17
                    у числовых/строковых/векторных значит РАЗНОЕ, поэтому тип проверяется до
                    вызова), lua_api (extern "C" ABI, который скрипт зовёт через FFI — НЕ
                    lua_CFunction, см. lua-scripting.md §2.2), lua_host (VM, пролог, песочница,
                    диспетч on_frame под ImGui ErrorRecovery-guard'ом + реестр скриптов:
                    метаданные из `-- @name/@author/@version/@description` читаются БЕЗ запуска
                    файла), lua_config (какие скрипты выключены, TweakerScripts.cfg — хранятся
                    только исключения), lua_ui (вкладка Scripts, регистрируется через
                    menu::add_extra_tab, как и Skybox). Выключение скрипта = снятие его подписок
                    и возврат оригинальных vtable, а не спящий хук; включение = повторный запуск
                    файла с диска, оно же горячая перезагрузка. Скрипты — loose-файлы
                    в scripts/ рядом с DLL, не ресурсы: их правят без пересборки; в бандл их
                    кладут CopyTweakerPlugin (TweakerUI.csproj) и Deploy.ps1
src/plugin/       — lifecycle, глобальное состояние, Quest3D state
src/resource/     — .rc-based упаковка ассетов (шрифты/текстуры/SVG/шейдеры) прямо в DLL;
                    assets/shaders/*.hlsl при этом компилируются fxc на этапе сборки, и вшивается
                    только байткод (TW_SHADER). Профиль берётся из имени: *.vs.hlsl / *.ps.hlsl;
                    *.hlsli — общие заголовки, вшиваются как TW_TEXT (не программы, глоб шейдеров
                    их не берёт), чтобы пользовательский .hlsl мог их #include без копии на диске
src/libtweeny, libstb, libuulog — vendored (Tween-анимации, stb_image + stb_image_resize2, лог) —
                    не трогать стиль
```

**Зависимости**: DirectX SDK / Quest3D SDK / ImGui / Detours ожидаются на диске (см. `cmake/*.cmake`).
`fxc.exe` из Windows SDK нужен для `assets/shaders/*.hlsl` — ищется автоматически (dev-окружение или
`Windows Kits/10/bin/*/x86`), при отсутствии configure падает с инструкцией; можно задать
`-DTWEAKER_FXC=<путь>`.
LuaJIT — git-сабмодуль `TweakerPlugin/LuaJIT` (ветка `v2.1`), собирается `cmake/LuaJIT.cmake` —
портом апстримного `msvcbuild.bat` на CMake (minilua → DynASM → buildvm → `lj_vm.obj`), x86,
`/arch:SSE2` (флаг обязателен: игра создаёт D3D9-устройство без `FPU_PRESERVE`).
LunaSVG — единственная, которая тянется сама: `FetchContent`, тег `v3.5.0`, клоны в
`TweakerPlugin/.deps/<генератор>/` (вне `build/`, чтобы `cmake --fresh` из pre-build хука не
переклонировал их на каждый `dotnet build`; разбивка по генератору обязательна — подкаталог
`-subbuild` у FetchContent привязан к генератору, и общий на Ninja и VS каталог ломает второй
configure). Значит, первый configure под каждый генератор требует сети.

CMake-пресеты разработчика (`CMakePresets.json`, отдельно от MSBuild pre-build хука выше). Пресеты
объявляют `architecture x86` / `toolset host=x64` со стратегией `external` — то есть окружение
обязан подготовить вызывающий: **`vcvarsall.bat amd64_x86`**, не `x86`. Если сконфигурировать из
шелла с другим хостом, CMake решит, что компилятор сменился, удалит кэш и переконфигурируется **без
пресета** — а `CMAKE_BUILD_TYPE` живёт только в пресете, так что `--build --preset x86-release`
начнёт молча собирать неоптимизированное в каталог с названием release. Корневой `CMakeLists.txt`
на этот случай падает с явной ошибкой; лечится `cmake --preset <имя>` из правильного шелла.

```
cmake --build --preset x86-debug     # Ninja, для clangd/compile_commands.json
cmake --build --preset x86-release
cmake --build --preset smoke         # smoke_test: визуальный Win32+OpenGL3 харнесс для ImGui UI
                                      # (EXCLUDE_FROM_ALL, не входит в обычный build)
```

- Корневой namespace: `tw::`, подпространства по слоям: `tw::plugin`, `tw::framework`, `tw::ui`.
- Публичный API — в заголовках; детали реализации (хуки, оригиналы, file-local state) — в
  **anonymous namespace** в `.cxx`, объявленный **вне** именованных, **над** ними, сразу после
  блока `#include`:

```cpp
#include "pch.hxx"

#include "framework/foo.hxx"

namespace
{
// file-local helpers, hooks, state
} // namespace

namespace tw::framework
{
// public API definitions
} // namespace tw::framework
```

- Не вкладывать anonymous namespace внутрь `tw::*`. Закрывающие комментарии namespace:
  `} // namespace tw::framework::d3d9`.
- Глобальное состояние — только там, где необходимо; префикс `g_` (`g_engine`, `g_ui_draw`).
- Vendored-код (`ImGui/`, `Detours/`, SDK) ТРОГАТЬ ЗАПРЕЩЕНО.

#### Performance (hot-path)

Hot-path: `EndScene`, `CallChannel`, любой код, вызываемый каждый кадр (включая `overlay_state`
чтение в `ui_main.cxx: draw_frame` — но **не** сам разбор `TW_OVL`-сообщений, он происходит
синхронно на IPC-потоке и редко, см. `overlay-protocol.md`, «Многопоточность»).

- **Performance > readability** на hot-path, но без бессмысленного говнокода.
- Минимум аллокаций, виртуальных вызовов, логирования, блокировок.
- Ранние `return`; проверки дешевле тяжёлой работы.
- **Никаких exceptions, try/catch, RTTI** в hot-path. Не вызывать в hot-path API, которые могут
  бросать исключения.
- Инициализация, Detours-транзакции, bootstrap D3D9 — cold-path; там допустима более читаемая логика.
- Для очевидных if/else путей допустимы `[[likely]]`/`[[unlikely]]`, если явное указание
  возможности/невозможности пути может увеличить перф.

#### Именование (из `.clang-tidy`)

Источник правды — `readability-identifier-naming` в `.clang-tidy`. Всё проектное — **lower_case**
(snake_case). Исключение — имена библиотечных/системных типов и функций (`IDirect3DDevice9`,
`HRESULT`, `DetourAttach`).

| Сущность (clang-tidy key) | Case | Prefix | Пример |
|---------------------------|------|--------|--------|
| Namespace | `lower_case` | — | `tw::framework::detour` |
| Class / Struct | `lower_case` | — | `binding`, `resource` |
| Function / Method | `lower_case` | — | `attach`, `draw_frame` |
| Variable / Parameter | `lower_case` | — | `device`, `module_handle` |
| TypeAlias | `lower_case` | — | `end_scene_fn`, `ui_plugin_draw_fn` |
| Enum / EnumConstant | `lower_case` | — | `resource_type::texture` |
| MemberPrivate | `lower_case` | `m_` | `m_device` |
| MemberProtected | `lower_case` | `m_` | `m_engine` |
| MemberPublic | `lower_case` | *(нет)* | `target`, `replacement` |
| TemplateParameter | `CamelCase` | — | `T`, `TValue`, `TResult` |
| TemplateTemplateParameter | `CamelCase` | — | `TContainer` |

Дополнительно по кодовой базе (не в clang-tidy, но принято):

| Сущность | Стиль | Пример |
|----------|-------|--------|
| file-local / global state | `g_` + snake_case | `g_bound_device`, `g_ui_draw` |
| hook / original | `hk_*` / `o_*` / `true_*` | `hk_end_scene`, `o_reset` |

`.clang-format` naming не задаёт — только layout. Расширения C++: **всегда** `.hxx` (заголовки) и
`.cxx` (translation units). `.h`/`.cpp` не использовать.

#### Precompiled headers

- Используется PCH (`src/pch.hxx`). Нужен системный или библиотечный заголовок — **сначала смотри
  в `pch.hxx`**. Если его там нет — **добавь в `pch.hxx`**, не в `.cxx`/`.hxx`.
- В `.cxx`/проектных заголовках **не** писать `#include` системных/SDK/STL заголовков напрямую.
- В `.cxx`: `#include "pch.hxx"` первым, затем свой заголовок, затем проектные.
- **Не** forward-declare системные и библиотечные типы (`HMODULE`, `HWND`, `IDirect3DDevice9`) —
  они уже в PCH. Forward-declare допустим только для **собственных** неполных типов в заголовках
  (`class EngineInterface;`).
- Quest3D SDK: порядок include в `pch.hxx` фиксирован; блок `// clang-format off/on` не трогать.

#### Порядок include

```cpp
#include "pch.hxx"

#include "framework/foo.hxx"    // свой заголовок

#include "plugin/bar.hxx"       // другие слои — группировать по root-пути

#include "ui/baz.hxx"
```

Группы разделять **пустой строкой**. Внутри группы — сортировка clang-format (`SortIncludes: true`).
Системные/библиотечные include — только через `pch.hxx`.

#### Обработка ошибок

Functional-style, **без exceptions** в проектном коде.

- Предпочитать `std::expected<T, E>` или `bool` / enum error-code.
- Win32/D3D: `SUCCEEDED`/`FAILED`, проверка `nullptr`, `NO_ERROR` для Detours.
- При ошибке — откат состояния (см. `install_d3d9_hooks`: обнуление указателей при failed attach).
- `try/catch` — **только** при обёртке внешних API, которые гарантированно бросают, и **только**
  вне hot-path.
- `assert` — только для инвариантов в debug; не как механизм обработки ошибок в runtime.

#### Форматирование (из `.clang-format`)

Источник правды — корневой `.clang-format` в `TweakerPlugin/`. Писать код сразу в этом стиле; не
полагаться на «поправлю потом» (нет гарантированного format-on-save в момент генерации кода).

- C++, `Standard: c++20` для форматтера (проект собирается как C++23). Отступ 4 пробела, табы
  запрещены. CRLF. Ширина строки 140. Максимум одна подряд пустая строка.
- Указатели/ссылки: `*`/`&` слева от имени (`int* p`, `const T& ref`); квалификатор — пробел
  перед ним (`const int* const`).
- **Функции и namespace** — `{` на новой строке (пустые тела тоже). **class/struct/enum/union/
  if/for/while/switch** — `{` на той же строке. `else`/`catch` — на новой строке перед собой.
  Тело лямбды — `{` на той же строке. Короткие блоки/if/циклы/лямбды/case/функции — **не**
  сжимать в одну строку (кроме коротких enum). **if/for/while и т.п. всегда оборачивают тело в
  скобки**; `case` внутри `switch` может без скобок для однострочных выражений.

```cpp
void draw_frame(IDirect3DDevice9* device)
{
    if(device == nullptr) {
        return;
    }
    else {
        render(device);
    }
}

namespace tw::ui
{
} // namespace tw::ui
```

- Нет пробела перед `(` у `if`/`for`/`while`/`switch`/вызовов; нет пробелов внутри `()`/`[]`/`<>`;
  нет пробела после C-cast и `!`; нет пробела после `template`. Есть пробел перед `=`/`+=`/... и
  перед `:` у range-for и у наследования/ctor-initializer; нет пробела перед `:` у `case`. Пробел
  перед `{` у braced-init, но список не «компактный Cpp11» (`params{ }`).
- Аргументы/параметры **не** bin-pack'ать: либо всё в одну строку (если ≤140), либо каждый на
  своей. После открывающей `(` — не выравнивать в столбец, обычный continuation indent 4.
  Неassignment бинарные операторы и тернарник — перенос **перед** оператором/`?`/`:`. `template<...>`
  у деклараций — всегда с переносом после. Список наследования/ctor-initializer — break перед `:`.
- Namespace не индентит содержимое; `a::b::c` не схлопывать в одну строку. Закрывающие комментарии
  namespace — автоматически (`} // namespace tw::ui`). Access modifiers — отступ −4 от членов.
  `case`/блоки case — с отступом. `#`-директивы без доп. отступа. `#include`/`using` — сортировать.

```cpp
template<typename TValue>
class resource
{
public:
    TValue* data;

private:
    int m_refcount;
};

bool ok = attach({
    { &o_reset, reinterpret_cast<void*>(hk_reset) },
    { &o_end_scene, reinterpret_cast<void*>(hk_end_scene) },
});

if(!ok || device == nullptr) {
    return false;
}
```

Форматирование в IDE: clangd + `formatOnSave` (см. `TweakerPlugin/.vscode/settings.json`).

#### Паттерны хуков

- Оригиналы: `o_*` или `true_*`; хуки: `hk_*` / `*_hook`. Типы оригиналов — `using ..._fn = ...`
  рядом с указателями.
- Множественные Detours — через `tw::framework::detour::attach({...})`, не дублировать
  транзакционную логику.
- D3D9 vtable resolve — cold-path bootstrap; не повторять в hot-path.
- UI подключается callback'ом (`attach_ui_plugin`), не жёсткой зависимостью framework → ui.

#### CMake / сборка

- Только **x86** (32-bit). C++23, `/std:c++latest` на MSVC.
- Новые `.cxx` — в `CMakeLists.txt` соответствующего подкаталога `src/`.
- `target_include_directories(TweakerPlugin PRIVATE src)` — include-пути от `src/`:
  `"framework/foo.hxx"`, не относительные `../`.

#### Чего избегать

- Exceptions, `throw`, тяжёлые `catch` в hot-path и в новом коде по умолчанию.
- `#include` SDK/STL в заголовках проекта (кроме PCH).
- Логика в `DllMain` кроме минимального bootstrap.
- Модификация vendored-зависимостей под стиль проекта.
- PascalCase/camelCase для проектных имён (кроме template-параметров).

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
`Docs/Internal/roadmap.md`. **Как плагин попадает в игру и живёт там без хоста** (ранняя загрузка из
`engine\channels\`, раскладка `engine\TweakerStuff\`, оффлайн-режим, установка из TweakerUI) —
`Docs/Internal/plugin-offline-mode.md`, и его §0 написан так, чтобы его читали первым. Протокол
оверлея (`TW_OVL`, host ↔ asbridge ↔ TweakerPlugin) — `Docs/Internal/overlay-protocol.md`; его Quick Player-половина (операции `QP_*`, вкладка Player) —
`Docs/Internal/overlay-quickplayer.md`; скриптовая (операции `SCRIPT_*`, протокол 2; реализована с обеих
сторон и принята 03.10.2026) — `Docs/Internal/overlay-scripts.md`. Skybox Replacer (подмена скайсферы игры на cube map, плюс
весь реверс её загрузки и отрисовки) — `Docs/Internal/skybox-replacer.md`; ресёрч и план
процедурного (шейдерного) неба — `Docs/Internal/skybox-procedural.md`; замена процедурного
шума запечённым объёмом и текстуры на базовом слое (что померено, чем за это заплачено в картинке) —
`Docs/Internal/skybox-baked-noise.md`; разбор того, почему одному
пиксельному шейдеру перестало хватать и что за геометрический слой (спрайты, в перспективе меши)
собираемся строить — `Docs/Internal/skybox-geometry.md`; формат сменных пакетов неба `.sky` (манифест,
слои, общие значения, пофайловые настройки) — `Docs/Internal/sky-package.md` (**устарел**: описывает
состояние до генерализации света, `fill`, `generator` и архивной формы). **Начинать с
`Docs/Internal/skybox-replacer-roadmap.md`** — это карта всех перечисленных документов, текущее
состояние работ, хронология принятых решений с обоснованиями и порядок дальнейших шагов. Проект встраивания LuaJIT в
`TweakerPlugin` (скрипты на пути обработки данных движка, выбор технологии, механика перехвата) —
`Docs/Internal/lua-scripting.md`. Он описывает **проект** слоя и остаётся источником правды по
механике перехвата; план превращения написанного прототипа в инструмент (жизненный цикл движка,
разделение разметки vtable по семействам каналов, изоляция отказов, сборка скриптов из модулей) —
`Docs/Internal/lua-engine-fix-roadmap.md`, и **начинать работу над Lua-слоем надо с него** (план
выполнен целиком, Ф1–Ф5 закрыты 27.09.2026). Хостовая сторона скриптов — **модуль скриптов в
`TweakerUI`** (вкладки Scripts и Script Dev, связь с плагином по `SCRIPT_*`; сделан и принят 03.10.2026) —
`Docs/Internal/script-manager.md`: руководство по модулю — как он устроен, что в каком файле, решения и
почему, `twllink` как CLI; в 2.5 тысячи строк плана движка заглядывать только за деталями, на которые оно
ссылается. **План в работе** (с 05.10.2026) — `Docs/Internal/script-coexistence-roadmap.md`: как скрипты
уживаются друг с другом — порядок выполнения, который задаёт пользователь (П1 плагин, П2 TweakerUI), и
`tw.hud.occupied`/`claim`/`place` — где в HUD уже занято (П3); начинать с его блока «где мы сейчас».

**Пользовательская** документация по скриптовому API (в отличие от всего вышеперечисленного —
на английском, для тех, кто пишет скрипты и делится ими) — `Docs/scripting.md` и `Docs/scripting/`:
getting-started, game-model (как устроен граф каналов игры и как в нём искать), channels, hooks,
drawing, api-reference, limits, examples (разобранные скрипты из `assets/scripts/dev/`), authoring
(проекты и `twllink`), tweaker (вкладки Script Dev и Scripts в TweakerUI — точка входа для автора; при
изменении этих вкладок обновлять и её). При изменении публичного API `tw.*` обновлять их обязательно — `api-reference.md`
перечисляет каждую функцию поимённо, и с Ф5b это **проверяется**: три списка имён — пролог,
`Docs/scripting/api.json`, `api-reference.md` — сверяют `ljtest` и `TweakerLuaLinker.Tests`, а
`Docs/scripting/tw.def.lua` (определения для LuaLS) генерируется из двух последних. Порядок после
правки API в прологе: поднять `tw.api_version` (в `tw.core`), если имя **добавлено**; прогнать `ljtest`
с `TW_API_UPDATE=1` (переписывает `api.json`); дописать `api-reference.md`; прогнать
`TweakerLuaLinker.Tests` с `TWLLINK_UPDATE_DEFS=1` (переписывает `tw.def.lua`); пересобрать `twllink`
(оба файла в него вшиты). Закоммитить всё вместе.

Второй такой же набор — по **формату неба** `.sky`, для тех, кто делает свои скайбоксы:
`Docs/skyboxes.md` и `Docs/skyboxes/`: getting-started, manifest (`Config.json` целиком), shaders
(константный контракт, чего трогать нельзя), music (музыкальный интерфейс и что он **не** может),
clouds (спрайтовый слой, `place()`/`fill()`), limits (цена, замеры, чёрное небо). Обновлять при
любом изменении манифеста, набора регистров или Lua-API генераторов. `Docs/Internal/sky-package.md`
устарел и **не** является заменой — источник правды теперь публичный набор.

Накопительный полевой журнал реверса самой игры — шесть файлов, все описывают **чужой** код, игру,
и служат источником для остальных документов: `Docs/Internal/reversing-journal-boot.md` — про
**запуск и жизненный цикл** (кто зовёт кадр — `EngineControl::EngineLoop`, порядок загрузки
`Q3DStart.q3d` → `start - project loader.cgr` → `XX_StartHere.cgr`, как увидеть «игра загрузилась»,
почему значение канала до первого исполнения графа принадлежит редактору, а не игре, выгрузка и
пересоздание групп во время игры, полная перепись семейств каналов и слотов 17–19 у каждого);
`Docs/Internal/reversing-journal-lua.md` (формат
`.cgr`, Lua-движок `Aco_Lua` и его API, дамп скриптов, настройка Ghidra);
`Docs/Internal/reversing-journal-gameplay.md` — про **игровую логику** (граф каналов Quest3D как
язык, `StatCollector.cgr` и вся статистика заезда, 18 персонажей/режимов в `SpecialPurpose.cgr`,
сетка `Puzzle.cgr`, **аудио** — спектр/громкость/позиция песни в `sounds/VisMusic.cgr`, §10);
`Docs/Internal/reversing-journal-engine.md` — про **нативное ядро**
(`HighPoly.dll`, ABI каналов и vtable, раскладка объекта `A3d_Channel`, как исполняется кадр, цена
операций, сводка экспортов для хуков); `Docs/Internal/reversing-journal-geometry.md` — про
**3D-геометрию** (чанки `VRCO`/`VPPI`/`PODA` в каналах `3D ObjectData`, vtable и сеттеры
`Aco_DX8_ObjectDataChannel`, холодный патч `.cgr` и горячая подмена мешей, экспорт в OBJ,
инвентарь всех 930 моделей игры); `Docs/Internal/reversing-journal-render.md` — про **графическое
окружение трассы** (`Aco_Tune_Forest` как спрайтовый батчер с атласом 2×2 и сортировкой по глубине,
`Aco_Tune_Wall` как экструзия дороги вдоль хребта `Highway.cgr`, загрузка текстур из
`engine/textures` через Lua, полный порядок кадра `RenderNormal`/`RenderMinimalist`, разбор колец,
партиклов, решётки и tile flyup). Там же §7 — разобранный кейс «отключить окраску партиклов» — и
§8, **список того, чего не хватает Lua API** (запись в векторные каналы и в `Array Vector`,
отцепление ребёнка канала): читать перед любой работой над скриптовым слоем, вместе с
`lua-scripting.md`. Читай эти файлы для полного контекста прежде, чем начинать
что-то нетривиальное в соответствующей области — этот CLAUDE.md даёт только ориентацию.

**Инструменты, которыми всё это добыто, — `Tools/CgrPy/`** (свой `README.md`): парсер `.cgr` и
графа каналов плюс минимальный PE-ридер, поверх них готовые команды в `rdy2use/` (`vtdump`,
`findvt`, `disasm`, `symbols`, `chtypes`, `render`, `meshes`). Запуск — `uvrun.bat`, зависимости
ставит `uv` сам, путь к игре берётся из `AUDIOSURF_DIR` или находится сам. Прежде чем писать
очередной разовый скрипт разбора — посмотри, нет ли уже готового: журналы ссылаются на конкретные
команды, которыми их числа получены, и их стоит перепроверять этими же командами. Игра
(`Audiosurf_Steam/`) в репозиторий не входит и не должна.

**Вторым инструментом есть IDA Pro 9.3 через MCP** — сервер `idalib` (плагин Claude Code
`ida-pro-mcp@mrexodia`, headless idalib, ставится на машину, а не в репозиторий). Пользоваться им
можно и нужно — вместо `CgrPy` или вместе с ним, по задаче. IDA — для кода: декомпиляция (x86
работает), xrefs, граф вызовов, чтение данных (`get_int`, `read_struct` — vtable, таблицы), поиск
по байтам и инструкциям, и **накопление понимания в базе** (`rename`, `set_comments`, `set_type`).
`CgrPy` — для того, чего IDA не умеет или делает дорого: граф каналов и формат `.cgr`, массовые
прогоны по всем 242 DLL каналов (`chtypes`, `findvt`), и числа, на которые уже ссылаются журналы.
Базы — `.i64` рядом с бинарём в `Audiosurf_Steam/`: это копия игры, отданная под эксперименты, базы
в ней писать и перезаписывать можно свободно; закрывать `idb_close` с сохранением, чтобы найденное
оставалось. Находки из IDA в журналах — с адресом/RVA и пометкой, что сняты IDA. Ловушка: на адрес,
где линкер склеил одинаковые функции (ICF), IDA вешает **одно** имя, иногда чужого класса —
`reversing-journal-engine.md` §11. Если сервера `idalib` в сессии нет, работают `CgrPy` и Ghidra, как
раньше.

Внимание: §7.2 `reversing-journal-gameplay.md` (событийный поток через детур на
`A3d_Channel::CallChannel`) **отменена** — см. `reversing-journal-engine.md` §7.

## Устройство решения

11 проектов в `Audiosurf SkinChanger.sln`, три платформенных слоя снизу вверх:

- **`TweakerCore`** — модельный/файловый слой без UI-зависимостей: формат скинов (zip +
  `manifest.json` + PNG), цветовые пресеты, `FolderChecker` (дрейф папки текстур), кэш превью,
  `PluginInstall/` (установка плагина в игру) и `Scripting/` — модель модуля скриптов
  (`script-manager.md` §4): шапки и `scripts.cfg` читаются **ровно как в плагине**, каталог
  `Scripts\` игры, проекты `twllink` и клиент его CLI, редакторы (`ScriptEditors`: поиск установленных,
  запуск с `DevTools\Binaries` в `PATH` и `AUDIOSURF_DIR` — пользовательский `PATH` не трогается) и
  `ScriptWire` — грамматика `SCRIPT_*` хоста (С5b; состояние и ожидание подтверждений —
  `TweakerUI/Core/ScriptsOverlayBridge`, транспорт — `OverlayHelper`).
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
  чтобы тема переключалась без перезапуска. Шаблоны контролов — только общие, из
  `Themes/SharedControlTemplates.axaml` (`FlatButtonTemplate`, `FlatTextBoxTemplate`,
  `FlatListItemTemplate`), не копия во вкладке; то же для строительных блоков, одинаковых во всех
  вкладках, — `Themes/SharedStyles.axaml` (`Card`, `Switch`, `ToggleRow`, `IconButton`, `ActionButton`, `TextField`, `RowList`, …, классы без
  префикса модуля). Модульный файл — только для своего; нужен другой размер/отступ — модификатор поверх
  общего класса, а не копия. **Перед тем как писать свой шаблон контрола или ловить клавиши внутри элемента списка —
  `Docs/Internal/avalonia-pitfalls.md`**: неочевидное поведение Avalonia, каждое из которых уже стоило
  бага (стили Fluent на частях шаблона, `TextBox` без `PART_ScrollViewer`, `KeyBinding` на контейнере,
  срабатывающий раньше поля ввода в фокусе, наследуемый `ToolTip.ServiceEnabled`). Переходы кистей —
  только `core:CrossfadeBrushTransition`, никогда встроенный `BrushTransition` (§5 там же).
  **Картинки и память** — §6 там же, читать перед любой работой с `Bitmap`/`SKBitmap`: убранный с экрана
  `Bitmap` освобождать явно (пиксели нативные, сборщик их не видит), но только после ухода из дерева;
  `Bitmap` из пикселей — через `WriteableBitmap` (`Extensions.ToAvaloniaBitmap`), не конструктором с
  `IntPtr` (его `SKBitmap.Copy()` держит лишнюю копию до GC); большой JSON — не строкой (пул держит буфер в
  3× её размер); запись zip — не через `MemoryStream`. Скин целиком (`SkinPackager.Decompile`, 4k скайсферы)
  распаковывается только установкой и экспортом; списку — `ReadName`, скриншотам — `ReadPreviews`.
  Файлы данных приложения (скины, палитры) — только от `Core/AppPaths`
  (`Environment.ProcessPath`). Голый относительный путь считается от рабочего каталога, а не от exe
  (ярлык с другим «Рабочим каталогом» — и данные «пропали»), а `AppContext.BaseDirectory` в
  single-file-сборке `Deploy.ps1` указывает во временную папку распаковки. Наступали дважды — скины и
  палитры; разбор в шапке `AppPaths.cs`.
  Проверять правки UI без владельца — **`Tools/UiStand/`** (свой `README.md`): headless Avalonia на настоящих
  стилях из собранной `TweakerUI.dll` — снимки вкладок и общих блоков во всех состояниях с попиксельным
  сравнением до/после, сверка всех анимируемых переходов цвета, вкладка Colors Configurator с живой
  вьюмоделью, вкладки Scripts и Script Dev с живыми вьюмоделями над поддельной папкой игры (`scripts`, `scriptdev` — второй с настоящим `twllink`), замер памяти Skin
  Changer (`mem`) и утечки вьюшек при смене вкладок (`tabs`). Там же — что в
  headless не работает (часы анимаций) и как это обходить.
- **`LegacyDataConverter`** — отдельный консольный `.exe`, намеренно застрявший на .NET
  Framework 4.8.1 (единственное место, где ещё жив `BinaryFormatter`, удалённый из рантайма
  начиная с .NET 9). Конвертирует старые `.tasp`/`.askin2`/`.pltc` в новые форматы; вызывается
  основным приложением как внешний процесс.
- **`ASBridge`** (`asbridge.exe`) — нативный C++23/CMake субпроцесс, единственный, кто трогает
  Win32 (`WM_COPYDATA`, поиск окна игры). Держит именованный пайп с текстовым протоколом наружу.
  Собирается CMake-хуком из pre-build шага `TweakerUI.csproj`, в `.sln` не участвует.
- **`InjectHelper`** — обобщённый `CreateRemoteThread`-DLL-инжектор (x86 exe), заморожен на
  `vcxproj`, собирается тем же pre-build-хуком. **Запасной путь:** игру `TweakerPlugin` грузит себе
  сама из `engine\channels\`, а инжектор нужен только чтобы подсадить плагин в сессию игры, которая
  была запущена раньше его установки, — и только по явному согласию пользователя.
- **`TweakerPlugin`** — нативный x86 C++23/CMake DLL-плагин внутриигрового оверлея (см. отдельный
  раздел ниже). Собирается аналогично `ASBridge`, но **best-effort**: неудачная сборка (нет
  DirectX/Quest3D SDK, ImGui или Detours submodule) не ломает сборку остального решения.
- **`Installer`** — COM-интероп для ярлыков, собирается только полным MSBuild (не `dotnet build`).
- **`TweakerLuaLinker`** (`twllink.exe`) — компоновщик Lua-проектов для скриптов `TweakerPlugin`:
  модули и `require` на входе, один файл для `Scripts\` на выходе (`lua-engine-fix-roadmap.md` §9,
  Ф5). Разбор Lua — `Loretta.CodeAnalysis.Lua`. `net10.0` (не `-windows`), `Nullable=enable` —
  намеренные отступления от `Directory.Build.props`, обоснованы в `.csproj`. Публикуется Native AOT
  (`-p:TwPublish=aot`, по умолчанию) или, запасным путём, `PublishSingleFile` (`-p:TwPublish=single`);
  **из-под агента** AOT-линковка падает на `vswhere.exe`, пока не снята
  `NoDefaultCurrentDirectoryInExePath` (`env -u ...`). `Project/` — `twproject.json`, `Linking/` —
  граф `require`, строки модулей, бандл и карта (карта пишется **во время** вывода, а не считается),
  `Diagnostics/` — коды `TWxxxx` (номера не переиспользуются, пользовательская таблица —
  `Docs/scripting/authoring.md`), `Cli.cs` — контракт командной строки (коды возврата 0/1/2/3, `--json`,
  `--install`/`--game` через `AUDIOSURF_DIR`, `--new <template> <dir>`, `--api`; `--watch` убран — следить за папкой проекта дело `TweakerUI`). `Api/` —
  `api.json` (вшит; что есть в окружении скрипта), разбор `api-reference.md`, генератор `tw.def.lua`;
  `Linking/ApiChecker` — проверки TW03xx (песочница, имена `tw.*`, запись в `tw`, глобалы) по `api.json`;
  `Project/Scaffold` — шаблоны `--new` и папка `.twllink/` проекта (копии `tw.def.lua` и схемы
  манифеста, освежаются каждой сборкой). Пользовательская сторона — `Docs/scripting/authoring.md`;
  проект и решения — `lua-engine-fix-roadmap.md` §9 и §12, Ф5.
- Тесты (NUnit): `TweakerCore.Tests`, `QuickPlayerCore.Tests`, `AudiosurfInterface.Tests`,
  `TweakerLuaLinker.Tests` (последний гоняет бандлы через `minilua.exe` из сборки LuaJIT плагина и
  пропускает эти тесты, если плагин не собран; `ShippedProjectsTests` там же сверяет поставляемые
  собранные скрипты с их проектами).

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
dotnet test TweakerLuaLinker.Tests/TweakerLuaLinker.Tests.csproj

# один тест:
dotnet test TweakerCore.Tests/TweakerCore.Tests.csproj --filter "FullyQualifiedName~ClassName.MethodName"
```

Всё, что требует реально запущенной игры (bridge-подключение, оверлей end-to-end) — юнит-тестами
не покрывается и не может быть проверено в песочнице; это явно за пользователем (см.
«Текущее состояние» в `overview.md`).

## Деплой / дистрибуция

`Scripts/Deploy.ps1` — release-бандл: self-contained single-file win-x64 `dotnet publish`
`TweakerUI` + release-сборка `LegacyDataConverter` (IL-merged через `ILRepack.Lib.MSBuild.Task`) +
`asbridge.exe`/`InjectHelper.exe`/(опционально) папка `PluginPayload\` с плагином, подобранные из-под
`TweakerUI/bin` после pre-build хуков — всё в `/distr/TweakerUI/`.

`Scripts/DeployDebug.ps1` — то же самое, но multi-file publish с PDB (managed и нативные
SkiaSharp/HarfBuzzSharp), чтобы можно было приаттачиться дебаггером к `TweakerUI.exe`.
`LegacyDataConverter` в обоих скриптах всегда собирается Release (коллизия сборок при Debug —
см. комментарий в `DeployDebug.ps1`).

**`PluginPayload\` копируется целиком, а не пересобирается по файлу.** Раскладка внутри неё значащая:
`PluginInstallation` (`TweakerCore/PluginInstall/`) опознаёт каждый файл по пути относительно корня
поставки, поэтому DLL обязана лежать в `channels\`, скрипты — в `TweakerStuff\Scripts\`, а пакеты неба —
в `TweakerStuff\SkyboxReplacer\Skyboxes\`. Раскладку и **состав** задаёт ровно одно место — target
`CopyTweakerPlugin` в `TweakerUI.csproj`; скрипты деплоя только переносят готовое. Поставка, собранная
руками мимо этого правила, ставится «успешно», но мимо игры — см. `Docs/Internal/plugin-offline-mode.md`,
«Ф5: правка по итогам первой проверки».

Источников у поставки два: `TweakerPlugin/assets/scripts/` (Lua) и `Skyboxes/` (пакеты `.sky`, отдельно от
`TweakerPlugin/assets` — те вшиваются в DLL ресурсами, эти остаются файлами, которые пользователь может
открыть и править). Правило отбора одно на оба: **подпапка `dev/` в бандл не попадает** — там пробы API и
инструменты разработки. Игровую папку `Scripts\` плагин сканирует нерекурсивно, поэтому пробу, нужную в
игре, кладут туда плоско руками; она остаётся файлом пользователя и в манифест не попадает.

**Скрипты-проекты.** Часть скриптов в `assets/scripts/` не пишется руками, а собирается `twllink` из
проекта в `TweakerPlugin/assets/script-projects/<sub>/<проект>/` в `assets/scripts/<sub>/` — сейчас
`puzzlepro_hud` и `dev/map_probe`. Собранный `.lua` и его `.lua.map` лежат в репозитории, поставка
берёт оба (`CopyTweakerPlugin`). **Правится проект, не собранный файл**: после правки модуля
`twllink --project <проект> --out <assets/scripts/...>`; забытая пересборка роняет
`TweakerLuaLinker.Tests` (`ShippedProjectsTests`). `twllink.exe` (Native AOT) публикуется всеми
четырьмя скриптами деплоя в `/distr/TweakerUI/DevTools/Binaries/`, в `PluginPayload\` не входит. В
дев-сборке его туда же кладёт `dotnet build` самого `TweakerUI` — обычной сборкой по `ProjectReference`
(target `CopyTwllink`).

Скрипты есть в двух видах и поддерживаются оба: `.ps1` (PowerShell) и `.sh` (Git Bash). Правишь один —
правь парный, иначе они разъезжаются молча.

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

**x86 DLL** для Audiosurf (32-bit процесс). Стек: **C++23**, Win32 API, **DirectX 9**
(June 2010 SDK), **Microsoft Detours**, **Quest3D SDK**, **ImGui**, **LunaSVG** (+ её сабмодуль
plutovg). Сборка: CMake + Ninja, MSVC, PCH.

**Как он оказывается в игре и что из этого следует** (подробно — `plugin-offline-mode.md`):

- Лежит в `engine\channels\` и загружается **самой игрой** при старте, до создания устройства D3D9.
  Инжект — запасной путь. Значит, код исполняется в двух разных режимах, и оба надо держать в голове.
- **Запуск по этапам.** `DllMain` только зовёт `on_process_attach` (`src/plugin/load.cxx`): резолв
  путей, проверка `QuestViewer.exe` и файла `DISABLE`, захват мьютекса присутствия, выбор режима,
  ранние хуки — и поток запуска. Под loader lock нельзя ждать, трогать оконные функции и
  приостанавливать потоки (§4.2); Detours-транзакции в ранней фазе идут **без** приостановки потоков.
- **Все пути — от `QuestViewer.exe`**, через `src/plugin/paths.{hxx,cxx}`: `engine\TweakerStuff\`
  (`Config\`, `Scripts\`, `SkyboxReplacer\`, `Logs\`). Ни рабочий каталог, ни положение самой DLL
  опорой быть не могут — игра меняет первый, а вторая лежит в разных местах в разных режимах.
- **Правило рендер-потока** (§4.5): запрет на системные вызовы касается **покадрового** кода.
  Инициализация на рендер-потоке — создание устройства, `bind_device`, `Reset` — под него не
  попадает, там `SetEvent` и запись в лог разрешены.
- **Лог жизненного цикла** `TW_BOOT_LOG` → `TweakerStuff\Logs\TweakerPlugin.log` включён и в Release:
  только kernel32, форматирование в буфер на стеке. Это единственный способ понять, что произошло у
  пользователя, которому плагин уронил игру ещё до появления окна.
- **Без хоста плагин полностью работоспособен**: небо, скрипты, оверлей. Хост нужен вкладкам Skins,
  Tweaks и Player — без него они заглушки, и висит пин Offline.

```
dllmain.cxx      — только DisableThreadLibraryCalls + plugin::on_process_attach
src/engine/       — движок Quest3D как объект, НЕ часть скриптового слоя (им пользуется и music, и
                    в перспективе skybox): engine_symbols (экспорты HighPoly.dll, один резолв; два
                    яруса — спина кадра обязательна, группы могут не резолвнуться и тогда деградирует
                    только реестр), engine_control (детур на EngineControl::EngineLoop — ЕДИНСТВЕННАЯ
                    покадровая точка входа в граф; оттуда же EngineInterface* на первом кадре,
                    личность объекта сверяется с экспортированным ??_7EngineControl@@6B@; подписчики
                    до/после графа публикуются через framework::ready), engine_frame (номер кадра
                    движка, dt, GetTreeCalculateCount), engine_groups (реестр загруженных групп:
                    ОДНО сравнение счётчика за кадр, пересборка только на изменении; ПОКОЛЕНИЕ на
                    группу — адреса переиспользуются, сравнение указателей не годится; детуры на
                    A3d_ChannelGroup::Release и EngineInterface::DeleteChannelGroup, где ДО оригинала
                    снимаются vtable-копии channel_shim — иначе деструктор канала пойдёт по копии,
                    которая его переживёт), engine_state (жизненный цикл detached → booting →
                    starting → ready ⇄ busy; признак — стартовая группа движка, а НЕ значение из
                    канала: .cgr хранит то, что оставил автор в редакторе, и StartupState врёт
                    правдоподобно. ЕДИНСТВЕННЫЙ ответ на «можно ли сейчас» — скриптовые колбэки до
                    ready не зовутся вовсе). Слой каналов (Ф3, бывший lua/lua_channels): channel_ref
                    (канал + группа + поколение + семейство + CHIC; резолв, valid(), live(),
                    bust_memo), channel_kind (baseguid → семейство — ЕДИНСТВЕННОЕ место, знающее
                    все семейства), channel_vtable (только арифметика слотов и is_code — НИ ОДНОГО
                    знания о том, что в каком слоте), family/fam_number, fam_text, fam_vector,
                    fam_matrix, fam_table — каждый файл владеет смыслом СВОИХ слотов и описывает,
                    чем грозит вызов этого слота на чужом семействе; функции семейств принимают
                    только channel_ref и отказывают ref'у чужого семейства одним сравнением. Новое
                    семейство — новый файл. См. reversing-journal-boot.md §1, §2.4, §6, §7 и
                    lua-engine-fix-roadmap.md
src/framework/    — хуки (Detours, D3D9, dinput8 + texture_hook на
                    Aco_DX8_Texture::LoadTextureFromMemory), channel_shim (перехват вызова
                    ОДНОГО канала подменой vptr на копию vtable; подписчиков на канал может быть
                    НЕСКОЛЬКО — идут по ключу порядка (место скрипта, не момент подписки), все before
                    отрабатывают до отмены, отмена это ИЛИ, у подавленного
                    вызова нет after; оригинальная vtable возвращается с уходом последнего
                    подписчика; группа канала запоминается ПРИ ПОДПИСКЕ, чтобы remove_all_in(group) на выгрузке
                    не спрашивал ничего у разрушаемого объекта — см. lua-scripting.md §8.5 и Ф4), d3d9_state (`state_scope`:
                    сохранение и откат РОВНО того состояния устройства, которое хук тронул — старое
                    значение читается перед записью нового, поэтому список «что ставим» и список
                    «что возвращаем» физически один и разойтись не могут; запись, ничего не
                    меняющая, не пишется и не занимает места; вложенные scope откатываются к
                    состоянию вызвавшего; покрывает и то, чего не умеет ни один state block — render
                    target, depth-stencil, SetSoftwareVertexProcessing. Пришёл на смену
                    `CreateStateBlock(D3DSBT_ALL)`: замерено 10.9 мкс против 0.85 мкс на перехват,
                    harness/state), wndproc_hub (общая точка
                    подписки на WndProc игры: IPC, D3D9 WM_ACTIVATEAPP, будущий ImGui-инпут).
                    vtable в d3d9_hooks/dinput8_hooks/channel_shim разбираются вручную; план
                    обобщить под это engine/channel_vtable — lua-engine-fix-roadmap.md §17.1
src/ipc/          — overlay_ipc: разбор/сборка L3-протокола TW_OVL (см. overlay-protocol.md);
                    операции с префиксом QP_ он не разбирает, а форвардит в src/ui/qp/, а SCRIPT_ — в
                    src/lua/lua_remote (overlay-scripts.md)
src/ui/           — overlay_state (кэш состояния, generation-counter, lock-free read по try_lock),
                    pending_actions (optimistic UI + таймаут-подтверждение для reverse-sync),
                    wire_text (общий percent-кодек L3), ui_main/theme, fonts (реестр запечённых
                    начертаний; в ui/, а не в imgui_backend, потому что smoke_test строит свой
                    контекст и обязан получить те же фейсы в том же порядке — индекс фейса часть
                    скриптового ABI), gpu_texture (единственная
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
                    D3DCAPS9/BehaviorFlags), sky_abi (какие регистры пишет ДВИЖОК, а не автор:
                    c5 время, c208-c210 музыка, c208-c223 зарезервированы целиком — низ файла
                    констант роздан ручкам вручную и столкновение там неизбежно). См.
                    skybox-replacer.md и skybox-procedural.md; sky_math существует затем, чтобы
                    не линковать d3dx9.
                    Пакет `.sky` — sky_package (манифест Config.json через yyjson), sky_vfs
                    (чтение пакета: каталог ИЛИ zip через libminiz, плюс ограничение имён —
                    ЕДИНСТВЕННОЕ место, где решается, что имя из скрипта/манифеста читать можно;
                    locate() у архива ВСЕГДА пусто, кто трактует это как ошибку — сломан на zip),
                    sky_shared (общий блок: свет, значения, привязки), sky_lua (своя VM для
                    place()/fill() — отдельная от src/lua/, у которой другая задача), sky_image
                    (float RGBA слои и операции), sky_texture (разбор .dds и создание 2D/volume/cube
                    ИЗ БАЙТОВ — D3DX не годится, каждый его загрузчик берёт путь, а внутри zip путей
                    нет; форму говорит сам файл, манифест её не объявляет), sky_sprites +
                    sky_sprite_atlas (геометрический слой). Текстуры слоя живут в кэше sky_shader
                    вместе с шейдерами: один жизненный цикл, одна инвалидация, один release.
                    См. skybox-geometry.md и skybox-replacer-roadmap.md
src/lua/          — LuaJIT-скриптинг поверх src/engine/ (своего доступа к графу у него больше нет),
                    разрезан по lua-engine-fix-roadmap.md §3.3 (Ф4); карта кусков — в шапке
                    lua_host.hxx. lua_host (фасад: подъём слоя, draw_frame под ОДНИМ ImGui
                    ErrorRecovery-guard'ом на весь диспетч), lua_vm (lua_State и
                    ЕДИНСТВЕННЫЙ список C-точек входа k_entry_points — ПО ИМЕНИ, не по позиции;
                    пролог сверяет имена в обе стороны и не стартует при расхождении), lua_prelude
                    (Lua-половина слоя текстом, шесть секций-чанков tw.abi/core/channels/hud/callbacks/
                    sandbox — песочница с Ф5b это ПОСЛЕДНЯЯ СЕКЦИЯ ПРОЛОГА, а не C; ljtest читает их
                    из этого файла; tw.api_version задан в tw.core и больше нигде, lua_vm читает его
                    оттуда; имена с "_" — приватные для пролога и в api.json не попадают; S.handle_types
                    / S.constructors / S.sandbox — только для инструментов), api/api_core|api_channels|api_hooks|api_hud
                    (extern "C" ABI, который скрипт зовёт через FFI — НЕ lua_CFunction, см.
                    lua-scripting.md §2.2; хендл канала в Lua — это channel_ref в куче, которым
                    владеет Lua-объект через ffi.gc → tw_ref_free), lua_registry (скрипты из
                    Scripts\, id = индекс, вкл/выкл/перезагрузка; ПОРЯДОК ВЫПОЛНЕНИЯ — отдельно от id, задаёт
                    только пользователь (перетаскиванием, строки `order=` в scripts.cfg), по нему идут тела на
                    старте, все списки пролога и подписчики канала; reload/переключение место не меняют —
                    script-coexistence-roadmap.md §3; скрипты в куче, в векторе — unique_ptr,
                    поэтому script* живёт весь сеанс; rescan() только дописывает в конец, пропавший файл —
                    запись `missing`, номера не сдвигаются; reload НЕ пишет scripts.cfg; revision() —
                    счётчик изменений для хоста; метаданные из
                    `-- @name/@author/@version/@description/@api` читаются БЕЗ запуска файла; скрипт с
                    @api больше tw.api_version не запускается; .lua.map грузится на каждом запуске), lua_script
                    (один скрипт + его здоровье), lua_sched (ЕДИНИЦА ОТКАЗА — СКРИПТ: каждый колбэк
                    под своим xpcall в прологе, учёт в C через tw_script_enter/leave; 5 ошибок за 300
                    кадров или >4 мс/кадр 120 кадров подряд → приостановка, у приостановленного
                    мьюты сняты; колбэки скриптов НЕ зовутся, пока engine_state не скажет ready),
                    lua_diag (дедуплицирующий приёмник: запись на (скрипт, место, форма), ошибки в
                    ленту раз и только в ready, лимит ленты на скрипт; позиции собранного скрипта
                    переписывает в позиции его модулей ДО ключа записи), lua_linemap (.lua.map от
                    twllink: разбор, сверка хэша бандла — карта рядом с поправленным руками скриптом
                    отвергается, а не врёт; грузится на каждом запуске скрипта в lua_registry), lua_config (какие
                    скрипты выключены, TweakerStuff\Config\scripts.cfg — только исключения, порядок `order=`, плюс
                    engine.settle_frames / engine.settle_ms — окно отстоя для engine_state, и save()
                    их сохраняет), lua_ui (вкладка Scripts, регистрируется через
                    menu::add_extra_tab, как и Skybox; шапка показывает состояние слоя, строка скрипта —
                    чип состояния, стоимость, хуки/ожидания, сообщения, Resume; строки в порядке выполнения,
                    перетаскиваются за ручку слева; кнопка Rescan; длинное
                    описание — «...» и подсказка), lua_remote (SCRIPT_* от хоста: разбор на IPC-потоке
                    только кладёт команду в ящик, pump() в начале lua_host::draw_frame её исполняет и
                    шлёт SCRIPT_STATE по сдвигу registry::revision() — источники изменений, включая
                    приостановку в lua_sched, только двигают счётчик). Выключение скрипта = on_unload + снятие его подписок
                    и возврат оригинальных vtable, а не спящий хук; включение = повторный запуск
                    файла с диска, оно же горячая перезагрузка. Скрипты — loose-файлы
                    в engine\TweakerStuff\Scripts\, не ресурсы: их правят без пересборки; в бандл
                    их кладёт CopyTweakerPlugin (TweakerUI.csproj) как PluginPayload\TweakerStuff\
                    Scripts\, а в игру — PluginInstallation (см. раздел «Деплой»)
src/plugin/       — load (этапы 0-3 запуска, оба режима загрузки), paths (вся раскладка от
                    QuestViewer.exe), presence (мьютекс «я здесь» + событие Ready для хоста),
                    boot_log (TW_BOOT_LOG, живёт и в Release), глобальное состояние, Quest3D state,
                    music (что играет прямо сейчас: читает `VisMusic` через engine/channel_ref, отдаёт
                    наружу низ/середину/верх/воздух, громкость, онсеты — НЕ «полосу k», см.
                    reversing-journal-gameplay.md §10)
src/resource/     — .rc-based упаковка ассетов (шрифты/текстуры/SVG/шейдеры) прямо в DLL;
                    assets/shaders/*.hlsl при этом компилируются fxc на этапе сборки, и вшивается
                    только байткод (TW_SHADER). Профиль берётся из имени: *.vs.hlsl / *.ps.hlsl;
                    *.hlsli — общие заголовки, вшиваются как TW_TEXT (не программы, глоб шейдеров
                    их не берёт), чтобы пользовательский .hlsl мог их #include без копии на диске
src/libtweeny, libstb, libuulog, libyyjson, libminiz — vendored (Tween-анимации, stb_image +
                    stb_image_resize2, лог, JSON-читалка манифеста, чтение zip) — не трогать стиль.
                    У libminiz дефайны MINIZ_NO_* объявлены PUBLIC намеренно: часть из них меняет
                    раскладку структур в miniz.h, и потребитель без них разойдётся с библиотекой
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

**Оффлайн-харнессы** — `TweakerPlugin/harness/`, вне CMake, свой `README.md`. Каждый
линкует настоящий `.cxx` плагина и работает без игры и без девайса; `harness\run.bat` собирает и
прогоняет сюиты с проверками, `run.bat build` — вдобавок инструменты (рендер превью, замеры
распределения спрайтов, дампы). Правишь `sky_*` — прогони. Скриптовый слой прогоняется там же: `harness/lua/` (`ljtest` — пролог
из `lua_prelude.cxx` против заглушек C-ABI, отданных по имени, и все поставляемые скрипты; заглушка
`tw_script_leave` считает любую ошибку скрипта провалом — после Ф4 ошибка не вылетает из
диспетчера, и иначе сломанный скрипт проходил бы молча; `shimtest` — настоящий
`channel_shim.cxx` плюс проверка соглашения о вызове `SetVector`; `arraytest` — настоящий
`engine/family/fam_table.cxx` против поддельных каналов; `famtest` — все семейства, по секции на
каждое, с мутационной проверкой; `TW_LJTEST_TRACE=<файл>` пишет каждый вызов HUD-заглушки `ljtest` в
файл — так бандл сверяется с исходником побайтно, см. `lua-engine-fix-roadmap.md` §12, Ф5). Слой движка — `harness/lifecycle/lifetest`:
настоящие `engine_groups.cxx`, `engine_state.cxx` и `channel_shim.cxx` против поддельного движка, а
заглушка `detour::attach` **запоминает адрес хук-функции**, поэтому «игра выгрузила группу» в сюите —
это вызов настоящего тела хука. Планировщик — `harness/sched/schedtest`: настоящие `lua_sched.cxx`,
`lua_diag.cxx`, `lua_script.cxx` против заглушек (бюджеты ошибок и времени — последнее настоящим
временем, ~1 с; дедупликация, маршруты в ленту; `lua_linemap.cxx` и путь карты через `diag::report`). Реестр глазами хоста — `harness/remote/remotetest`:
настоящие `lua_registry.cxx`, `lua_remote.cxx`, `lua_script.cxx` над настоящей временной папкой (rescan,
стабильные номера и адреса, порядок выполнения, ящик `SCRIPT_*`, что и когда уходит хосту), без `luajit.lib`. Правишь `src/lua/*`, `src/engine/*` или `assets/scripts/*` — прогони. Часть требует собранного плагина: они
линкуются с `build/x86-release/luajit.lib`.

**`TweakerPlugin/harness/` при этом в `.gitignore`** (строка 12) — каталог не в репозитории, правки
в нём локальные и в коммит не попадают. Учитывать, когда добавляешь туда сюиту: её не будет у
следующего, кто склонирует репозиторий.

- Корневой namespace: `tw::`, подпространства по слоям: `tw::plugin`, `tw::framework`, `tw::engine`, `tw::ui`.
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

Hot-path: `EndScene`, `Present`, `EngineControl::EngineLoop` (спина кадра, `src/engine/engine_control.cxx`),
перехваченный `CallChannel` конкретного канала, любой код, вызываемый каждый кадр (включая `overlay_state`
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

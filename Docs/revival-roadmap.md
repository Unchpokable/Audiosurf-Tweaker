# Audiosurf Tweaker — план возрождения проекта

## Контекст

Audiosurf Tweaker — старый студенческий проект (WPF, .NET Framework 4.8, 12 проектов в решении) для управления скинами/цветами/твиками игры Audiosurf. Аудит показал рабочее ядро (Skin Changer через файловую замену текстур, live-твики через `WM_COPYDATA` IPC с процессом игры `QuestViewer`, Server Swapper через собственный DSL-интерпретатор), но вместе с ним — заброшенные заготовки (`DiscordRichPresence`, недописанный `QuickPlayerCore`), мёртвый билд-мусор (`TweakerOverlay`, `MigrationBackup`, `mmap`), формат скинов на небезопасном/умирающем `BinaryFormatter`, и россыпь мелких багов.

Цель: превратить проект в поддерживаемую, современную кодовую базу — .NET 10 + AvaloniaUI + CommunityToolkit.Mvvm — не потеряв рабочую функциональность и дав пользователям путь миграции их существующих скин-библиотек.

**Принятые решения (раунд 1):**
- UI: полный переезд с WPF на **AvaloniaUI + CommunityToolkit.Mvvm**.
- Порядок: сначала **чистка и стабилизация текущего .NET Framework кода**, потом миграция платформы.
- Формат скинов: **zip-контейнер + JSON-манифест + PNG-файлы внутри**, вместо `BinaryFormatter`.
- `QuickPlayer` (плейлисты + запуск песен через IPC игры) — в дорожной карте, но **финальной фазой**, после стабилизации ядра. `TweakerOverlay` — **полностью исключён** из этой дорожной карты (см. ниже).

**Принятые решения (раунд 2):**
- `mmap/` — удалить. Это заброшенный альтернативный инжектор на manual DLL mapping, отменён как избыточное усложнение: проект не читерский инструмент, прятаться от системы незачем, `CreateRemoteThread` — нормальный подход.
- `SkinEditorTool` (WinForms) — просто выкинуть as-is, без архивации "на референс". Полный ревард редактора скинов на Avalonia + .NET 10 — отдельная задача крайне далёкой перспективы, вне этой дорожной карты.
- Легаси-конвертация `.tasp`/`.askin2` — **не** встраивается адаптером внутрь основного приложения. Выносится в отдельный CLI-инструмент на .NET Framework 4.8, который Tweaker (уже на .NET 10) вызывает как внешний процесс, если файл не читается в новом формате. Так основной Tweaker не тянет за собой зависимость от старого рантайма — конвертер опционален и на машинах без .NET Framework просто не будет работать, не ломая сам Tweaker. *(Уточнено в раунде 4: инструмент один на все легаси-форматы, не только скины — см. ниже.)*
- Переименования и слияния: `ChangerAPI` → **`TweakerCore`**; `ASCommander` → **`AudiosurfInterface`**; `textpackctrl` (реально — проверка дрейфа папки текстур по SHA-256, namespace `FolderChecker`) сливается **внутрь `TweakerCore`** как внутренний модуль/namespace, отдельным проектом в решении больше не является.
- **Live IPC с игрой полностью переезжает на нативный C++ субпроцесс** — `asbridge.exe` — с именованным пайпом и текстовым протоколом до managed-стороны (`AudiosurfInterface` становится тонким pipe-клиентом). Обоснование и детали — в Фазе 3.
- `TweakerOverlay`/`InternalOverlayRenderer` — тема закрыта для этой дорожной карты. Причина: нынешний рендер-хук построен на костыле (игра сама себе шлёт `WM_COPYDATA`), плюс баги UI из-за особенностей DX9-окна Audiosurf. Нормальная реализация требует отдельного реверс-инжиниринга рендеринга игры — когда до этого дойдут руки, тема поднимается заново отдельным планом. Существующий код `InternalOverlayRenderer`/`InjectHelper` не трогаем и не мигрируем, оставляем замороженным как есть.

**Принятые решения (раунд 3):**
- C++ больше не живёт на MSBuild/vcxproj — новые нативные модули (сейчас это `asbridge`) собираются **CMake**. MSBuild дёргает CMake как чёрный ящик из pre-build шага, без генерации VS-проектов и без записи в `.sln` — при работе над `asbridge` он открывается/дебажится как отдельная CMake-папка (VS "Open Folder"/CLion), минимальная связанность с `.sln`.
- Замороженные `InjectHelper`/`InternalOverlayRenderer` остаются как есть на своих `vcxproj` — они часть замороженного оверлей-функционала целиком, под CMake-конвейер не переводятся, пока (если) оверлей не будет разморожен отдельным треком.

**Принятые решения (раунд 4 — аудит `BinaryFormatter` по всему проекту):**

Полный проход по проекту (`BinaryFormatter`/`IFormatter`/`[Serializable]`) нашёл 4 разных сценария использования, а не только скины. Делятся на две принципиально разные по риску категории:

- **Пользовательские данные (нужна настоящая миграция, терять нельзя):**
  1. Скины `.tasp`/`.askin2` (`SkinPackager`, `AudiosurfSkin`/`AudiosurfSkinExtended`) — уже был в плане.
  2. Цветовые пресеты `.pltc`/`.palette` (`PaletteDynamicLoadContainer`, `ColorPalette.Save/Load`, `ColorPalettePrint`) — **пропущено в исходном плане**. Судя по README ("сохранить и поделиться цветовыми пресетами"), это такие же шареable пользовательские файлы, как скины, просто раньше не были в фокусе аудита.
- **Локальный одноразовый кэш (просто меняем формат, старый файл при несовпадении молча игнорируется/пересоздаётся, миграция не нужна):**
  3. `FolderHashInfo` (`.hinf`, сейчас `textpackctrl`, переезжает в `TweakerCore` — раунд 2) — служебная проверка "не трогал ли юзер папку текстур руками".
  4. `LoadingCache` (`load.cache`, `SkinChangerRestyle`) — кэш превьюшек скинов для UI, к тому же завязан на `System.Drawing.Bitmap`.

Решение по конвертеру: **один инструмент на все легаси-форматы**, не по одному на каждый — переименовывается в **`LegacyDataConverter.exe`**, конвертирует и скины, и палитры (по пути/типу файла на входе). Обоснование: маловероятно, что у пользователя часть библиотеки конвертирована, а часть — нет; удобнее гонять один бинарник один раз.

Также нашли: `AudiosurfSkin.DeepClone()`/`AudiosurfSkinExtended.DeepClone()` используют `BinaryFormatter` не для файла, а как трюк "сериализуй в память и обратно = глубокая копия" (в отличие от `Clone()`, который просто копирует ссылки, а не данные). Это тоже отвалится и требует настоящей ручной реализации глубокого копирования — делать это одновременно с переписыванием модели скина в Фазе 2, а не отдельным заходом.

---

## Общий архитектурный принцип для platform-specific кода

Договорились не городить одну и ту же тяжёлую схему везде:

- **Субпроцесс + именованный пайп** — там, где код реально общается с посторонним процессом (игрой) и может неожиданно упасть/зависнуть. Даёт изоляцию сбоя от UI-процесса Tweaker-а, managed-сторона получает только тривиальный `NamedPipeClientStream` без единого P/Invoke.
- **Обычный P/Invoke/COM-интероп прямо в коде** — для одноразовых системных вызовов из собственного процесса (например COM shell-ссылка/регистрация иконки в `Installer`). Заворачивать это в субпроцесс с пайпом было бы избыточным усложнением.

---

## Сборка нативных (C++) модулей: CMake + MSBuild pre-build hook

- Активно разрабатываемые C++ модули (сейчас — `asbridge`, в будущем — что угодно нативное) собираются **CMake**, не MSBuild/vcxproj. Новых `vcxproj` для них не заводится.
- Оркестрация: у managed-сборки (MSBuild, скорее всего общий `Directory.Build.targets`/pre-build target в `SkinChangerRestyle.csproj`) есть шаг, который на этапе сборки .NET-проекта вызывает `cmake --build` для нативных модулей и проверяет exit code. Интеграция — чисто внешний вызов, никакой генерации VS-проектов из CMake и никакой привязки к `.sln`.
- **Windows:** если сборка `asbridge` (или другого активного Live-IPC модуля) падает — это фейл сборки всего решения наравне с ошибкой компиляции C#. Молча пропустить и собрать остальное — не вариант.
- **Linux (на перспективу):** нативный Live-IPC модуль условно исключается из сборки — не пытаемся собрать и не считаем это фейлом. Целевая игра под Linux работает только через Wine/Proton, и способа нативному Linux-процессу доставить `WM_COPYDATA`/`WNDPROC` внутрь Wine-хостящего игру процесса на сегодня не просматривается — это не "пока не реализовано", а вероятно нереализуемо в принципе, поэтому pre-build шаг должен это именно пропускать по платформе, а не пытаться и падать.
- Собранный `asbridge.exe` кладётся в выходную директорию managed-сборки по тому же принципу, что уже сегодня используется для `Plugins\InjectHelper.exe`/`Plugins\InternalOverlayRenderer.dll`.
- Замороженные `InjectHelper`/`InternalOverlayRenderer` в этот конвейер не включаются (см. решения раунда 3 выше) — остаются на `vcxproj`, собираются (если вообще собираются) по-старому, отдельно от новой CMake-цепочки.

---

## Фаза 0 — Гигиена репозитория

**Статус: ✅ Выполнено**

- Удалить `DiscordRichPresence` целиком (проект, ссылка в `.sln`, неиспользуемые NuGet-пакеты). Нефункционален, нигде не подключён.
- Удалить `MigrationBackup/` — мусор от старой NuGet-миграции.
- Удалить `mmap/` — заброшенный manual-mapping инжектор, решение отменить зафиксировано.
- Удалить папку `TweakerOverlay/` (сталые `obj`/`pdb`/`.tlog` от переименованного предка `InternalOverlayRenderer`, исходников там уже нет).
- Удалить `SkinEditorTool` целиком (WinForms, не входит в `.sln`) — без архивации, полный ревард когда-нибудь позже отдельным треком.
- Почистить `.csproj`-ссылки, которые никуда не ведут после удалений.

**Проверка:** решение собирается после удалений, `.sln` не содержит висячих ссылок.

---

## Фаза 1 — Стабилизация текущей кодовой базы (ещё на .NET Framework 4.8)

**Статус: ✅ Выполнено**

Известные из аудита конкретные дефекты для разбора и фикса:
- `SkinChangerRestyle/Core/AudiosurfConfigurationPresenter.cs` — indexer-сеттер безусловно бросает исключение после апдейта.
- `ChangerAPI`/`SkinPackager._texturesNames` — дублирующаяся запись `"cliff2-1.png"`, отсутствие расширения у `"ring2B"` — сверить с `Docs/texturing.md` и исправить маску файлов.
- `SkinPackager.Compile`/`Decompile` глотают все исключения молча — завести единый паттерн логирования вместо тихого `catch { return false/null; }`.
- `SkinChangerViewModel.InstallSkinInternal` — лишний `unsafe`/`fixed (char* ...)`; убрать, отключить `AllowUnsafeBlocks`, если больше нигде не нужен.
- `textpackctrl.Test/EnvironmentalChecker_Test.cs` — захардкожен персональный путь; переписать на `Path.GetTempPath()`/`TestContext` temp-каталоги.

Системная работа:
- Единый механизм логирования + глобальный обработчик необработанных исключений (`AppDomain.UnhandledException`/`DispatcherUnhandledException`) — прямой ответ на "непредсказуемые креши".
- Пройтись по кодовой базе на паттерн "silent catch" (как в `SkinPackager`) и исправить по месту.
- Расширить `TweakerScriptsInterpreter.Tests` (сейчас 2 теста) — некорректный синтаксис, отсутствующий `END-SECTION`, неверное число параметров у команд.

*Примечание:* фикс `ASCommander`-специфичного бага с `Win32Exception`/фейковым `TimeSpan` из первой версии плана убран отсюда — этот код целиком выбрасывается в Фазе 3 при переезде live-IPC на `asbridge`, чинить то, что скоро удаляем, не нужно.

**Проверка:** ручной прогон Skin Changer / Color Configurator / Server Swapper / Tweaker на текущей WPF-версии, юнит-тесты зелёные, логирование ловит намеренно спровоцированную ошибку.

---

## Фаза 2 — Новый формат скинов и палитр + единый легаси-конвертер

**Статус: ✅ Выполнено** (проверено на реальных скинах пользователя — `Nano.tasp` 55MB, `Vintage Works.askin2` 2.9MB — оба чисто конвертировались, распакованы 7Zip и вручную сверены, текстуры целые; `audiosurftweaker.exe` на новом коде запускается и работает).

Причина делать это *до* переезда на .NET 10: начиная с .NET 9 `BinaryFormatter` полностью удалён из рантайма — старый код на новом рантайме не заработает в принципе. Конвертацию нужно готовить, пока проект ещё на .NET Framework. Затрагивает оба сценария с пользовательскими данными (см. раунд 4 решений выше) — скины и цветовые пресеты.

**Новый формат скинов** (в `ChangerAPI`, переименование в `TweakerCore` — отдельная задача Фазы 3):
- `.tasp`/`.askin2` — zip-архив с `manifest.json` (имя, `UID`, `formatVersion`, список текстурных групп → имена файлов внутри архива) и PNG/JPG текстурами как обычными файлами архива.
- Публичное API `SkinPackager` (`Compile`/`CompileToPath`/`CompileToFile`/`RewriteCompile`/`Decompile`) **не переименовывалось** — по факту реализации оказалось проще и безопаснее поменять реализацию этих методов местами, чем городить параллельные V2-методы: ни один из ~9 вызывающих мест в `SkinChangerRestyle` трогать не пришлось. `Decompile` сам определяет по сигнатуре файла (zip magic bytes), новый это формат или легаси, и сам вызывает конвертер при необходимости.
- `AudiosurfSkin.DeepClone()`/`AudiosurfSkinExtended.DeepClone()` переписаны на ручной глубокий копир объектного графа (`ImageGroup`/`NamedBitmap` получили собственные `DeepClone()`).

**Новый формат цветовых пресетов** (`PaletteDynamicLoadContainer`, `ColorPalette`, `ColorPalettePrint` — в `SkinChangerRestyle`):
- `System.Text.Json` напрямую в `.pltc`/`.palette`, без zip-обёртки (данные тривиальные).
- Обнаружение легаси-файла — по первому непробельному символу (`{` = JSON, иначе бинарник → вызов конвертера).

**`LegacyDataConverter.exe` — отдельный проект, .NET Framework 4.8.1:**
- Один консольный инструмент на оба легаси-формата, с «замороженными» копиями старых типов (`Legacy/*.cs`) под собственным `SerializationBinder` — types резолвятся в свою же сборку независимо от того, как называлась и версионировалась исходная `ChangerAPI`/`SkinChangerRestyle` на момент записи файла.
  - Важный найденный нюанс: у generic-типов (`List<NamedBitmap>`) assembly-квалификация типа-параметра зашита прямо в строку `typeName`, которую получает `BindToType` — наивный `assemblyName == "ChangerAPI"` чек её не ловит. Решение — `Regex.Replace` всех embedded `ChangerAPI, Version=...`/`SkinChangerRestyle, Version=...` ссылок на своё имя сборки перед резолвом.
- Вход: путь к файлу (тип по расширению: `.tasp`/`.askin2` → скин, `.pltc`/`.palette` → пресет). Пишет новый формат собственным (независимым от `ChangerAPI.SkinPackager`) writer'ом — `LegacyDataConverter` не зависит от живого `ChangerAPI`, чтобы не сломаться, когда тот эволюционирует дальше в Фазах 3/4.
- Основной Tweaker при обнаружении нечитаемого в новом формате файла ищет конвертер рядом с собой (не в `Plugins\` — конвертер служебная утилита, а не опциональный плагин вроде инжектора/оверлея), вызывает как внешний процесс, при успехе перечитывает уже сконвертированный файл.
- **Деплой-нюанс, вскрытый на практике:** `LegacyDataConverter.exe` тянет `System.Text.Json` и его транзитивные зависимости (~10 DLL: `System.Buffers`, `System.Memory`, `System.Text.Encodings.Web` и т.д.) — при упаковке нужно копировать **всю** папку сборки конвертера, не только сам `.exe`. Без соседних DLL процесс падает при старте, и `LegacyConverter.TryConvert` в `ChangerAPI` тихо возвращает `false`.

**Тесты:** новый проект `ChangerAPI.Tests` (NUnit) — round-trip компиляции/декомпиляции нового формата, `DeepClone` на независимость битмапов, поведение на не-скин-файлах. Плюс сквозная ручная проверка легаси-конвертации: синтетический фикстур и оба реальных пользовательских скина — все прошли, включая проверку валидности итогового zip (`testzip`) и содержимого `manifest.json`.

**Проверка:** старые `.tasp`/`.askin2` и `.pltc`/`.palette` из библиотеки пользователя успешно конвертируются `LegacyDataConverter.exe` в новые форматы; round-trip обоих новых форматов (Compile→Decompile / Save→Load) даёт побитово идентичные данные; поведение основного приложения при отсутствии конвертера — предсказуемая ошибка, а не краш.

---

## Фаза 3 — Миграция платформы: .NET Framework 4.8 → .NET 10

**Перед началом фазы:** заглянуть в тег `archive/beta-alt-design-updater` (архивная точка бывшей ветки `Beta-Alt-Design`, удалённой при уборке git — см. журнал работы) и оценить, в каком состоянии там недоделанный in-app автообновлятор (`GithubRepoReleaseProvider`, UI прогресса скачивания, ICommand-инфраструктура). Если что-то из этого стоит воскресить — переносить логику до переезда UI на Avalonia (Фаза 4), пока ещё есть смысл сверяться со старым WPF-кодом as reference. Всё, что в этой же ветке касалось стилизации оверлея — не смотреть, использовать незачем независимо от состояния автообновлятора.

Три параллельных потока работы внутри фазы:

**3a. Managed-проекты на SDK-style + net10.0-windows**

**Статус: ✅ Выполнено** (текущий заход — только 3a; 3b/3c не начаты)

**Итоги по факту реализации** (отклонения от плана и находки, вскрывшиеся только при реальной сборке):
- Переименование `ChangerAPI`→`TweakerCore`/`ASCommander`→`AudiosurfInterface`/слияние `textpackctrl`+`textpackctrl.Test` выполнено ровно по плану. `FolderChecker` (внутри `TweakerCore`) получил namespace `TweakerCore.FolderChecker`. `LegacyDataConverter` не тронут — проверено отдельной сборкой через `MSBuild.exe`, всё ещё компилируется на `v4.8.1`.
- **`LoadingCache` (см. раунд 4) пришлось чинить прямо в 3a, не дожидаясь 3b.** План относил его к 3b (вместе со сменой `Bitmap`→`SkiaSharp`), но на .NET 9+ обращение к `BinaryFormatter.Serialize/Deserialize` — не просто предупреждение, а `error SYSLIB0011` при компиляции (тип помечен `[Obsolete(..., error: true)]` в референс-сборках), т.е. это жёсткий блокер сборки, а не стилистический вопрос. Переписан на `System.Text.Json` с PNG→Base64 буферизацией через `MemoryStream` (тот же приём, что уже был в `SkinPackager`/`SkinWriter` в Фазе 2) — `Bitmap` как тип не тронут, обещание "SkiaSharp только в 3b" сдержано.
- `ToastContentBuilder.Show()` (`Microsoft.Toolkit.Uwp.Notifications`, UWP-toast уведомления) не резолвился на голом `net10.0-windows` — WinRT-проекция для `Show()` появляется только на TFM с версией Windows SDK. Решение: `SkinChangerRestyle` таргетирует `net10.0-windows10.0.19041.0` (а не просто `net10.0-windows`, как остальные проекты) — единственное отклонение от единого TFM, заданного в `Directory.Build.props`.
- XAML-компилятор .NET 10 (в отличие от Framework/`PresentationBuildTasks`) не терпит пустой `<Grid.ColumnDefinitions></Grid.ColumnDefinitions>` без дочерних элементов (`error MC3063`) — найден один такой мёртвый тег в `ColorConfiguratorView.xaml`, удалён (поведение не меняется, `Grid` и без него использует одну неявную колонку).
- Два места с мёртвым `protected X(SerializationInfo, StreamingContext)` конструктором на пользовательских `Exception`-наследниках (`TweakerScriptsInterpreter/Exceptions/*`) — та же ситуация, что уже поймали в Фазе 2 на `BadFileFormatException`: конструктор никогда не вызывался (эти типы не участвуют в кросс-доменной/файловой сериализации), только тянул `SYSLIB0051`-предупреждение. Убраны.
- `Installer` (`<COMReference>` на `IWshRuntimeLibrary` для ярлыков) собирается через SDK-style csproj без переписывания, но **только полным `MSBuild.exe` из Visual Studio** — `dotnet build` падает с `MSB4803`, так как `ResolveComReference`-таск недоступен в кроссплатформенном MSBuild из .NET SDK. Зафиксировать в мышечной памяти: `Installer` не собирается через `dotnet build`/`dotnet test`, только через VS/`MSBuild.exe`.
- `System.Configuration.ConfigurationManager`/`System.Drawing.Common` в итоге не понадобились как явные `PackageReference` в самом `SkinChangerRestyle.csproj` — на `UseWPF`+`UseWindowsForms` они приходят транзитивно (NuGet явно предупреждает `NU1510`, если пин оставить), в отличие от `TweakerCore`/`TweakerCore.Tests`, где `System.Drawing.Common` пришлось оставить явно (эти два проекта — чистые библиотеки без `UseWindowsForms`/`UseWPF`).
- В `SkinChangerRestyle` найдены и удалены мёртвые `Properties/Settings.settings`+`Settings.Designer.cs` — пустой (без единого поля) `ApplicationSettingsBase`-класс, нигде не используемый (`Properties.Settings`/`Settings.Default` не встречались в коде ни разу), а `ApplicationSettingsBase` не портирован в современный .NET — чинить сериализацию несуществующих настроек смысла не было.
- Полная сборка решения (`MSBuild.exe "Audiosurf SkinChanger.sln"`) даёт 0 ошибок на всех managed-проектах + `InjectHelper.vcxproj`; единственные ошибки — 9 штук в `InternalOverlayRenderer.vcxproj` (`d3dx9.h` отсутствует, `imgui/*` не завендорено — см. `.gitignore`), что ожидаемо и не связано с этой фазой: компонент уже был помечен замороженным и вне плана.
- `TweakerCore.Tests` (7/7) и `TweakerScriptsInterpreter.Tests` (5/5) — зелёные через `dotnet test`. По пути пришлось добавить `FolderHashInfo()`-конструктор без параметров — `System.Text.Json` не десериализует типы без параметрless/JsonConstructor-конструктора, тот же приём уже применялся к `LoadedSkinData` в этом же заходе.
- Ручной смоук-тест `audiosurftweaker.exe`: приложение стартует на .NET 10, не падает, доходит до штатных диалогов первого запуска ("Root skins directory not found" → "Can not detect game installation path") — оба ожидаемы в песочнице без установленной Steam/Audiosurf и без изменений логики с моей стороны. Полный прогон golden path (Skin Changer/Color Configurator/Server Swapper/Tweaker с реальной игрой) остаётся за пользователем, как и в Фазе 2.
- **Постфактум-баг, найденный пользователем после первого прохода:** на реальном запуске (уже не в песочнице) вываливалось ~10 подряд диалогов "Settings initialization error" и затем креш `SettingViewModel.set_WatcherTempFile` с `InvalidOperationException` — окно оставалось пустым (только левое меню). На Framework-сборке такого не было. Корень: `ConfigurationManager.cs` резолвил путь к конфигу через `System.Configuration.ConfigurationManager.OpenExeConfiguration(AppDomain.CurrentDomain.FriendlyName)`. `OpenExeConfiguration(path)` ищет файл `"<path>.config"` — на Framework `FriendlyName` был `"audiosurftweaker.exe"`, что совпадало с реально лежащим `audiosurftweaker.exe.config`. На .NET 10 `FriendlyName` — просто `"audiosurftweaker"` (без расширения), а SDK кладёт `audiosurftweaker.dll.config` — несовпадение имён означало, что каждый вызов `OpenExeConfiguration` открывал пустой in-memory конфиг вместо реального файла (подтверждено отдельным throwaway-проектом: `AppDomain.CurrentDomain.FriendlyName` = `"confignametest"`, а `Assembly.GetExecutingAssembly().Location` = полный путь до `.dll`, который всегда совпадает с реальным именем конфиг-файла на обеих платформах). Итог: и чтение (`InitializeEnvironment`, ранее использовавший неявный статический `ConfigurationManager.AppSettings`), и запись (`SetUpDefaultSettings`/`RewriteSettings`/`UpdateSection`) молча промахивались мимо настоящего файла. Пофикшено единообразно — все четыре метода теперь используют `OpenExeConfiguration(Assembly.GetExecutingAssembly().Location)` через общий приватный `ExePath`; `InitializeEnvironment` тоже переведён с неявного статического `AppSettings` на явный `OpenExeConfiguration` для консистентности и предсказуемости. Перепроверено вручную — окно открывается сразу с заголовком "Audiosurf Tweaker", без единого диалога.

Затрагивает `SkinChangerRestyle`, `TweakerCore` (бывш. `ChangerAPI`, включает бывший `textpackctrl`), `AudiosurfInterface` (бывш. `ASCommander`), `QuickPlayerCore`, `TweakerScriptsInterpreter(+.Tests)`, `Installer`. Обновление зависимостей (`Gameloop.Vdf`, `TagLibSharp`, `NUnit`). Таргет честно `-windows` — `WM_COPYDATA`/COM-интеропы никуда не деваются независимо от UI-фреймворка. Заодно `FolderHashInfo` (бывший `textpackctrl`, см. раунд 4) переводится с `BinaryFormatter` на JSON/простой бинарный формат — это локальный кэш, легаси-конвертер ему не нужен, старый `.hinf` при несовпадении формата просто игнорируется и пересоздаётся.

Разведка перед стартом (проверено):
- `.NET 10 SDK 10.0.301` уже установлен локально.
- Тег `archive/beta-alt-design-updater` содержит рабочий на вид WPF-автообновлятор (`Updater/`, таргет `net7.0-windows`, `GithubRepoReleaseProvider`, `MainViewModel`/`RelayCommand` в собственной реализации). **Решение: отложить** — в 3a не переносится и не трогается, вопрос по нему поднимается заново перед Фазой 4 (Avalonia), как и было изначально записано в дорожной карте.
- `Installer` использует COM-интероп (`IWshRuntimeLibrary` через `<COMReference>`, `ComImport.cs` — свой `IShellLink`) для ярлыков — SDK-style проекты поддерживают `<COMReference>` на Windows-таргетах без переписывания.
- `SkinChangerRestyle` тянет `System.Configuration.ConfigurationManager` (`AppSettings`, `OpenExeConfiguration/.Save()`) как основной механизм хранения настроек (`ConfigurationManager.cs`, `SettingsProvider.cs`) — на .NET 10 это переживает миграцию без переписывания логики, нужен только NuGet-пакет `System.Configuration.ConfigurationManager`; `App.config` остаётся как есть.
- `AudiosurfInterface` (бывш. `ASCommander`) держит `WndProcMessageService`/`WinApiServiceBase` на `System.Windows.Forms.NativeWindow`-подобной модели для приёма `WM_COPYDATA` — переживает миграцию на `net10.0-windows` без изменений в логике (нужен `UseWindowsForms=true`); реализация умышленно не трогается в 3a, её целиком заменяет `asbridge`-пайп в 3c.
- `TweakerScriptsInterpreter` тащит устаревший `packages.config`-referenced `System.IO.Compression` (`..\packages\System.IO.Compression.4.3.0\`) — на .NET 10 это часть рантайма, пакет просто выбрасывается.
- **Критично:** `LegacyDataConverter/Legacy/*.cs` намеренно содержит копии старых типов в namespace `ChangerAPI.Engine`/`ChangerAPI.Utilities`/`SkinChangerRestyle.MVVM.Model`, и `LegacyBinder` матчит эти литералы регуляркой при десериализации старых бинарных файлов пользователей. Эти namespace'ы — не про "текущее название проекта", а про то, что буквально зашито в старых `.tasp`/`.pltc` файлах на диске у пользователей. Переименование `ChangerAPI`→`TweakerCore` **не должно** затрагивать `LegacyDataConverter` вообще — ни его namespace'ы, ни регулярку в `LegacyBinder`, ни `TargetFrameworkVersion` (остаётся `v4.8.1`, `BinaryFormatter` необходим и жив только там).

Решения по объёму этого захода:
- **Только 3a.** Замена `System.Drawing`→`SkiaSharp` (3b) и переезд live-IPC на `asbridge` (3c) — отдельные последующие подтверждения, в этом заходе `AudiosurfInterface` и работа с `Bitmap` переносятся на `net10.0-windows` как есть, без переписывания логики.
- `System.Drawing.Common` на .NET 10 требует явного `PackageReference` (раньше был неявно частью Framework) и технически Windows-only — ставим `<NoWarn>CA1416</NoWarn>` на уровне проекта вместо расстановки `[SupportedOSPlatform("windows")]` по всем вызовам, т.к. это временное состояние до 3b.
- Переименования: `ChangerAPI` → `TweakerCore` (папка/csproj/namespace); `ASCommander` → `AudiosurfInterface` (папка/csproj/namespace); `textpackctrl` (namespace `FolderChecker`) сливается в `TweakerCore/FolderChecker/` как internal-модуль, отдельным проектом/папкой быть перестаёт; `textpackctrl.Test` сливается в `ChangerAPI.Tests`, который сам переименовывается в `TweakerCore.Tests`.
- SDK-style переезд затрагивает 8 проектов: `TweakerCore`, `TweakerCore.Tests`, `AudiosurfInterface`, `QuickPlayerCore`, `TweakerScriptsInterpreter`, `TweakerScriptsInterpreter.Tests`, `Installer`, `SkinChangerRestyle`. `LegacyDataConverter` (+ его отсутствующий тест-проект) не трогается — остаётся `net481`/старый csproj намеренно заморожен.
- Новые `.csproj` — `Sdk="Microsoft.NET.Sdk"`, `<TargetFramework>net10.0-windows</TargetFramework>`, implicit file globbing (все явные `<Compile Include>`/`<Page Include>` вычищаются — SDK-style сам подхватывает `*.cs`/`*.xaml`; спецпункты вроде `ApplicationIcon`, `SplashScreen`, `Settings.settings` (`SettingsSingleFileGenerator`) остаются явно, но через `Update=`, не `Include=`, чтобы не словить duplicate-item ошибку). `UseWPF=true` для `SkinChangerRestyle`, `UseWindowsForms=true` для `SkinChangerRestyle`/`AudiosurfInterface`. Общие свойства (`TargetFramework`, `LangVersion`, `Nullable=disable` — сознательно не включаем nullable, чтобы не тащить лишний unrelated churn) выносятся в новый корневой `Directory.Build.props`.
- Упрощение конфигураций: вместо пары `AnyCPU`/`x64` (со странным `Prefer32Bit=true` при `PlatformTarget=x64` в старых csproj) — единый `PlatformTarget=x64` без дублирования Debug/Release-специфичных `LangVersion`-пиннингов (net10 сам ставит актуальный C#).
- `packages.config` → `PackageReference` для NUnit/NUnit3TestAdapter (+ `Microsoft.NET.Test.Sdk`, нужен для `dotnet test` на SDK-style) и `TagLibSharp` в `QuickPlayerCore`.
- `.sln`: обновить путь/имя для переименованных проектов (GUID'ы не трогаем), убрать записи `textpackctrl`/`textpackctrl.Test`. `ASBridge` в `.sln` не добавляется (решение раунда 3 — CMake, не MSBuild).

Порядок выполнения:
1. `Directory.Build.props` в корне репо.
2. Переименование `ChangerAPI`→`TweakerCore` (папка, csproj, namespace во всех `.cs`), слияние `textpackctrl` → `TweakerCore/FolderChecker/`.
3. Переименование `ASCommander`→`AudiosurfInterface` (папка, csproj, namespace).
4. Переименование `ChangerAPI.Tests`→`TweakerCore.Tests`, перенос `EnvironmentalChecker_Test.cs` из `textpackctrl.Test`, правка `ProjectReference`.
5. Конвертация всех 8 незамороженных `.csproj` в SDK-style + `net10.0-windows`.
6. Правка `.sln` (пути/имена, удаление слитых проектов).
7. Правка всех `using ChangerAPI`/`using ASCommander`/квалифицированных обращений в `SkinChangerRestyle`, `QuickPlayerCore`, `TweakerCore.Tests`.
8. `dotnet build` всего решения (кроме `LegacyDataConverter` — он через MSBuild/VS, не `dotnet build`, т.к. остаётся Framework), итеративный фикс ошибок компиляции/NuGet-совместимости.
9. `dotnet test` для `TweakerCore.Tests`, `TweakerScriptsInterpreter.Tests`.
10. Ручной прогон `audiosurftweaker.exe` (net10) — Skin Changer / Color Configurator / Server Swapper / Tweaker, плюс проверка, что вызов `LegacyDataConverter.exe` как внешнего процесса всё ещё работает из net10-хоста.
11. Обновить дорожную карту (статус 3a, зафиксировать реальные отклонения — версии пакетов, что пришлось доп. поправить).
12. Коммит и пуш — только по явному подтверждению пользователя, как и в предыдущих фазах.

**3b. Замена работы с изображениями**
`TweakerCore.NamedBitmap`/`ImageGroup` сейчас держат `System.Drawing.Bitmap` напрямую. `System.Drawing.Common` в современном .NET — Windows-only и не рекомендуется; раз всё равно переезжаем на Avalonia (использует Skia) — сразу переводим работу с изображениями на `SkiaSharp`, вместо того чтобы тащить `System.Drawing` и переписывать это ещё раз в Фазе 4. Заодно `LoadingCache` (см. раунд 4) переводится с `BinaryFormatter` на новый формат — тоже локальный кэш без миграции, но менять его формат раньше смены `Bitmap` на `SkiaSharp` бессмысленно, поэтому делается одним заходом здесь.

**3c. Live IPC на `asbridge` — самый крупный кусок фазы**
Новая архитектура вместо прежнего плана "P/Invoke прямо в managed-коде":
- Новый нативный C++ проект **`asbridge`** (exe, `asbridge.exe`) — полностью владеет Win32-стороной: находит процесс `QuestViewer`, регистрирует message-only окно, обменивается `WM_COPYDATA` с игрой (протокол `ascommand`/`asconfig` не меняется — это фиксированный словарь, который понимает игра/патч). Дополнительно поднимает сервер именованного пайпа с простым текстовым построчным протоколом наружу.
- `AudiosurfInterface` (managed) перестаёт сама трогать Win32 — становится тонким клиентом: запускает/отслеживает субпроцесс `asbridge.exe`, держит `NamedPipeClientStream`, транслирует команды/статусы в существующую модель состояний (`ASHandleState`, очередь команд и т.д.), которую уже используют `TweakerViewModel` и остальные.
- Обрыв пайпа/падение `asbridge.exe` не роняет UI Tweaker-а — `AudiosurfInterface` детектит разрыв и перезапускает субпроцесс, показывая статус "переподключение" вместо краша всего приложения. Это прямое продолжение цели Фазы 1 по устойчивости к крашам.
- Архитектурно это расширение уже существующего в проекте паттерна — `InjectHelper.exe` уже сегодня запускается как отдельный процесс из `OverlayHelper`.

**Проверка:** решение собирается и запускается на .NET 10 (UI ещё WPF — промежуточный шаг), `asbridge.exe` реально запускается, находит `QuestViewer`, шлёт/принимает `WM_COPYDATA`; `AudiosurfInterface` через пайп получает live-статусы и умеет пережить принудительное убийство `asbridge.exe` без падения UI; Skin Changer и Color Configurator работают с новыми форматами и умеют вызывать `LegacyDataConverter.exe` на старых файлах.

---

## Фаза 4 — Переезд UI на AvaloniaUI + CommunityToolkit.Mvvm

- Заменить самодельные `ObservableObject`/`RelayCommand` на `CommunityToolkit.Mvvm` (`[ObservableProperty]`, `[RelayCommand]`).
- `ColorPalette`/цветовая модель — с `System.Windows.Media.Color` на `Avalonia.Media.Color` (или собственный UI-агностичный struct).
- Портировать по одному фиче-модулю за раз, в порядке возрастания сложности:
  1. Оболочка приложения (`MainWindow`/`MainViewModel`).
  2. Color Configurator.
  3. Skin Changer (список скинов, установка/сохранение, диалоги).
  4. Server Swapper (список серверов + пинг-статус).
  5. Tweaker + `ServiceWindows` (диалоги: `ProcessSelectionWindow`, `TweakerDialog`, `GuidancePage`).
- Темы/стили (`Themes/`) переписать как Avalonia `Styles`/`ControlTheme` — не 1:1 копия, WPF `DataTrigger`/ресурсы переизобретаются через Avalonia Selectors/псевдоклассы.
- Держать старое WPF-приложение рабочим до тех пор, пока Avalonia-версия не покроет фичу паритетно.

**Проверка:** на каждом портированном модуле — ручной прогон в приложении (golden path + edge cases), сравнение поведения со старой WPF-версией перед её отключением.

---

## Фаза 5 — QuickPlayer

Финальная фаза, после того как ядро (миграция+чистка+баги+формат+live-IPC) стабильно.

- Из старого `QuickPlayerCore` реально переиспользуемо: `MetadataReader` (обёртка над TagLibSharp) и билдер тегов Audiosurf (`Audiosurf/SongTags.cs`, уже валидирует `[as-4lane]`/`[as-msz{n}]`/`[as-wb{n}]`).
- Не переиспользуется: пустой стаб `TagWriter.WriteTags`, битая эвристика `Codec` через `Enum.TryParse(Path.GetExtension(...))` — выбрасываются, а не чинятся.
- Игровой IPC-интерфейс умеет управлять загрузкой/запуском треков — это подтверждено, но набор команд нигде в проекте не задокументирован. Первый шаг фазы — задокументировать реально используемый словарь команд (тот же `ascommand`/`asconfig`-протокол, которым уже говорит `asbridge` с игрой), прежде чем проектировать API плеера поверх него.
- Дальше: модель плейлиста + Avalonia UI поверх неё + драйвер последовательного воспроизведения, посылающий команды через уже существующий канал `AudiosurfInterface` → `asbridge` → игра.

**Проверка:** реальное создание плейлиста и подтверждённый переход игры между треками через отправленную IPC-команду.

---

## Сквозные риски, которые стоит держать в голове

- Все "живые" фичи (Tweaker, будущий QuickPlayer) в любом случае Windows-only из-за `WM_COPYDATA`/HWND внутри `asbridge` — Avalonia даёт кросс-платформенность только файловым фичам (Skin Changer, Color Configurator, Server Swapper).
- Замена форматов скинов и цветовых пресетов — breaking change для существующих пользователей; `LegacyDataConverter.exe` обязателен и должен быть протестирован на реальных старых файлах обоих типов, иначе люди потеряют свои скин-библиотеки и/или пресеты при обновлении.
- `asbridge` — новый компонент с собственным жизненным циклом процесса; нужно нормально продумать "что если субпроцесс не запустился", "что если завис", "что если основной Tweaker убит без штатного закрытия пайпа" (осиротевший `asbridge.exe`) — заложить на это время в Фазе 3, это не тривиальная задача, хоть и проще, чем P/Invoke-колбэки.
- `TweakerOverlay` умышленно вне плана — если он понадобится раньше, чем ожидается, это отдельное обсуждение, не влезающее в эту дорожную карту без реверс-инжиниринга рендеринга игры.

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
- `TweakerOverlay`/`InternalOverlayRenderer` — тема закрыта для этой дорожной карты. Причина: нынешний рендер-хук построен на костыле (игра сама себе шлёт `WM_COPYDATA`), плюс баги UI из-за особенностей DX9-окна Audiosurf. Нормальная реализация требует отдельного реверс-инжиниринга рендеринга игры — когда до этого дойдут руки, тема поднимается заново отдельным планом. ~~Существующий код `InternalOverlayRenderer`/`InjectHelper` не трогаем и не мигрируем, оставляем замороженным как есть.~~ **Пересмотрено на этапе 4.5 (2026-07-13):** раз внедряемый модуль всё равно ждёт полный реворк на других механизмах, держать его замороженным ради референса смысла не имеет — `InternalOverlayRenderer` удалён из репозитория целиком (проект, исходники, ссылка в `.sln`). `InjectHelper` (обобщённый CreateRemoteThread-инжектор, не завязан на конкретную полезную нагрузку) не удалён — решение по нему отдельное, не поднималось.

**Принятые решения (раунд 3):**
- C++ больше не живёт на MSBuild/vcxproj — новые нативные модули (сейчас это `asbridge`) собираются **CMake**. MSBuild дёргает CMake как чёрный ящик из pre-build шага, без генерации VS-проектов и без записи в `.sln` — при работе над `asbridge` он открывается/дебажится как отдельная CMake-папка (VS "Open Folder"/CLion), минимальная связанность с `.sln`.
- Замороженные `InjectHelper`/`InternalOverlayRenderer` остаются как есть на своих `vcxproj` — они часть замороженного оверлей-функционала целиком, под CMake-конвейер не переводятся, пока (если) оверлей не будет разморожен отдельным треком. *(`InternalOverlayRenderer` с этапа 4.5 удалён из репозитория — см. пересмотр решения раунда 2 выше. `InjectHelper` пока остаётся на `vcxproj`, вне CMake-конвейера.)*

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

**Инвариант на будущее:** `LegacyDataConverter/Legacy/*.cs` держит копии старых типов в namespace, буквально зашитом в старые `.tasp`/`.pltc` файлы пользователей на диске (`ChangerAPI.Engine`/`ChangerAPI.Utilities`/`SkinChangerRestyle.MVVM.Model`) — это не "текущее название проекта", менять нельзя. Никакое будущее переименование живых проектов не должно трогать эти namespace'ы, регулярку в `LegacyBinder`, или `TargetFrameworkVersion` конвертера (`v4.8.1`, нужен для живого `BinaryFormatter`).

---

## Общий архитектурный принцип для platform-specific кода

Договорились не городить одну и ту же тяжёлую схему везде:

- **Субпроцесс + именованный пайп** — там, где код реально общается с посторонним процессом (игрой) и может неожиданно упасть/зависнуть. Даёт изоляцию сбоя от UI-процесса Tweaker-а, managed-сторона получает только тривиальный `NamedPipeClientStream` без единого P/Invoke.
- **Обычный P/Invoke/COM-интероп прямо в коде** — для одноразовых системных вызовов из собственного процесса (например COM shell-ссылка/регистрация иконки в `Installer`). Заворачивать это в субпроцесс с пайпом было бы избыточным усложнением.

---

## Сборка нативных (C++) модулей: CMake + MSBuild pre-build hook

- Активно разрабатываемые C++ модули (сейчас — `asbridge`, в будущем — что угодно нативное) собираются **CMake**, не MSBuild/vcxproj. Новых `vcxproj` для них не заводится.
- Оркестрация: у managed-сборки (MSBuild, скорее всего общий `Directory.Build.targets`/pre-build target в `.csproj`) есть шаг, который на этапе сборки .NET-проекта вызывает `cmake --build` для нативных модулей и проверяет exit code. Интеграция — чисто внешний вызов, никакой генерации VS-проектов из CMake и никакой привязки к `.sln`.
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

Три параллельных потока работы внутри фазы:

**3a. Managed-проекты на SDK-style + net10.0-windows**

**Статус: ✅ Выполнено**

Переименования (`ChangerAPI`→`TweakerCore`, `ASCommander`→`AudiosurfInterface`, слияние `textpackctrl`→`TweakerCore.FolderChecker`, `ChangerAPI.Tests`→`TweakerCore.Tests`) выполнены по плану, все 8 незамороженных проектов переведены на SDK-style + `net10.0-windows` через общий `Directory.Build.props`. `LegacyDataConverter` намеренно не тронут — остаётся `net481`, единственное место, где `BinaryFormatter` ещё жив (см. инвариант в начале документа).

Находки, которые стоит держать в голове на будущее:
- `LoadingCache` пришлось перевести на `System.Text.Json` уже здесь, не дожидаясь 3b — на .NET 9+ `BinaryFormatter.Serialize/Deserialize` не warning, а `error SYSLIB0011` (жёсткий компиляционный блокер).
- UWP toast-уведомления (`ToastContentBuilder.Show()`) резолвятся только на TFM с версией Windows SDK — отсюда `net10.0-windows10.0.19041.0` вместо голого `net10.0-windows`. Это же ограничение унаследовал `TweakerUI.csproj` в Фазе 4 (см. комментарий в файле).
- `Installer` (COM-интероп на ярлыки, `<COMReference>`) собирается только полным `MSBuild.exe`/Visual Studio — `dotnet build`/`dotnet test` падают на нём с `MSB4803` (`ResolveComReference`-таск недоступен в кроссплатформенном SDK). Не гонять его через `dotnet`-CLI.
- Постфактум-баг, найденный пользователем на реальном запуске (не в песочнице): `ConfigurationManager.cs` резолвил путь конфига через `AppDomain.CurrentDomain.FriendlyName`, который на .NET 10 не совпадает с именем `.config`-файла, которое кладёт SDK — конфиг молча не находился, `SettingViewModel` падал с `InvalidOperationException`. Пофикшено на `OpenExeConfiguration(Assembly.GetExecutingAssembly().Location)`; фикс и его обоснование задокументированы прямо в коде (`ExePath` в `ConfigurationManager.cs`) и сохранены при переносе в `TweakerUI/Core` (Фаза 4.0).
- Полная сборка решения — 0 ошибок вне замороженного `InternalOverlayRenderer.vcxproj`; `TweakerCore.Tests`/`TweakerScriptsInterpreter.Tests` — 12/12.

**3b. Замена работы с изображениями**

**Статус: ✅ Выполнено**

`SkiaSharp 3.119.4` выбран не самой свежей 4.x-версией специально — Avalonia (цель Фазы 4) собрана против SkiaSharp 3.119, выравнивание версий заранее убирает конфликт нативных бинарей при переезде UI. `NamedBitmap`/`ImageGroup`/`SkinPackager`/`LoadingCache`/`TweakerCore.Tests` переведены с `System.Drawing.Bitmap` на `SKBitmap` без сюрпризов уровня 3a; попутно ушёл GDI-костыль из Фазы 2 (буферизация `Bitmap.Save()` через `MemoryStream`) — `SKData` уже в памяти и пишется в zip-entry напрямую. GDI-путь (`ToImageSource(Bitmap)`+`DeleteObject`) сознательно оставлен только для WPF resx-иконок — умирает вместе с ними в Фазе 4.

Проверка: полная сборка — 0 ошибок вне замороженного `InternalOverlayRenderer`; тесты 12/12; сквозной ручной прогон на реальном `Vintage Works.askin2` подтвердил корректный round-trip `load.cache` (создание, чтение обратно без пересоздания).

**3c. Live IPC на `asbridge`**

**Статус: ✅ Выполнено** (сквозная проверка против фейковой игры — см. ниже; прогон с реальным Audiosurf остаётся за пользователем)

Архитектура (актуальна для Фазы 4.5/Tweaker-консоли и Фазы 5/QuickPlayer — оба говорят через этот канал): нативный `asbridge.exe` полностью владеет Win32-стороной — находит окно игры по имени процесса (`EnumWindows`+`QueryFullProcessImageNameW`, не по заголовку — заголовок не гарантирован), держит скрытое окно-«губку» для приёма `WM_COPYDATA` в ANSI-кодировке (легаси-протокол `ascommand`/`asconfig` игры не меняется), и поднимает именованный пайп с текстовым протоколом наружу. `AudiosurfInterface` (managed) — тонкий pipe-клиент; публичный фасад `AudiosurfHandle` (`Instance`, события `StateChanged`/`Registered`/`MessageResieved`/`CommandSent`, свойства `IsValid`/`GamePID`/`StateMessage`, методы `Command`/`TryConnect`/`StartAutoHandling`/`ReinitializeWndProcMessageService`) не менялся для вызывающего кода. `WndProcMessageService`/`WinApiServiceBase`/`PInvoke/` и ручной выбор процесса (`SetHandle(Process)`) выпилены полностью.

**Протокол** (нужен при работе с Tweaker-консолью в 4.5 и с QuickPlayer в Фазе 5): клиент → `CCOMMAND SEND "cmd"`; сервер → `SREPORT OK/FAILED/BROADCAST_FORWARD/SERVICE "..."` (`SERVICE "WINDOW_FOUND" "<pid>"`, `SERVICE "WINDOW_LOST"`, `BROADCAST_FORWARD "successfullyregistered"`/`"successfullyquickstartregistered"` — регистрацию и quickstart-ветку с проверкой возраста процесса < 30с шлёт сам `asbridge`, managed-стороне ничего вручную дёргать не нужно).

**Устойчивость:** `asbridge.exe` падает/убит → твикер рестартует процесс и переподключает пайп, статус временно `NotConnected`, UI не падает; твикер убит без штатного закрытия → `asbridge.exe` завершает себя сам по смерти PID-родителя (проверено — 0 осиротевших процессов).

CMake pre-build hook (см. общий раздел выше) интегрирован в `SkinChangerRestyle.csproj` и продублирован 1:1 в `TweakerUI.csproj` в Фазе 4.0 — `dotnet build` любого из двух проектов сам собирает `asbridge.exe` и кладёт рядом с exe. Три C++-бага (потеря `m_owning` при move окна-губки, инвертированная валидация сообщений, гонка порядка `WINDOW_FOUND`/`successfullyregistered`) были найдены только сквозным тестированием, не ревью кода, — уже исправлены, повторный разбор не нужен.

---

## Фаза 4 — Переезд UI на AvaloniaUI + CommunityToolkit.Mvvm

**Статус: ✅ Перенос модулей завершён (2026-07-13)** — все этапы 4.0-4.6 выполнены (Shell, Color Configurator, Skin Changer, Tweaker, Settings), включая раунды визуальных правок по фидбеку пользователя и уборку мёртвого кода, оставшегося от отменённых Server Swapper/Overlay (`NetworkTools`, `OverlayHelper`, `InternalOverlayRenderer`, overlay-only настройки). Фаза не закрыта полностью — см. «Финал фазы» ниже: визуальный consistency-проход не сделан (не обязателен), а удаление `SkinChangerRestyle`/`TweakerScriptsInterpreter` осознанно ждёт ручной проверки паритета с реальной игрой (не может быть сделано мной — нет реального Steam/Audiosurf в песочнице).

**Контекст:** это самый крупный кусок дорожной карты — полная замена WPF-фронтенда (`SkinChangerRestyle`, ~70 файлов, MVVM с самодельными `ObservableObject`/`RelayCommand`) на AvaloniaUI. Пользователь уже создал каркас проекта `TweakerUI` через мастер Avalonia для Visual Studio (Avalonia 12.0.5, CommunityToolkit.Mvvm 8.4.1, штатный `ViewLocator`, `CompiledBindings` включены). `SkinChangerRestyle` остаётся проектом-донором в `.sln` до конца фазы — код и вёрстка оттуда копируются и адаптируются модуль за модулем, чтобы всегда было с чем сверить поведение. Пользователь подтвердил: рестайлинг — «улучшать по ходу, модуль за модулем» (не 1:1 копия вёрстки, но и не отдельный редизайн-заход).

### Найденные и исправленные проблемы в каркасе `TweakerUI` (этап 4.0)

Мастер Avalonia сгенерировал разъехавшийся namespace (`AudiosurfTweaker.*` для `App`/`MainWindow` вместо `TweakerUI.*` для остального) — это тихо ломало бы `ViewLocator.Build` (резолвит View по замене `"ViewModel"→"View"` в `FullName`) на первом же реальном модуле. Плюс голый `net10.0` вместо `net10.0-windows`, и не зафиксированный явно `RootNamespace`. Всё сведено к единому `TweakerUI.*` + `net10.0-windows` + явный `<RootNamespace>TweakerUI</RootNamespace>`.

### Сквозные архитектурные решения (действуют для всех последующих этапов)

1. **MVVM-база:** самодельные `SkinChangerRestyle.Core.ObservableObject`/`RelayCommand` заменяются на `CommunityToolkit.Mvvm` (`[ObservableProperty]`, `[RelayCommand]`, `partial class`). Все вью-модели наследуют `TweakerUI.ViewModels.ViewModelBase`. `ScrollAllowed`-костыль старого `ObservableObject` (использовался в одном месте — `MainViewModel.CurrentViewScrollAllowed`) переносится как обычное свойство базового класса, если понадобится.
2. **Namespace/папки для `ViewLocator`:** держим плоские `TweakerUI.ViewModels`/`TweakerUI.Views`/`TweakerUI.Models` (без вложенных подпапок по фичам — вложенные namespace'ы усложняют работу `ViewLocator`) и именование `<Feature>ViewModel`/`<Feature>View`.
3. **Кросс-поточный доступ к UI:** WPF-паттерны `BindingOperations.EnableCollectionSynchronization` и `Dispatcher.Invoke` заменяются на `Avalonia.Threading.Dispatcher.UIThread.Post/InvokeAsync`. Правило: любой код, который трогает `ObservableCollection`/забинженные свойства из фонового потока или из колбэка `AudiosurfHandle`/`TexturesWatcher`/`Task.ContinueWith`, обязан завернуться в `Dispatcher.UIThread`.
4. **Уведомления:** `Notification.Wpf.NotificationManager` заменяется на встроенный `Avalonia.Controls.Notifications.WindowNotificationManager`. UWP toast-ветка (`Microsoft.Toolkit.Uwp.Notifications`) не трогается — не зависит от WPF. Публичный фасад `ApplicationNotificationManager` (статический `Manager`, методы `Show*`/`AskForAction`/`ShowImportantInfo`) сохраняется 1:1 по сигнатурам, меняется только реализация внутри.
5. **Диалоги (`AskForAction`/`ShowImportantInfo`, старый `TweakerDialog.xaml`):** порт на нативный async-паттерн (`await window.ShowDialog<TweakerDialogResult>(owner)`). **Важно:** блокирующий `.GetAwaiter().GetResult()` не вариант — в отличие от WPF, Avalonia `Window.ShowDialog<T>()` держится на `Dispatcher.UIThread`, а не поднимает свой message loop; блокирующее ожидание с того же UI-потока — гарантированный дедлок. `AskForAction`/`ShowImportantInfo` стали честными `async Task<bool>`/`async Task`, вызывающий код везде переписывается на `await` по мере портирования модуля.
6. **Статичные иконки перестают быть C#-кодом.** PNG-файлы (`SkinChangerRestyle/Resources/*.png`, без SVG-дублей — `Avalonia.Svg.Skia` не подключаем) переносятся в `TweakerUI/Assets/Icons/`, биндятся в XAML напрямую по `avares://` — без единого `ImageSource`-свойства во вью-модели.
7. **Динамические изображения:** `TweakerCore.Engine.NamedBitmap`/`ImageGroup` продолжают отдавать `SKBitmap` (не трогаем — общий слой из Фазы 3b). `TweakerUI/Core/Extensions.cs` получает `ToAvaloniaBitmap(this SKBitmap)` (энкод PNG в память → `Avalonia.Media.Imaging.Bitmap`). Все динамические `ImageSource`-свойства переходят на этот тип.
8. **Диалоги выбора файлов/папок:** WinForms-диалоги заменяются на Avalonia `IStorageProvider` (async), обёрнутый статическим `TweakerUI/Services/FileDialogService.cs` (по образу `ApplicationNotificationManager`, без DI-контейнера — остальной код проекта тоже без DI).
9. **`Core/`-хелперы без UI-зависимостей переносятся в `TweakerUI/Core/` почти дословно, в `TweakerCore` НЕ перемещаются** (отдельный необязательный рефакторинг, не мешаем его с переездом UI): `SettingsProvider.cs`, `ConfigurationManager.cs`, `Logger.cs`, `Utils/Utils.cs`, `AudiosurfConfigurationPresenter.cs`. `Core/ServerSwapper/ServerSwapper.cs` и `Core/NetworkTools/*` **не переносятся** — Server Swapper отменён целиком (см. 4.4), `NetworkTools` (ping-утилита для проверки доступности Remote-сервера) был нужен только ему.
10. **`ColorPalette`:** `System.Windows.Media.Color` → `Avalonia.Media.Color` напрямую (модель и так живёт в UI-слое, второго потребителя не просматривается). Хранение в `.pltc`/`.palette` через `ColorPalettePrint`+`System.Text.Json` не меняется.
11. **Конвертеры** (`EnumBooleanConverter`, `InversedBooleanToColorConverter`) — портируются почти без изменений, `IValueConverter` в Avalonia имеет ту же форму.
12. **Drag&drop:** Avalonia 12 API — `IDataObject`/`DataFormats.Files` устарели, реальный путь идёт через `DragEventArgs.DataTransfer` (`IDataTransfer`) и `DataTransferExtensions.TryGetFiles(...)`.
13. **`ProcessSelectionWindow`/`ProcessSelectionViewModel` — не переносятся, удаляются целиком** (решение пользователя): ручной выбор процесса уже выпилен в Фазе 3c, кнопка «Reset» в шапке делает то же самое. Кнопка «Find» в новой вёрстке не появляется.

### Порядок выполнения (модуль за модулем)

**4.0 — Инфраструктура и скелет.** Статус: ✅ Выполнено. Namespace/TFM/`RootNamespace`-фиксы (выше); добавлены `ProjectReference` на `TweakerCore`/`AudiosurfInterface`/`TweakerScriptsInterpreter`, `TweakerUI/Core/` (перенос framework-agnostic хелперов по п. 9), `NotificationService`/`FileDialogService`/`Core/Extensions.cs` (п. 4, 6-8). `TweakerUI` добавлен в `.sln`. Asbridge CMake pre-build hook (Фаза 3c) продублирован из `SkinChangerRestyle.csproj` в `TweakerUI.csproj` — нужен, т.к. `TweakerUI` теперь сам запускает `AudiosurfHandle` и должен нести свою копию `asbridge.exe`. Собрано без конфликта версий `SkiaSharp` между `TweakerCore` (зафиксирован 3.119.4 ещё в 3b) и `Avalonia.Skia`.

**4.1 — Оболочка приложения (Shell).** Статус: ✅ Выполнено, закоммичено и запушено. Порт `MainViewModel`→`MainWindowViewModel` (дочерние VM как заглушки до соответствующих этапов), безрамочное окно (`WindowDecorations="None"` + `PointerPressed`→`BeginMoveDrag`), левое нав-меню, статус-индикатор подключения к игре на `AudiosurfHandle.Instance` (подтверждено: `SynchronizationContext`, захваченный в Фазе 3c, корректно резолвится в Avalonia UI-поток). `TweakerDialog`/`WindowNotificationManager`-хост заведены здесь же, т.к. нужны почти всем следующим модулям. Кнопка Find не переносится (п. 13). **Проверка:** шапка/навигация/статус-индикатор работают, окно двигается/сворачивается/разворачивается/закрывается, переключение (пока пустых) вкладок работает.

**4.2 — Color Configurator.** Статус: ✅ Выполнено, закоммичено и запушено (`23426fb`). Порт `ColorPalette`/`ColorPalettePrint`/`PaletteDynamicLoadContainer`/`ColorsConfiguratorViewModel`. Пикер собран с нуля по макету пользователя («Palette Designer», Claude Design MCP), а не по изначально планировавшемуся `PixiEditor.ColorPicker`-аналогу: кастомные `HsvWheel` (SkiaSharp sweep-gradient bitmap — у Avalonia нет conic-gradient кисти) и `GradientSlider` в `TweakerUI/Controls/` — оба переиспользуемы для будущих color-related экранов. Инверсия палитры реализована как поворот hue на 180°, а не RGB-инверсия (сознательное отличие от старого приложения, по макету). `ColorPalette` получил стабильный `Id` (`Guid`) поверх сравнения "по значению" — старая схема путала одноимённые палитры-дубликаты при переименовании/замене/удалении; **паттерн стоит помнить для любой будущей коллекции пользовательских именованных объектов**. Глубокое тестирование (реальная игра, `options.ini`) в объём Фазы 4 не закладывалось. **Проверка (смоук):** CRUD палитры, экспорт/импорт `.palette`, адаптивность вёрстки при ресайзе.

**4.3 — Skin Changer (самый крупный модуль).** Статус: ✅ Выполнено, закоммичено и запушено (`8944670`, `acd17ea`). Порт `SkinCard`/`InteractableScreenshot`/`SkinChangerViewModel`/вёрстки по макету «Audiosurf Tweaker» (Claude Design MCP): сетка карточек, установка/экспорт/переименование/удаление, drag&drop `.tasp`/`.askin2`, `OverlayHelper` перенесён как есть (P/Invoke, не WPF-специфичен) — *позже, на этапе 4.5, удалён вместе со всей overlay-интеграцией, см. ниже*. **Edit on Disk выпилен полностью** — вне объёма этого возрождения, замена будет отдельной задачей. Иконки — `StreamGeometry`-ресурсы по SVG-путям макета, вынесены в собственный `TweakerUI/Themes/SkinChangerStyles.axaml` (по прямой просьбе пользователя — стили каждой вкладки в своём файле, не одной кучей; та же схема, что `CCCard`/`SCCard` в 4.2 — токены дублируются под своим префиксом на файл, не шарятся напрямую). Кэширования скриншотов/карточек **сознательно нет** (память) — вместо этого пульсирующие skeleton-плейсхолдеры на время загрузки, с количеством из `hint-placeholder-count` макета — переиспользуемый паттерн для будущих модулей со схожей нагрузкой. Из этого же решения следует: hover-превью карточки скина (было в донор-коде, реализовано, затем **выпилено окончательно**) — требует заранее декодированного скриншота на карточку, что прямо противоречит "не кэшируем"; в код добавлен явный комментарий, почему превью нет — **не пытаться вернуть без пересмотра решения о кэшировании**. `ImageViewWindow` перенесено раньше срока (планировалось на 4.5). `TweakerUI/Controls/AspectRatioBox.cs` — новый переиспользуемый примитив (у Avalonia нет аналога CSS `aspect-ratio`), держит превью скриншотов на 16:9 при любом ресайзе окна. Несколько раундов визуальных правок по скриншотам пользователя (alignment, цвет выделения, светлые подложки-карточки для консистентности с Color Configurator) — вкладка закрыта после подтверждения пользователя ("для задачи «перенести и чуть-чуть подправить» — более чем"). **Проверка (смоук, оба мок-скина из `Mocks/`, включая легаси `.askin2` через конвертер):** загрузка сетки, install/clear-install, экспорт, добавление (файл + drag&drop + переименование — вручную), удаление, превью+увеличение по двойному клику, skeleton-плейсхолдеры. Не проверено вручную: реальный клик «Import from Game» с отсутствующим путём (guard есть по аналогии с `InstallSkin`, сам клик не воспроизводился).

**Досмотренный визуальный баг (2026-07-13, найден пользователем при финальном проходе по Фазе 4):** иконочная колонка (Add/Import from Game/Refresh) стояла отдельно от `Border.SCCard` списка скинов, прямо на фоне страницы — визуальный провал/дыра в общей карточной раскладке. Исправлено переносом колонки внутрь той же карточки (общий `Grid ColumnDefinitions="40,10,*"` внутри одного `Border.SCCard`, вместо отдельной `StackPanel` в соседней колонке внешнего грида) — теперь иконки и список скинов — один визуальный блок.

**4.4 — Server Swapper.** Статус: ⛔ Отменено, в `TweakerUI` не переносится (решение пользователя, зафиксировано 2026-07-13). Причина: не востребована — у единственного альтернативного сервера для игры есть собственный официальный онлайн-установщик, Server Swapper в твикере не даёт ничего сверх этого, только усложняет кодовую базу. Модуль просто не появляется в новом UI (не "перенесён, потом удалён" — изначально не переносится). Следствия для остального плана: п. 9 выше больше не включает `Core/ServerSwapper/ServerSwapper.cs`, п. 12 больше не касается `ServerSwapperViewModel.OnFileDrop`.
  - Мёртвый код от этого решения вычищен (2026-07-13): удалены заглушки `ServerSwapperViewModel.cs`/`ServerSwapperView.axaml(.cs)`, нав-пункт и `ShowServerSwapperCommand`/`ServerSwapperVM` из `MainWindowViewModel`/`MainWindow.axaml`, неиспользуемый `ProjectReference` на `TweakerScriptsInterpreter` из `TweakerUI.csproj`. **Добавление задним числом (найдено пользователем при финальном проходе по Фазе 4):** `TweakerUI/Core/NetworkTools/` (ping-утилита для проверки доступности Remote-сервера — нужна была только Server Swapper-у) тоже осталась незамеченной при первой уборке; ноль обращений к ней где-либо ещё в кодовой базе — удалена целиком.
  - `TweakerScriptsInterpreter`/`.Tests` (DSL-интерпретатор install-скриптов, был нужен только для Server Swapper) сам по себе **пока не удаляется** — старый WPF Server Swapper в `SkinChangerRestyle` всё ещё на нём держится и остаётся донором-референсом до конца Фазы 4. Удаление проекта из `.sln` — см. «Финал фазы».

**4.5 — Tweaker + оставшиеся `ServiceWindows`.** Статус: ✅ Выполнено.

Порт `TweakerViewModel`/`TweakerConsole`/`Tweaker.xaml` на карточную вёрстку: чекбоксы донора заменены на toggle-переключатели в стиле Skin Changer (`TWSwitch`, копия `SCSwitch` по уже принятой per-file-стилизации, `TweakerStyles.axaml`) с своими `StreamGeometry`-иконками, plain-кнопки — на `TWCommandButton`-список, лог — на тёмную консоль (`TWConsole`) с автопрокруткой и кнопкой очистки (`FlushConsole` в доноре был объявлен, но нигде не забинжен — забинден сейчас). **Важная правка по ходу (2026-07-13, по прямому указанию пользователя):** донорский `TweakerViewModel` гонял `tw-config tweak-active ...`/слушал `tw-Notify-Tweak-Changed` — это был канал синхронизации с ImGui-чекбоксами `InternalOverlayRenderer`. Раз внедряемый модуль всё равно ждёт полный реворк на новых механизмах, портировать этот канал было ошибкой — весь `tw-config`/`tw-Notify-Tweak-Changed` код из `TweakerViewModel` убран, все 7 твиков теперь единообразно шлют только `asconfig ...` в игру. Заодно из `SkinChangerViewModel` убрана вся overlay-инъекция (`InjectOverlayPlugin`/`OnOverlayInjected`/`UpdateOverlaySkinsList`/`OnMessageReceived`, `tw-Install-package`/`nowplayingsongtitle`/`songcomplete`/`oncharacterscreen`), удалён `TweakerUI/Core/OverlayHelper.cs` (существовал только для инжекта `InternalOverlayRenderer.dll`), из `SettingsProvider`/`ConfigurationManager` убраны мёртвые теперь `IsOverlayEnabled`/`IsOverlayInstanceAlive`/`Infopanel*`. `InternalOverlayRenderer` удалён из репозитория целиком — не просто заморожен, как планировалось изначально. `InjectHelper` (универсальный инжектор, не завязан на конкретную полезную нагрузку) не тронут.

`ProcessSelectionWindow`/`ImageViewWindow`/`EditOnDiskLockWindow` выпадают из объёма этого этапа (удалена/перенесена досрочно/не нужна — см. п. 13 выше и итоги 4.3). `GuidancePage` — вопрос закрыт в 4.6: не переносится вообще — оба гайда, которые она показывала (Server Swapper, Overlay Troubleshooting), были про уже отменённые фичи.

**Проверка:** тумблеры твиков доходят до игры (`asconfig`), консоль показывает отправленные/полученные команды и поддерживает очистку, `closeaudiosurf` завершает процесс игры.

**Раунд визуальных правок по скриншотам пользователя (2026-07-13):** консоль (`TWConsole`) белела на hover/focus, текст становился нечитаем — тот же корневой баг, что и `CCTextBoxTemplate`/`STPathBox` (FluentTheme красит фокус/hover прямо на `Border#PART_BorderElement` внутри своего шаблона, поверх обычного `Background`-сеттера) — исправлено точечными `:pointerover`/`:focus`/`:focus-within` оверрайдами на тот же template-part. Иконка твика `IconCaterpillar` "уехала вверх" — три точки на плоской линии имели bbox ~19×5 (гораздо шире, чем выше), при `Stretch="Uniform"` в квадратный 18×18 бокс это оставляло почти всю высоту как неиспользуемый slack, который рендерер иконки не центрирует, а анкорит; исправлено не сменой геометрии (остались те же 3 точки — по фидбеку пользователя это осмысленная метафора для твика "блоки спавнятся цепочкой"), а подгонкой `Height` конкретно этого использования `Path` под реальный аспект контента (5 вместо 18), чтобы slack был околонулевым независимо от анкоринга.

**4.6 — Settings.** Статус: ✅ Выполнено. Перед кодом прошли по функционалу донора вручную (без макета — как и Tweaker в 4.5).

**Разбор функционала донора на живое/мёртвое:**
- **Мёртвое, не портировалось:** «[Experimental] Enable in-game overlay» + кнопка «Overlay works bad?» (оверлей удалён — см. 4.5); `GuidancePage` целиком — оба гайда, которые она показывала, про отменённые фичи; `OnMessageRecieved`/`tw-Apply-configuration` — тоже overlay-only (ImGui-панель конфига оверлея пушила свои font/offset настройки обратно в Tweaker); `InstalledServerPackageName`/`DefaultDylanServerName`/`BaseServerPackagePath` в `SettingsProvider` — хвосты Server Swapper (4.4), нигде не показывались в UI, тоже почищены.
- **Живое, портировалось:** пути (текстуры игры, доп. папка скинов), Skin Changer (Hot reload, checksum-guard, safety installation), Textures Watcher (enabled, store-to-temp, override temp + browse/duplicate), UWP-уведомления (allowed + silent).
- **Уточнение по ходу разбора:** «Fast Preview» сначала выглядела как ещё одна мёртвая настройка (спутана с убитым в 4.3 hover-превью карточки). Проверка кода опровергла: `SkinCard`/`SkinChangerViewModel.RefreshSelectedScreenshotsAsync` уже содержат реальную рабочую ветку `UseFastPreview` (эагерная декодировка скриншотов карточки при загрузке вместо decode-при-выборе) — просто никогда не срабатывала, т.к. `SettingsProvider.UseFastPreview` всегда был `false` по умолчанию (см. блокер с конфигом ниже). Решение пользователя: оставить как реальный tradeoff и вынести в Settings — «кэширования нет, сознательно» (Фаза 4.3) в этой части не абсолютно, это дефолт, а не единственный путь.

**Найденный и исправленный блокер, не специфичный для Settings (2026-07-13):** конфиг вообще никогда не загружался в `TweakerUI`. Ни `App.config` не существовал в проекте, ни `MainWindowViewModel` не вызывал `ConfigurationManager.SetUpDefaultSettings()`/`InitializeEnvironment()` (донорский `MainViewModel` делает это первым делом в конструкторе, до создания дочерних VM). На практике это значило, что все `SettingsProvider.*` поля тихо сидели на дефолтах C# (`false`/`null`) во всех смоук-тестах Фазы 4.0-4.5. Исправлено:
- Добавлен `TweakerUI/App.config` (шаблон `<appSettings>`, без мёртвых Overlay/Infopanel/Server-Swapper ключей).
- `MainWindowViewModel` теперь вызывает `SetUpDefaultSettings()`/`InitializeEnvironment()` первым делом в конструкторе; `InitializationFaultCallback` ведёт на отложенный (не блокирующий) тост через `Dispatcher.UIThread.Post(..., DispatcherPriority.Loaded)` вместо WinForms `MessageBox.Show`.
- **Дополнительный найденный баг в этом же коде:** `OpenExeConfiguration` на отсутствующем/неполном конфиге возвращает не-null, но пустой `AppSettings` — `Settings["FirstRun"].Value` кидал `NullReferenceException`, которое `SetUpDefaultSettings()` тихо ловило само же и, что важно, **до** первого `cfg.Save()` — то есть конфиг не мог самовосстановиться в принципе, ни разу, ни при каком количестве перезапусков. Подтверждено отдельным throwaway-проектом. Пофикшено: `EnsureDefaultKeysExist()` добавляет отсутствующие ключи с дефолтами (по списку, зеркалящему `App.config`) и сохраняет **до** проверки `FirstRun`, так что конфиг самовосстанавливается независимо от причины отсутствия/неполноты файла. Проверено: `TweakerUI.dll.config` удалён вручную, приложение перезапущено — файл пересоздался с полным набором ключей и корректным `FirstRun=False`.

**`TexturesWatcher`** (`TweakerUI/Models/`) — портирован, с одним осознанным поведенческим отличием (решение пользователя): в доноре тумблер «Enabled» требовал перезапуска приложения (вотчер создавался только в конструкторе `SettingViewModel`, по чистой лени реализации, не по техническому ограничению). Теперь это единственный такой тумблер, и он сделан живым: `TexturesWatcher.Instance` — lazy-singleton, `TexturesWatcher.Reset()` дозирует его на выключении, так что повторное включение создаёт свежий инстанс. `TexturesWatcher.IfActive` (null-safe) заменяет донорский `AccordingToApplicationConfiguration`. Раз тумблер живой, донорский паттерн «нужен перезапуск» в новом Settings **больше не нужен ни для одной настройки**.

**Побочный багфикс, вскрытый портированием вотчера (2026-07-13):** донорский `InstallSkin` гасил вотчер (`DisableRaisingEvents`) в начале и включал обратно (`EnableRaisingEvents`) в конце — без `try/finally`. Любой ранний `return` навсегда оставлял вотчер выключенным до следующего полного успешного инсталла. В порту обёрнуто в `try/finally`.

**Вёрстка:** `TweakerUI/Themes/SettingsStyles.axaml` (`ST`-префикс) — одностраничная карточная компоновка вместо донорского `TabControl` (решение пользователя: единообразно с Tweaker/Skin Changer/Color Configurator). 4 карточки: Paths, Skin Changer, Textures Watcher, Notifications. Тумблеры — тот же `STSwitch`, но без иконки на строку (обычные preference-переключатели, не твики с конкретным игровым смыслом). Диалоги путей — через `FileDialogService`, никакого WinForms.

**Проверка:** сборка чистая; сквозной smoke-test с удалённым `TweakerUI.dll.config` подтвердил самовосстановление конфига и стабильный запуск/остановку. Пользователь протестировал вручную на реальном профиле (включил Watcher/Store-to-temp — `Storage/temp.tasp`, 20MB, реальный слепок текстур, появился на диске; `/Storage/` добавлен в `.gitignore` по аналогии с `/Skins/`).

**Раунд визуальных правок (2026-07-13):** поля путей (`STPathBox`) грубо ободились ярко-синим на фокусе с непрозрачным выделением текста — тот же корневой баг, что у `CCTextBoxTemplate` в 4.2. Исправлено идентично: полный собственный `STTextBoxTemplate` (без `ScrollViewer`, `TextPresenter` прямо в `Border`), `BrushTransition` на `BorderBrush`, полупрозрачный `SelectionBrush`. Заодно `FontWeight="Medium"` вместо дефолтного Regular — Inter в 13.5px Regular на светлом фоне поля читался как слишком тонкий/резкий по прямому фидбеку.

### Финал фазы

- **Стили:** по решению пользователя рестайлинг идёт по ходу каждого модуля (Avalonia `Styles`/`ControlTheme`, с посильными визуальными улучшениями на каждом шаге), отдельного финального «причёсывающего» этапа не закладываем — но после 4.6 стоит один быстрый проход на визуальную консистентность между модулями (шрифты/отступы/цвета), раз они портировались по отдельности. **Не сделано, не обязательно.**
- **Удаление `SkinChangerRestyle`:** только после того, как `TweakerUI` пройдёт ручную проверку паритета с реальной игрой по всем модулям (пользователь тестирует лично). **Не сделано, гейтится реальным прогоном.** Тогда же: убрать проект из `.sln`, удалить папку, поправить `Installer` (если ссылается на имя/путь `audiosurftweaker.exe` из `SkinChangerRestyle`, перенаправить на выходной `.exe` `TweakerUI`), обновить `Docs/revival-roadmap.md`.
- **Удаление `TweakerScriptsInterpreter`/`TweakerScriptsInterpreter.Tests`:** в связке с удалением `SkinChangerRestyle` выше. Убрать оба проекта из `.sln`, удалить папки.
- **Порядок коммитов:** по аналогии с Фазой 3 — коммит и пуш по каждому завершённому и вручную проверенному модулю, только по явному подтверждению пользователя, не одним гигантским коммитом в конце.

---

## Фаза 5 — QuickPlayer

Финальная фаза, после того как ядро (миграция+чистка+баги+формат+live-IPC) стабильно.

- Из старого `QuickPlayerCore` реально переиспользуемо: `MetadataReader` (обёртка над TagLibSharp) и билдер тегов Audiosurf (`Audiosurf/SongTags.cs`, уже валидирует `[as-4lane]`/`[as-msz{n}]`/`[as-wb{n}]`).
- Не переиспользуется: пустой стаб `TagWriter.WriteTags`, битая эвристика `Codec` через `Enum.TryParse(Path.GetExtension(...))` — выбрасываются, а не чинятся.
- Игровой IPC-интерфейс умеет управлять загрузкой/запуском треков — это подтверждено, но набор команд нигде в проекте не задокументирован. Первый шаг фазы — задокументировать реально используемый словарь команд (тот же `ascommand`/`asconfig`-протокол, которым уже говорит `asbridge` с игрой — см. итоги Фазы 3c), прежде чем проектировать API плеера поверх него.
- Дальше: модель плейлиста + Avalonia UI поверх неё + драйвер последовательного воспроизведения, посылающий команды через уже существующий канал `AudiosurfInterface` → `asbridge` → игра.

**Проверка:** реальное создание плейлиста и подтверждённый переход игры между треками через отправленную IPC-команду.

---

## Автообновление приложения (отложено, вне активной дорожной карты)

Тег `archive/beta-alt-design-updater` (архив бывшей ветки `Beta-Alt-Design`, удалённой при уборке git) содержит рабочий на вид, но полностью WPF-шный in-app автообновлятор (`Updater/` — `net7.0-windows`, `GithubRepoReleaseProvider` для GitHub Releases API, собственные `RelayCommand`/`ViewModelBase`, `Newtonsoft.Json`, `MdXaml` для рендера changelog'а).

Проверено (2026-07-13, разбор архивной ветки): логика простая и рабочая — скачать `Update.zip`, убить целевой процесс по PID, распаковать `.exe`/`.dll`/`.xml` и `.cfg`/`config` поверх существующей установки. Но код целиком на WPF + Newtonsoft — ровно те технологии, от которых уходит этот ревайвл (Avalonia + CommunityToolkit.Mvvm + System.Text.Json). Портировать as-is бессмысленно: пришлось бы переписывать всё равно.

**Решение:** код не портируется, архив используется только как референс дизайна (клиент GitHub Releases API, последовательность «убить процесс → перезаписать бинарники → перезапустить»). Реализация — отдельная будущая задача вне этой дорожной карты, поднимается заново, когда/если появится необходимость. Overlay-стилизация из той же ветки по-прежнему не нужна независимо от судьбы автообновлятора.

---

## Сквозные риски, которые стоит держать в голове

- Все "живые" фичи (Tweaker, будущий QuickPlayer) в любом случае Windows-only из-за `WM_COPYDATA`/HWND внутри `asbridge` — Avalonia даёт кросс-платформенность только файловым фичам (Skin Changer, Color Configurator).
- Замена форматов скинов и цветовых пресетов — breaking change для существующих пользователей; `LegacyDataConverter.exe` обязателен и должен быть протестирован на реальных старых файлах обоих типов, иначе люди потеряют свои скин-библиотеки и/или пресеты при обновлении.
- `asbridge` — компонент с собственным жизненным циклом процесса; реконнект и orphan-protection уже реализованы и проверены (Фаза 3c), но это не тривиальная область — держать в уме при будущих изменениях протокола (Фаза 4.5/5).
- `TweakerOverlay` умышленно вне плана — если он понадобится раньше, чем ожидается, это отдельное обсуждение, не влезающее в эту дорожную карту без реверс-инжиниринга рендеринга игры. С этапа 4.5 старый `InternalOverlayRenderer` удалён из репозитория целиком (не просто заморожен) — будущий оверлей строится с нуля на других механизмах, а не воскрешением этого кода.

# QuickPlayer — архитектура ядра/бэкенда (Фаза 5)

Снимок раздела "Фаза 5 — QuickPlayer" из `C:\Users\Unchp\.claude\plans\iridescent-churning-aurora.md` на момент согласования архитектуры (до начала реализации). UI сознательно не проектируется в этом заходе — только ядро/бэкенд.

**Статус: ✅ Ядро/бэкенд реализовано (все пункты 0-9 ниже), сборка и юнит-тесты (45/45, новый `QuickPlayerCore.Tests`) зелёные, смоук-ран `TweakerUI` чистый. Ждёт коммита/пуша по явному подтверждению пользователя. UI — отдельным заходом позже.**

**Две точечные поправки относительно исходного текста плана, вскрывшиеся при реализации:**
1. **`PlaylistTag` — класс, не `(SongTagToken, int?)`-кортеж.** `System.Text.Json` не сериализует публичные поля `ValueTuple` (только properties) — кортеж молча ушёл бы в пустой `{}` при каждом сохранении плейлиста. `PlaylistEntry.Tags` — `List<PlaylistTag>` с обычными properties `Token`/`Parameter`.
2. **Прогресс-события у `TempFileTagger`/`PlaybackController` — обычные `Action`-и, не прямой вызов `StatusService.Manager.Begin(...)`.** `StatusService` — `internal class` в `TweakerUI.Core`, `QuickPlayerCore` физически не может на него сослаться (и не должен — правило "UI-agnostic core"). `TempFileTagger.TaggingStarted`/`PlaybackController.EntryStarted`/`EntryEnded` — плейн-события, тот же паттерн, что уже есть у `LegacyConverter.ConversionStarted`/`SkinPackager.OperationFailed`. Будущая QuickPlayer-вьюмодель подпишется на них и сама вызовет `StatusService.Manager.Begin(...)` — как `SkinChangerViewModel` уже делает с `LegacyConverter.ConversionStarted`.

**Контекст.** Пользователь предоставил официальный дамп протокола разработчика (`GameProtocol_FromDylan.md` — полный список `ascommand`/`asconfig`/`asreport`, включая ранее не задокументированные в коде `playsong<character>` варианты, `minimize`, `setwindowpositionsize`, репорты `songcomplete`/`oncharacterscreen`/`nowplaying*`) и список игровых тэгов песен (`GameProtocol_SongTags.md` — `[as-4lane]`, `[as-mszX]` и т.п.). Задача этого захода — актуализировать `GameProtocol.cs` по дампу и спроектировать архитектуру ядра плеера: модель плейлиста, хранение записей (путь, отображаемые артист/название, обложка, игровой режим, персональные твики, тэги), применение тэгов (либо через модификацию тега названия во временной копии файла, либо через `asconfig`-команду для тех тэгов, у которых есть игровой твик-эквивалент — с LIFO-снятием по окончании песни), и расширяемый (не захардкоженный) реестр поддерживаемых аудио-форматов.

Пользователь подтвердил: для LIFO-отката тэг-твиков используется общий `GameConfigState`-трекер (единственный источник правды о "последнем отправленном" значении `asconfig`-ключа), а не изолированная от Tweaker-а логика — `TweakerViewModel` получает точечный рефакторинг (7 мест) на использование этого трекера вместо прямого `AudiosurfHandle.Command(...)`.

## 0. Актуализация `GameProtocol.cs` (`AudiosurfInterface/GameProtocol.cs`)

Сверка с дампом показала: все 7 текущих `asconfig`-ключей и 9 из текущих `ascommand`-имён уже верны и совпадают с протоколом дословно — ничего из существующего не меняется. Добавляется:

- `ascommand`: `Minimize = "minimize"`, `SetWindowPositionSize(int x, int y, int w, int h)` — форматирующий хелпер (`"setwindowpositionsize {x},{y},{w},{h}"`), не константа, т.к. параметризован.
- Отчёты (`asreport`, приходят через `AudiosurfHandle.MessageResieved` как уже отделённый от префикса `content`, см. `AsBridgeProtocol.TryParseReport`/`HandleGameBroadcast` — `report.Details[0]` это и есть `content` вида `"songcomplete 142798"`): новые именованные константы `ReportSongComplete = "songcomplete"`, `ReportOnCharacterScreen = "oncharacterscreen"`, `ReportNowPlayingArtist = "nowplayingartistname"`, `ReportNowPlayingTitle = "nowplayingsongtitle"`, `ReportNowPlayingAshFile = "nowplayingashfile"`. (Существующие `successfullyregistered`/`successfullyquickstartregistered`, которые сейчас матчатся инлайн-подстрокой в `AudiosurfHandle.HandleGameBroadcast`, не трогаем — это внутренняя механика `AudiosurfHandle`, не нужна вызывающему коду напрямую.)
- **Сознательно НЕ добавляется**: `registerlistenerwindow`/`quickstartregisterwindow`/`quickstartqueuecommand` — по итогам Фазы 3c эти команды уже отправляет сам `asbridge.exe` при обнаружении окна игры (managed-стороне вручную дёргать их нельзя — второй параллельный `registerlistenerwindow` от QuickPlayer поверх уже сделанной bridge'ем регистрации только всё сломает). Существующая очередь команд в `AudiosurfHandle.Command` (копится, пока `State != Connected`, флашится по `Registered`) уже даёт QuickPlayer тот же практический эффект, что и `quickstartqueuecommand` — отдельно реализовывать не нужно.

## 1. `GameCharacter` — персонажи/режимы воспроизведения (`AudiosurfInterface/GameCharacter.cs`, новый файл)

```csharp
public enum GameCharacter
{
    CurrentCharacter, // не форсировать - играть тем персонажем, что уже выбран на экране персонажей
    Mono, Pointman, DoubleVision, MonoPro, Vegas, Eraser, PointmanPro, Pusher,
    DoubleVisionPro, NinjaMono, EraserElite, PointmanElite, PusherElite, DoubleVisionElite,
    Freeride
}
```
Удачное совпадение: `character.ToString().ToLowerInvariant()` буквально совпадает с суффиксом команды для каждого значения (`DoubleVisionElite` → `doublevisionelite` → `playsongdoublevisionelite`, `CurrentCharacter` → `currentcharacter` → `playsongcurrentcharacter`) — не нужен словарь маппинга, один хелпер в `GameProtocol`: `public static string PlaySong(GameCharacter character, string filePath) => Command($"playsong{character.ToString().ToLowerInvariant()} {filePath}");`. Живёт в `AudiosurfInterface`, а не в `QuickPlayerCore` — это часть игрового протокола, не специфика плеера.

## 2. `GameConfigState` — общий трекер `asconfig` + LIFO-стек оверрайдов (`AudiosurfInterface/GameConfigState.cs`, новый файл)

Тот же архитектурный приём, что уже опробован и одобрен для `StatusService` (Фаза "Статус-бар приложения") — синглтон-фасад (`internal class`, `static Manager`, без DI, по образцу `ApplicationNotificationManager`/`StatusService`), disposable-хендл на каждый temporary-оверрайд, `Dispose()` восстанавливает предыдущее известное значение. LIFO получается "бесплатно" через порядок `using`/явного `Dispose()` — если параллельно активны несколько оверрайдов на разные ключи, они друг другу не мешают (каждый ключ — свой независимый стек).

```csharp
internal class GameConfigState
{
    public static GameConfigState Manager => _instance ??= new GameConfigState();

    // Явная установка (используется и глобальными тумблерами Tweaker-а, и любым кодом, которому
    // не нужен временный оверрайд) - шлёт asconfig и одновременно обновляет "последнее известное" значение.
    public void Set(string key, bool value);

    // Временный оверрайд с восстановлением на Dispose. Снимок "текущего" значения снимается в момент
    // вызова Push - если ключ ещё ни разу не отправлялся никем, восстановление при Dispose не шлётся
    // (нет достоверного "предыдущего" значения - подробнее см. открытый вопрос ниже).
    public IDisposable PushOverride(string key, bool value);
}
```

**Рефакторинг `TweakerUI/ViewModels/TweakerViewModel.cs`:** все 7 `partial void On...Changed` меняют `_audiosurfHandle.Command(GameProtocol.Config(key, value))` на `GameConfigState.Manager.Set(key, value)` — точечная правка, поведение снаружи не меняется (тот же `asconfig`, та же команда), просто появляется общий источник правды. `GameConfigState` сама вызывает `AudiosurfHandle.Instance.Command(...)` внутри `Set`, так что `TweakerViewModel` теряет прямую зависимость от факта отправки, только от факта "значение установлено".

**Решено при реализации (было открытым вопросом, закрыто по замечанию пользователя):** если QuickPlayer оверрайднул ключ, который до этого НИКТО (ни Tweaker, ни другой QuickPlayer-трек) не устанавливал явно — восстанавливать нужно не "ничего не делать", а к `false`. Каждый `asconfig`-ключ здесь — булев тумблер, и у игры есть собственное скрытое дефолтное состояние для непотроганных твиков — это всегда "выключено", пока что-то явно не включило. `PushOverride` берёт `false` как значение по умолчанию и восстанавливает БЕЗ УСЛОВИЯ на Dispose.

## 3. `GameReportListener` — типизированный слой над отчётами игры (`AudiosurfInterface/GameReportListener.cs`, новый файл)

`AudiosurfHandle.MessageResieved` отдаёт сырой `content` (`"songcomplete 142798"`, `"oncharacterscreen"`, `"nowplayingartistname nine inch nails"`...). Вместо того чтобы каждый будущий потребитель (сейчас — только QuickPlayer, но не только в перспективе) заново парсил префикс/остаток руками — тонкая обёртка над `AudiosurfHandle.Instance.MessageResieved`, разбирающая `content` по первому пробелу и поднимающая типизированные события:

```csharp
public sealed class GameReportListener : IDisposable
{
    public event Action<int> SongCompleted;         // score
    public event Action OnCharacterScreen;
    public event Action<string, string, string> NowPlaying; // artist, title, ashFilePath - собираются из 3 последовательных репортов и стреляют одним событием, когда все три получены
}
```
Живёт в `AudiosurfInterface` (не в `QuickPlayerCore`) — это разбор игрового протокола, а не логика плеера, полезен независимо от QuickPlayer.

## 4. Реестр аудио-форматов (`QuickPlayerCore/SupportedAudioFormats.cs`, новый файл)

Прямой ответ на просьбу пользователя "не хардкодом, а в одном месте, чтобы позже подправить руками". Один статический класс со списком поддерживаемых расширений (`.mp3`, `.m4a`, `.flac`, `.wav` — стартовый набор из сообщения пользователя) в виде **изменяемого во время выполнения** списка (не `readonly`/`const`), плюс `IsSupported(string path)`. Всё остальное — сканирование папки при добавлении в плейлист, фильтр диалога открытия файла, drag&drop — обращается сюда, ни одного расширения-литерала больше нигде. Персистентность списка (чтобы пользователь мог сохранить "я добавил .ogg" между запусками) — через `SettingsProvider`, тем же паттерном, что и остальные персистентные настройки (сериализованный список в `App.config`), не в объёме этого захода на код (сейчас — только сам реестр с runtime-изменяемым списком и хардкоженным дефолтом, персистентность настройки — заготовка на будущий Settings-пункт).

Заодно чинится найденный в старом `QuickPlayerCore.MetadataReader` мёртвый код: `Enum.TryParse(Path.GetExtension(pathToFile), out Codec codec)` никогда не парсится успешно (расширение содержит точку — `".mp3"`, а не `"Mp3"`), `Codec` всегда падает в `Unsupported`. Новый `SupportedAudioFormats` даёт единственное место, откуда и `MetadataReader`, и всё остальное берут нормализованное расширение → `Codec` maps (без точки, регистронезависимо).

## 5. Каталог тэгов песен (`QuickPlayerCore/Audiosurf/SongTagCatalog.cs`, заменяет `SongTags.cs`)

Старый `SongTags.cs` — плоский набор `readonly string`-полей, без различения "тэг = суффикс в названии" от "тэг = игровой твик" и с опечаткой (`as-npstlth` вместо задокументированного `as-nostlth`). Новый каталог явно кодирует оба вида и параметризованные тэги:

```csharp
public enum SongTagToken
{
    FourLanes, Portal, MonoOnly, EverybodyMono, MonoLessGrey, MonoAllGrey, MonoNoGrey,
    MonoBasePoints, NoStealth, SidewinderCamera, BankingCamera, FirstPerson, Caterpillar,
    MinimumMatchSize, WhitesBlacksPercent, MatchCollectionTicks, PuzzleRowsCount, HidePuzzleGrid, Steep
}

public sealed class SongTagDefinition
{
    public SongTagToken Token { get; init; }
    public bool HasParameter { get; init; }         // true для msz/wb/mt/prows/monopt
    public string Format(int? parameter);            // "[as-4lane]" или "[as-msz12]"
    public (string ConfigKey, bool Value)? AsConfigBinding { get; init; } // не-null только для SidewinderCamera/BankingCamera
}

public static class SongTagCatalog
{
    public static IReadOnlyList<SongTagDefinition> All { get; }
}
```
Только `SidewinderCamera`(`as-swind`→`sidewinder`) и `BankingCamera`(`as-bankcam`→`usebankingcamera`) получают `AsConfigBinding` — это единственные два тэга из дампа, у которых есть прямой эквивалент среди 7 существующих `asconfig`-ключей `GameProtocol`. Остальные 17 — чистые title-тэги, эквивалента в `asconfig` нет.

## 6. Модель записи плейлиста (`QuickPlayerCore/PlaylistEntry.cs`, `QuickPlayerCore/Playlist.cs`, новые файлы — заменяют недописанный `PlaylistRecord.cs`)

`PlaylistRecord` (существующий стаб) читает теги при конструировании и ни к чему не привязан персистентно — не годится как модель хранимой записи (нет стабильного Id, нет полей под твики/тэги/режим/обложку). Новая модель:

```csharp
public sealed class PlaylistEntry
{
    public Guid Id { get; init; } = Guid.NewGuid();   // стабильный Id, тот же урок, что и с ColorPalette (Фаза 4.2) -
                                                        // одна и та же песня в разных плейлистах, или дважды в одном, не путается по пути/имени
    public string FilePath { get; set; }
    public string ArtistName { get; set; }             // из MetadataReader при добавлении, редактируемо вручную
    public string SongTitle { get; set; }
    public string CoverPath { get; set; }               // резолвится ICoverArtProvider-ом, кэшируется здесь как путь (не байты)
    public GameCharacter Character { get; set; } = GameCharacter.CurrentCharacter;
    public Dictionary<string, bool> ConfigOverrides { get; set; } = new();   // явные asconfig твики, per-song, независимо от глобальных
    public List<(SongTagToken Token, int? Parameter)> Tags { get; set; } = new();
}

public sealed class Playlist
{
    public Guid Id { get; init; } = Guid.NewGuid();
    public string Name { get; set; }
    public List<PlaylistEntry> Entries { get; set; } = new();
    public bool AutoAdvance { get; set; } = true;   // проигрывать следующий трек по songcomplete; точка расширения под shuffle/repeat в будущем - не проектируем сейчас
}
```
`Tags`, у которых есть `AsConfigBinding` (сейчас — `SidewinderCamera`/`BankingCamera`), при воспроизведении резолвятся в тот же оверрайд-стек (`GameConfigState.PushOverride`), что и явные `ConfigOverrides` — с точки зрения плеера это один и тот же набор "что оверрайднуть на время трека", тэги — просто более удобный UI-способ задать типовой оверрайд, не отдельный механизм.

Персистентность — `System.Text.Json`, тем же паттерном, что `ColorPalette`/`.palette` (Фаза 2/4.2): один плейлист — один файл, `QuickPlayer/Playlists/<Id>.json`, путь резолвится от папки exe. **Важно:** резолвинг "папки рядом с exe" уже один раз ловил баг под single-file-publish (Финальная полировка, `Environment.ProcessPath` вместо `Assembly.GetExecutingAssembly().Location`/`AppDomain.CurrentDomain.BaseDirectory`) — новый код для `QuickPlayer/` использует тот же приём (`Environment.ProcessPath`-based), не изобретает резолвинг заново. `QuickPlayerCore` уже зависит от `TweakerCore` (см. его `.csproj`) — резолвер живёт как маленький статический хелпер в `QuickPlayerCore` (не тянем UI-слойный `SkinChangerViewModel.AppDirectory` в core-проект; дублирование одной строки логики между двумя местами — приемлемая цена, чтобы не тащить `TweakerUI`-зависимость в `QuickPlayerCore`).

## 7. Обложки (`QuickPlayerCore/CoverArt/ICoverArtProvider.cs`, `LocalFileCoverArtProvider.cs`)

```csharp
public interface ICoverArtProvider
{
    Task<string> ResolveCoverPathAsync(PlaylistEntry entry); // возвращает локальный путь к файлу обложки или null
}
```
`LocalFileCoverArtProvider` — единственная реализация в этом заходе: ищет `cover.jpg`/`cover.jpeg`/`cover.png` (регистронезависимо, через `Directory.EnumerateFiles`+`OrdinalIgnoreCase`-сравнение, не через паттерн, зависящий от ФС-регистрозависимости) рядом с `entry.FilePath`. MusicBrainz — не реализуется (прямая просьба пользователя отложить), но интерфейс уже даёт место, куда воткнуть `MusicBrainzCoverArtProvider` позже без изменения вызывающего кода (например, композитный provider "сначала локально, потом MusicBrainz" — тоже не реализуется сейчас, просто архитектурно возможен).

## 8. Применение тэгов к файлу (`QuickPlayerCore/TempFileTagger.cs`)

Реализует пожелание пользователя буквально: для тэгов БЕЗ `AsConfigBinding`, активных у записи, создаёт копию исходного файла в `QuickPlayer/Temp/<playlistId>/<оригинальное имя файла>` и переписывает ID3/тег названия через `TagLib` (`file.Tag.Title = $"{entry.SongTitle} {tag1}{tag2}..."`) — оживляет пустой стаб `TagWriter.WriteTags` старого `QuickPlayerCore` (замена, не правка — старый метод ничего не делал). Если у записи нет активных title-тэгов (только `AsConfigBinding`-тэги/явные `ConfigOverrides` или вообще без тэгов) — временная копия не создаётся, играется оригинальный файл напрямую (нет смысла копировать файл ради подмены, которой не будет).

Кэширование по `File.GetLastWriteTimeUtc(entry.FilePath)` + хэш активного набора тэгов, сравнение с уже существующей временной копией (если есть и совпадает — не пересоздаём); при несовпадении — пересоздаём. Дисковая операция небольшая (копия+ретег одного аудиофайла), но раз в проекте уже есть прецедент "конвертация видимо занимает время без обратной связи" (легаси-конвертация скинов) — операция оборачивается в `StatusService.Manager.Begin(StatusToken.DiskProcess, "Quick Player", $"Preparing {entry.SongTitle}...")`, тот же сервис, что и Skin Changer, ради которого он и делался app-wide.

## 9. Драйвер воспроизведения (`QuickPlayerCore/PlaybackController.cs`)

Владеет текущим плейлистом/индексом, координирует всё вышеперечисленное:

```csharp
public sealed class PlaybackController : IDisposable
{
    public void Play(Playlist playlist, int index);
    public void Stop();
    public event Action<PlaylistEntry> EntryStarted;
    public event Action<PlaylistEntry> EntryEnded;
}
```
`Play`:
1. `TempFileTagger` резолвит итоговый путь к файлу (оригинал или тегированная копия).
2. Собранный набор оверрайдов (явные `ConfigOverrides` записи + `AsConfigBinding`-тэги) пушится через `GameConfigState.PushOverride` — все хендлы складываются в **один стек хендлов у самого `PlaybackController`** (не per-key), чтобы гарантированно снять именно те оверрайды, что поставила именно эта запись, при завершении — LIFO-порядок соблюдается автоматически, т.к. `List<IDisposable>` снимается в обратном порядке добавления.
3. `AudiosurfHandle.Instance.Command(GameProtocol.PlaySong(entry.Character, resolvedPath))`.
4. `StatusService.Manager.Begin(StatusToken.Playing, "Quick Player", $"Now playing: {entry.ArtistName} - {entry.SongTitle}...")` — ровно пример пользователя из обсуждения статус-бара.

Завершение трека — три независимых источника, каждый должен гарантированно один раз снять оверрайды/статус (та же идемпотентность, что у `StatusHandle.Dispose`/`_disposed`-флага):
- `GameReportListener.SongCompleted` (штатное завершение — играем следующий трек, если `playlist.AutoAdvance`);
- `GameReportListener.OnCharacterScreen` (пользователь вручную вышел на экран персонажей раньше конца — трактуем как принудительную остановку, не автопереход);
- `AudiosurfHandle.StateChanged` → `NotConnected` (игра/мост отвалились посреди трека — тот же принудительный стоп).

Все три сводятся к одному внутреннему `EndCurrent()` с флагом "уже обработано" (аналог `StatusHandle._disposed`) — какой бы сигнал ни пришёл первым, повторные не приводят к двойному попу стека/двойному диспоузу статуса.

## Расположение кода

Всё, что касается протокола/состояния игры (`GameCharacter`, `GameConfigState`, `GameReportListener`) — в `AudiosurfInterface` (уже зависимость `TweakerViewModel`/`QuickPlayerCore`, ничего не тянет обратно). Всё, что касается плейлиста/тегов/файлов (`SupportedAudioFormats`, `SongTagCatalog`, `PlaylistEntry`/`Playlist`, `ICoverArtProvider`, `TempFileTagger`, `PlaybackController`) — в существующем `QuickPlayerCore` (уже ссылается на `AudiosurfInterface`+`TweakerCore`+`TagLibSharp`, ничего добавлять в `.csproj` не нужно). `TweakerCore` не трогается вообще. Единственная правка вне новых файлов — точечный рефакторинг 7 мест в `TweakerUI/ViewModels/TweakerViewModel.cs` (п. 2 выше).

## Что осознанно вне объёма этого захода
- UI (плейлист-экран, драг-дроп добавления треков, редактор тэгов/твиков записи) — отдельный заход, после того как пользователь согласует этот бэкенд.
- `MusicBrainzCoverArtProvider` — отложено по прямой просьбе пользователя, только интерфейс-заготовка.
- Персистентность списка поддерживаемых форматов через Settings UI — заготовлен только сам реестр.
- Shuffle/repeat-стратегии порядка воспроизведения — `Playlist.AutoAdvance` это простое linear-advance по индексу, точка расширения оставлена, но не проектируется.

## Проверка
Юнит-тесты (новый `QuickPlayerCore.Tests`, по аналогии с `TweakerCore.Tests`): `SongTagCatalog.Format` на все токены + параметризованные варианты, `SupportedAudioFormats.IsSupported` на регистр/точку, `TempFileTagger` round-trip (заголовок реально переписался, кэш реально не пересоздаёт файл при повторном вызове с теми же тэгами), `GameConfigState.PushOverride`/`Dispose` LIFO на синтетическом in-memory сценарии (без реальной игры — просто проверка порядка вызовов `Set`). Ручная проверка с реальной игрой (создание плейлиста, реальный переход между треками по `songcomplete`, реальный откат `sidewinder`/`usebankingcamera` после трека с тэгом) — за пользователем, как и раньше для всех IPC-функций, т.к. песочница не может поднять реальный Steam/Audiosurf.
# Полевой журнал реверса Audiosurf — запуск, кадр движка и жизненный цикл групп

Пятый журнал в серии. Первые четыре отвечают на вопрос «как устроено то, что уже работает»
(`-engine`, `-gameplay`, `-geometry`, `-render`, `-lua`). Этот отвечает на другой, который до сих пор
нигде не был закрыт и стоил нам конкретного бага:

> **С какого момента граф каналов Quest3D вообще что-то значит, и как это увидеть изнутри процесса.**

Повод — симптом из `plugin-offline-mode.md`, Ф8: с ранней загрузкой из `engine\channels\` Lua-скрипты
начинают исполняться, пока игра ещё грузится, заспамливают notefeed ошибками и рисуют HUD поверх
загрузочного экрана. Владелец верно определил, что чинится это не флажком, а отдельным блоком работ,
которому нужен реверс самой игры. Вот он.

Читать вместе с `reversing-journal-engine.md` — там ABI канала, раскладка объекта и цена операций.
Здесь — **что происходит вокруг** этого ABI: кто зовёт кадр, в каком порядке появляются группы, и
почему значение, прочитанное из канала «слишком рано», выглядит правдоподобным и при этом неверно.

Практическое применение всего изложенного — `Docs/Internal/lua-engine-fix-roadmap.md`.

---

## Итог одним абзацем

Кадр движка — это **один** вызов `EngineControl::EngineLoop()` из главного цикла `QuestViewer.exe`;
внутри него ровно один `IncreaseTreeCount()` и ровно один `CallStartChannel()` на **стартовой
группе**, а вся остальная работа графа — это обход по ссылкам из неё. Стартовая группа при запуске
не та, что в игре: сначала это загрузчик (`start - project loader.cgr`), и только когда он досчитал
свой прогресс до 1, канал типа `SetNewStartChannel` переводит движок на
`StartGroup::Do_StartFromExternal` в `XX_StartHere.cgr`. **Эта передача управления и есть
единственный точный признак «игра загрузилась»**, и он наблюдаем снаружи без резолва каналов по
имени. Читать вместо него состояние игры из канала нельзя: `.cgr` хранит значение каждого листа
таким, каким автор оставил его в редакторе, так что до первого исполнения графа `StartupState`
читается как 2 («меню»), а `ini_Initialized?` — как 1 («конфиг разобран»), и оба врут. Отличить
«движок это посчитал» от «так было сохранено» позволяет поле `channelCalculatedAtCount_` (`+0x10`)
против `A3d_ChannelGroup::GetTreeCalculateCount()`. Группы при этом приходят и уходят в течение всей
сессии (пул `Renderer` пересоздаётся на каждый заезд), а указатель `EngineInterface*` доступен с
первого же кадра через `EngineControl::GetEngineInterface()` — то есть детур на
`A3d_Channel::CallChannel`, которым плагин ловит движок сейчас, не нужен вовсе.

---

## 1. Кто зовёт кадр

### 1.1 `QuestViewer.exe` держит `EngineControl`, а не `EngineInterface`

`reversing-journal-engine.md` §4.1 приводит главный цикл как `engine->vtable[+0x0c]()` и называет
объект «движком». Уточнение: объект по адресу `0x0040af88` — это **`EngineControl`**, отдельный
класс `HighPoly.dll` (`??_7EngineControl@@6B@` @ RVA `0x13660`), а не `EngineInterface`. Сходится по
слотам, которые дёргает цикл:

| Слот | Смещение | Метод | Где в цикле |
|---|---|---|---|
| 3 | `+0x0c` | `EngineControl::EngineLoop` | апдейт кадра |
| 9 | `+0x24` | `EngineControl::GetIfKeepRunningApplication` | выбор `PeekMessage` / `GetMessage` |
| 5 | `+0x14` | `EngineControl::GetEngineInterface` | — |
| 1 | `+0x04` | `EngineControl::LoadProject` | однократно при старте |

`QuestViewer.exe` вообще **не импортирует `HighPoly.dll`** — в его таблице импорта только
`KERNEL32`, `USER32`, `ADVAPI32`, `urlmon`. Модуль грузится руками, точка входа —
экспортируемая C-функция **`GetEngineControlInstance`** (RVA `0x10dd0`).

Осторожно: `GetEngineControlInstance` — это **фабрика**, а не аксессор. Она делает
`operator new(0x7e8)`, кладёт vptr и обнуляет поля. Позвать её «чтобы узнать адрес движка» нельзя —
получится второй, пустой `EngineControl`.

### 1.2 `EngineControl::EngineLoop` — весь кадр графа

RVA `0x10b10`, экспортируется как `?EngineLoop@EngineControl@@UAEXXZ`:

```c
void EngineControl::EngineLoop() {
    if (this->inLoop /* +0x3fc */)  return;          // реентрантность
    if (!this->running /* +0x3f4 */) return;
    this->inLoop = 1;

    EngineInterface*    engine = this->engine_ /* +0x04 */;
    EngineInterfaceExt* ext    = engine->ext_  /* +0x40 */;
    ext->vtable[+0x28]();                            // IncreaseTreeCount()

    if (engine) {
        ext = engine->ext_;
        int idx = ext->vtable[+0x6c]();                      // GetStartGroup() -> int
        A3d_ChannelGroup* g = engine->vtable[+0x24](idx);    // GetChannelGroup(int)
        this->startGroup_ /* +0x08 */ = g;
        if (g) g->vtable[+0x60]();                           // CallStartChannel()
    }

    this->inLoop = 0;
}
```

Три следствия, каждое практическое:

- **`IncreaseTreeCount` — ровно один раз за кадр.** Это закрывает открытый вопрос
  `reversing-journal-engine.md` §10 «кто и как часто инкрементит глобальный tree count». Значит
  «кадр» в смысле мемоизации (`CheckRenderCount`, §4.3 того же журнала) — это ровно один
  `EngineLoop`, и счётчик тикает **даже если стартовой группы нет**.
- **`CallStartChannel` вызывается для ОДНОЙ группы за кадр — стартовой.** Формулировка §4.2
  engine-журнала («ровно один вызов на группу за кадр… по числу живых групп») вводит в заблуждение:
  остальные 160 групп за кадр так не вызываются вовсе, до них доходят по ссылкам из стартовой.
  Детур на `CallStartChannel` поэтому стоит **один переход за кадр**, а не 161.
- **`EngineInterface*` достаётся из `this` бесплатно и с первого кадра** — либо полем `+0x04`, либо,
  без опоры на смещение, вызовом `GetEngineInterface()` (слот 5). Отложенность захвата, описанная в
  §7 engine-журнала («игроку может понадобиться что-нибудь кликнуть»), — свойство **того** способа
  ловли, а не свойство движка.

**`EngineLoop` — правильная точка подвеса для покадровой логики плагина.** Она даёт «до графа» и
«после графа» на потоке движка, не зависит от D3D и от того, рисуется ли оверлей, и стоит один
трамплин. `CallStartChannel` для этого хуже: его умеет звать не только движок (§6.4).

### 1.3 Что ещё есть у `EngineInterfaceExt`

Дамп `??_7EngineInterfaceExt@@6B@` @ RVA `0x13260` — там лежит почти весь ответ на вопрос «в каком
состоянии движок»:

| Слот | Смещение | Метод | Что это |
|---|---|---|---|
| 5 | `+0x14` | `UpdateChannelTree` | то же тело, что делает `EngineLoop` ниже `IncreaseTreeCount` |
| 10 | `+0x28` | `IncreaseTreeCount` | кадр мемоизации |
| 22 | `+0x58` | `SetGraphRunning` | |
| 23 | `+0x5c` | `GetGraphRunning` | `return this->byte[+0x05]` — плоский флаг |
| 25 | `+0x64` | `GetQ3DStartGroup` | **грузит** группу из `.q3d`, если её нет; не аксессор |
| 26 | `+0x68` | `SetStartGroup(int)` | |
| 27 | `+0x6c` | `GetStartGroup() -> int` | **индекс текущей стартовой группы** |
| 43 | `+0xac` | `GetInitialized` | `return this->byte[+0xbd4]` |

`EngineInterface::GetGraphRunning` (слот 21, `+0x54`) — просто форвардер на `ext`.

**`GetStartGroup()` + `EngineInterface::GetChannelGroup(int)` — это ровно то, что делает сам
`EngineLoop`**, то есть самый дешёвый и самый честный способ спросить «какая группа сейчас главная».
`GetQ3DStartGroup()` для этого **не годится**: при промахе она загружает группу с диска и показывает
`MessageBoxA`.

---

## 2. Порядок загрузки

### 2.1 Что запускается первым

`engine/Q3DStart.q3d` — контейнер `ACTF` с zlib-потоком со смещения 56. Внутри, открытым текстом,
имя стартовой группы:

```
start - project loader.cgr
```

То есть первая загруженная группа — **не `XX_StartHere.cgr` и не `Preloader.cgr`**, а стандартный
загрузчик проекта Quest3D (1236 каналов), вместе с его спутниками `load group sequence.cgr`,
`load single group.cgr`, `progress calculator.cgr`, `group list handler.cgr`.

`Preloader.cgr` (267 каналов, `Preloader_config.xml`, `projectName="PreloadedAudiosurf"`) — **вторая
цель публикации**, не живой путь. Механика у неё та же, и разобрана она ниже как более простой
пример: там всё то же самое, но в тридцать раз компактнее.

### 2.2 `Preloader.cgr` — та же схема, читаемая целиком

```
#0   Project Start          -> #17 "Load and start"
#17  Load and start:
       #266 OneTime         -> #3   (Initialise)
       #23  If NotInEditor  -> Win32 SetWindowText / SetWindow / FillRect
       #1   If FrameCounter>0            -> #5  Do_RenderD3D   (Clear / BeginScene / анимация / End+Present)
       #6   IfElse FrameCounter > #39    -> #8  Do_RunMainProject
       #9   FrameCounter += TC
#39  FramesBeforeStartLoad = 3
#8   Do_RunMainProject:
       #12 OneTime -> #15 Lua Script "Load CGR"( "XX_StartHere.cgr", "StartGroup", pool 0 )
       #169 If FrameCounter >= #119 -> #37 SetNewStartChannel( #38 )
#119 MinIntroLoadFrames = 70
#38  ChannelCaller, ВНЕШНИЙ -> ("Do_StartFromExternal", "StartGroup")
```

Три кадра рисуется заставка, на четвёртом один `Lua Script` синхронно грузит `XX_StartHere.cgr` в
пул `StartGroup`, дальше крутится анимация, и на 70-м кадре `SetNewStartChannel` переводит движок на
`XX_StartHere::Do_StartFromExternal` (#5338).

### 2.3 Живой путь: `start - project loader.cgr`

Крупнее, но устроен идентично. Стартовый канал `#0 "Load Project call"`, внутри — экран
предупреждения о светочувствительности, прогресс-бар и вызовы в `load group sequence.cgr`
(`Initialise call`, `Load Sequence of groups call`, `Progress (0-1) val`).

Передача управления — `#256 SetNewStartChannel` → `#257` (внешний,
`StartGroup::Do_StartFromExternal`). Условий на неё два, и они интересны сами по себе:

```
#335 If ((load group sequence::Progress (0-1) val >= 1)
         && ReadyToAdvancePastNews?
         && !Quickstart?)                                 -> handover

#334 Trigger ((Quickstart?
               && (load group sequence::Progress (0-1) val >= 1)
               && ReadyToAdvancePastNews?)
              || (SeizureWarning_SecondsViewed > 1))      -> Do_Quickstart (#572) -> handover
```

**Это собственное определение игры «всё загрузилось»**, и главное слагаемое в нём —
`load group sequence::Progress (0-1) val`, доля загруженных стартовых групп в диапазоне 0…1
(счётчик — `progress calculator::Number of groups loaded so far`).

Состав того, что грузится, задан `XX_StartHere_config.xml`: 81 файл (`Actors/*`, `Environment/*`,
`Intros/*`, `Render/RenderCommon`, `Scores/StatCollector`, `SongSelector/*`, `Stage/*`, `Support/*`,
`gui/*`, `sounds/*`). Остальные 80 из 161 группы в поставке — динамические (см. §6).

### 2.4 Что делает `SetNewStartChannel`

`Aco_SetNewStartChannel::CallChannel`, RVA `0x10f0` в `3FF51E2F-….dll`:

```c
A3d_Channel*      target = this->GetChild(0);
A3d_ChannelGroup* group  = target->GetChannelGroup();
EngineInterface*  engine = this->engine_ /* +0x08 */;

group->vtable[+0x64]( target->GetChannelIDIndexNr() );   // A3d_ChannelGroup::SetStartChannel(int)
engine->ext_->vtable[+0x68]( group->vtable[+0x1c]() );   // EngineInterfaceExt::SetStartGroup(GetGroupIndex())
```

То есть ровно два эффекта: у группы выставляется её собственный стартовый канал, и движку говорится,
что теперь главная — эта группа. Оба наблюдаемы экспортированными аксессорами:
`A3d_ChannelGroup::GetStartChannel()` (RVA `0x9620`, буквально `mov eax,[ecx+0x38]; ret` — заодно
подтверждает смещение `+0x38` из §4.2 engine-журнала) и `EngineInterfaceExt::GetStartGroup()`.

**Признак «игра загрузилась», который мы будем использовать:**

> стартовая группа движка — та, чей `GetChannelGroupFileName()` равен `XX_StartHere.cgr`
> (а `GetPoolName()` — `StartGroup`), **и** мы видели по крайней мере один `EngineLoop` в этом
> состоянии.

Он одинаково верен и при ранней загрузке (мы видим переход), и при позднем инжекте в уже идущую игру
(мы видим состояние). Это его главное достоинство перед любым «поймать событие».

### 2.5 Что происходит сразу после передачи

`XX_StartHere::Do_StartFromExternal` (#5338) → `Start` (#3765):

```
OneTime #3769:  ini_Initialized? := 0 ;  CurrentRadioSong := -1
Do_ReadConfigFile_ (#3700):
    OneTime: config.ini := blank ; Group Loader #3702 читает "./../config.ini" в Buffer
    Trigger (Group Loader Status #3704 == 1) -> Do_ParseConfigFile (#3708)
        Lua Script_ParseConfig.ini -> ini_ResolutionWidth / Height / Fullscreen?
        ... ; Set Value #3768:  ini_Initialized? := 1
Do_DiscoverWorkingDirectory (#4687):  Install Path, LanguagePack, BusyBee::Do_PreloadAllTextures
if (ini_Initialized?) {
    Player::Do_ReadMouseInput
    StartChannel (#900)          <-- вся игра: Clear / BeginScene / Do_StartupScreens / ... / Present
}
```

Значит между передачей управления и первым настоящим кадром игры проходит ещё несколько кадров
(асинхронное чтение `config.ini` через `Group Loader`). В `OneTime #1802` внутри `StartChannel`
обнуляется `StartupState` и ещё семь флагов — то есть **реальное значение `StartupState` появляется
позже, чем группа**.

---

## 3. Почему нельзя просто прочитать состояние из канала

Это главный вывод журнала, и он общий, а не про конкретный канал.

**`.cgr` хранит значение каждого листа таким, каким оно было в редакторе в момент сохранения
проекта.** Чанк `FLVA` числового канала, `STVA` строкового — это не «начальное значение» в смысле
«ноль», это снимок рабочего стола автора. Поэтому канал, который ещё ни разу не исполнялся, отдаёт
**правдоподобное и неверное** число, а не признак «я не знаю».

Два примера, оба — ровно те кандидаты, на которые тянет опереться:

| Канал | Хранится в файле | Когда становится настоящим |
|---|---|---|
| `StartGroup::StartupState` (#855) | `2` = `State_MainMenu` | обнуляется `OneTime #1802` внутри `StartChannel`, то есть уже после передачи управления |
| `StartGroup::ini_Initialized?` (#3764) | `1.0` | `Set Value #3766` кладёт `0` в `OneTime #3769` на первом кадре `Start`, и только потом `#3768` возвращает `1` |

`ini_Initialized?` — особенно поучительный случай: читая его «слишком рано», получаешь **правильный
ответ по неправильной причине**, и это работает ровно до дня, когда перестанет.

Отсюда правило, которое стоит держать в голове при любом внешнем чтении графа:

> **Значение канала имеет смысл только после того, как движок этот канал посчитал. До этого читается
> редактор, а не игра.**

### 3.1 Как отличить одно от другого: `channelCalculatedAtCount_`

`A3d_Channel::CheckRenderCount` (`reversing-journal-engine.md` §4.3) пишет в поле `+0x10` номер
кадра, в котором канал посчитан:

```c
if (this->ignoreTreeCount /* +0x60, флаг CHIC */) { this->recalcCount = 0; return true; }
int now = group ? group->GetTreeCalculateCount() : engine->GetTreeCalculateCount();
if (this->calculatedAt /* +0x10 */ != now) { this->calculatedAt = now; this->recalcCount = 0; return true; }
this->recalcCount++; return false;
```

Значит для канала **с `CHIC = 0`** (90.6 % каналов проекта) сравнение

```
channel->calculatedAt (+0x10)  ==  channel->GetChannelGroup()->GetTreeCalculateCount()
```

отвечает на вопрос **«движок вычислял этот канал в текущем кадре»** — не косвенно, а по тому самому
полю, которым он сам себя мемоизирует. Это даёт признак «живости» любой ветки графа: не «загрузилась
ли группа», а «исполняется ли то, что мне нужно, прямо сейчас».

`A3d_ChannelGroup::GetTreeCalculateCount` экспортируется (RVA `0x9840`, невиртуальный `QAE`) и равна
`this->+0x6c->vtable[+0x14]() + this->+0x54`, то есть глобальный счётчик движка плюс собственный
счётчик группы — это подтверждает догадку §10 engine-журнала.

Три оговорки, без которых признак превращается в ловушку:

- **`CHIC = 1` → поле не пишется никогда.** У `Array Value` это 92.8 % экземпляров, у `Array Text` —
  83.8 %. Для таких каналов ответ честно «неизвестно», и это надо возвращать как третье состояние, а
  не как «нет». Флаг читается один раз при резолве: `*(uint8_t*)(chan + 0x60)`.
- **Счётчик кольцевой** (обнуление на 30000, см. `IncreaseTreeCount`) — сравнивать можно только на
  равенство.
- **Поле пишется при вычислении, а не при вызове.** Канал, который движок «позвал» (`CallChannel`),
  но чей `GetFloat` никто не дёрнул, поля не тронет.

---

## 4. Признаки состояния: сводная таблица

Что реально можно спросить у движка снаружи, по убыванию надёжности.

| Вопрос | Чем отвечать | Цена | Оговорки |
|---|---|---|---|
| Движок вообще тикает? | видели ли мы `EngineControl::EngineLoop` | ноль (мы в нём) | — |
| Какой сейчас кадр графа? | `EngineInterface::GetTreeCalculateCount()` | один вызов | кольцевой |
| Игра загрузилась? | стартовая группа == `XX_StartHere.cgr` (`GetStartGroup` → `GetChannelGroup(int)` → `GetChannelGroupFileName`) | два вызова | см. §2.4 |
| Сколько групп сейчас есть? | `EngineInterface::GetChannelGroupCount()` | один вызов | меняется весь сеанс, см. §6 |
| Загружена ли группа X? | `GetChannelGroup(name, 0)` / `GetPoolGroup(pool, 0)` | линейный скан по списку групп | не по каналам, то есть дёшево |
| Идёт ли догрузка стартовых групп? | `load group sequence::Progress (0-1) val` | резолв + `GetFloat` | нужен резолв по имени, то есть после §3 |
| Этот канал сейчас живой? | `+0x10` против tree count группы | две загрузки | только при `CHIC = 0` |
| Идёт ли заезд? | `StartupState == State_Gameplay` (`gameplay`-журнал §3.9) | два `GetFloat` | **только после того, как признак «игра загрузилась» поднялся** |

Последняя строка — суть всей истории: существующий тест «идёт заезд» верен, он просто задавался
слишком рано.

---

## 5. Загрузочные типы каналов

Инвентарь по всему проекту — типы, которыми игра управляет группами:

| Тип | Экземпляров | Где | Что делает |
|---|---|---|---|
| `Group Loader` | 15 | Preloader, MainMenu, XX_StartHere, RadioBrowser, PlaylistManager, project loader | асинхронно читает файл в `Buffer` |
| `Group Loader Status` | 12 | рядом с каждым | `0` — идёт, `1` — готово (проверяется как `A==1` / `A==0`) |
| `Load ChannelGroup` | 5 | XX_StartHere #102/#138, RadioBrowser #528 | грузит `.cgr` из буфера в именованный пул |
| `Remove Group` | 5 | XX_StartHere #85/#125, RadioBrowser ×3 | выгружает пул |
| `IsChannelLoaded` | 3 | XX_StartHere #97/#135, MainMenu #369 | есть ли группа |
| `Group Array Pointer` | 4 | рядом | выбор элемента массива групп |
| `GroupSelfSave` | 11 | Options, SavedConstants, Scores, QuickPicks… | сохранение группы на диск |
| `DynamicGroupLoading` / `DynamicLuaLoading` | — | | догрузка контента |

Все «числовые» из них (`Group Loader Status`, `IsChannelLoaded`, `Group Array Pointer`) — наследники
`Aco_FloatChannel`, то есть читаются обычным числовым аксессором.

---

## 6. Жизненный цикл групп — он не заканчивается загрузкой

### 6.1 Пулы, которые пересоздаются во время игры

`XX_StartHere::Do_ResetLevelContent` (#110):

```
OneTimeReset #83
RemoveGroup  #85   ( Pool Name = "Renderer" )
Flush Video Memory #109
```

и парный загрузчик:

```
#93  Group Loader "GroupBuffer"  <- ChannelSwitch #80 по CurrentGroup:
                                   ".\Render\Render1.cgr" … ".\Render\Render9.cgr"
#97  IsChannelLoaded
#102 Load ChannelGroup "Array_LoadGroup" ( GroupBuffer, Pool "Renderer", CurrentGroup )
```

То есть **пул `Renderer` выгружается и загружается заново на каждый заезд**. Второй такой пул —
`FreeVersionAd` (#125/#138, `Do_ServerAuthorization`).

Практический вывод для любого инструмента, который кэширует `A3d_Channel*`: **указатель в
динамическую группу живёт меньше, чем сессия игры**, и переживает не больше одного заезда. Для
плагина, который вдобавок подменяет vptr объекта на свою копию таблицы, это не «протухший хендл», а
падение: деструктор канала пойдёт по нашей копии, а копия переживёт объект.

### 6.2 Чем выгрузка наблюдается

`Aco_RemoveGroup::CallChannel` (RVA `0x10e0` в `BAC7326D-….dll`), разобранная целиком:

```c
A3d_Channel* poolName = this->GetChild(0);
A3d_Channel* index    = this->GetChild(1);
...
A3d_ChannelGroup* g = engine->vtable[+0x28]( poolName->GetString(), (int)index->GetFloat() );  // GetPoolGroup
if (!g) return;
if (g == this->GetChannelGroup()) { ShowDebugMessage("..."); return; }   // себя не выгружаем
engine->vtable[+0x34]( g->vtable[+0x1c]() );   // DeleteChannelGroup( GetGroupIndex() )
```

Значит точек перехвата две, обе экспортируются по имени и обе на холодном пути:

- `?DeleteChannelGroup@EngineInterface@@UAEXH@Z` (RVA `0x5c90`) — путь `Remove Group`;
- `?Release@A3d_ChannelGroup@@UAEXXZ` (RVA `0x8b00`, слот 1) — общий путь, включая
  `EngineInterfaceExt::ReleaseGroups` при выходе.

Второй шире и поэтому правильнее: разрушение группы проходит через него в любом случае.

### 6.3 Появление групп

Отдельного события нет, и искать его не надо: `EngineInterface::GetChannelGroupCount()` — один
вызов, и одного сравнения за кадр достаточно, чтобы заметить и появление, и исчезновение. Именно на
этом (пусть и вслепую, без понимания причин) держался существующий «write gate» в
`lua_api.cxx: tick()`.

### 6.4 Группами управляет ещё и собственная Lua игры

Скан импортов всех 242 channel-DLL: `?CallStartChannel@A3d_ChannelGroup@@UAEXXZ`,
`?SetStartChannel@…` и `?Release@A3d_ChannelGroup@@UAEXXZ` импортируются ровно двумя модулями —
`Lua Script` (`Aco_Lua`) и `LoadBufferIntoChannel`. Сходится с `reversing-journal-lua.md` §5, где у
игровой Lua есть `q.LoadChannelGroup()`, `q.RemoveChannelGroup()`, `q.LoadChannelGroupDQ()`.

**Практический вывод:** `CallStartChannel` — не только движковая точка входа, её может дёрнуть
игровой скрипт. Строить на предположении «один вызов за кадр» нельзя; спина кадра должна висеть на
`EngineLoop`.

---

## 7. Семейства каналов: полная перепись

Способ, которым получена таблица, стоит отдельного слова, потому что он дешевле реверса и точнее
догадок: **channel-DLL импортирует конструктор своего базового класса из DLL этого базового
класса.** Скан `??0Aco_*Channel@@QAE@XZ` в таблицах импорта всех 242 модулей даёт дерево
наследования целиком, за секунды и без дизассемблера.

| Базовый класс | Производных типов | Типы |
|---|---|---|
| `Aco_FloatChannel` | 30 (+ сам `Value`) | Array InfoValue, Array Value, AssociativeArray_Value, BASS_GetSongPosition, CollisionRayCheck, CollisionSphereCheck, DetectMouseCollision, Envelope, Expression Value, FiniteStateMachine, Group Array Pointer, Group Loader Status, Inertia, IsChannelLoaded, Lua Script, MediaTexture Info Value, Mersenne_Twister, MouseInsideViewRect, NotInEditor, Selector, Set Value, SysInfo, TickCount, Trigger, UniqueTickCount, UserInput, ValueOperator, Win32 DirectoryInfoValue, Win32 MessageBox, XML_NumChildNodes |
| `Aco_StringChannel` | 17 (+ сам `Text`) | Array Text, FetchUnixTime, FileDialog, GetProgramPath, HTTP_Fetch, KeyboardCapture_Unicode, LanguagePack_Translate, TextOperator, Text_Parse, TypeText, Unicode_CurrentDirectory, Unicode_Merge, Win32 DirectoryInfoText, Win32 StartProgram, Win32 TextOut, XML_NodeAttribute, XML_NodeText |
| `Aco_DX8_ObjectDataChannel` | 15 (+ сам `3D ObjectData`) | 3DText, 3DTextFromTexture, CustomGeometry, FastMultiObjectData, HLSLObject, MorphObject, NaturePainter, ParticleObject, Primitive, SkinnedCharacter, Texter_Unicode, Tune_Car, Tune_Forest, Tune_Thruster, Tune_Wall |
| `Aco_MatrixChannel` | 7 (+ сам `Matrix`) | Array Matrix, InterpolateRotationQuaternions, MatrixMotion, MatrixOperator, Motion, ODE Body, ProjectionMatrix |
| `Aco_DX8_D3DDeviceUse` | 7 | 3D ObjectData, Sprite_Unique, StencilShadow, StencilShadowBaseObject, Surface, Textout, Texture |
| `Aco_DX8_Texture` | 5 (+ сам `Texture`) | CubeTexture, CustomTexture, CustumTexture05, MediaTexture, RenderTexture |
| `Aco_VectorChannel` | 3 (+ сам `Value Vector`) | Array Vector, BASS_GetLevels, VectorOperator |
| прочие | 134 | собственные корни: команды, рендер, ввод, файлы, сеть |

Расхождение с таблицей по `baseguid` из `reversing-journal-engine.md` §3.2 (там у строковых 19, тут
18) — у метода есть предел: тип, чей конструктор заинлайнен или вызван не по имени, в скан не
попадает. Как **карта того, где что лежит**, таблица верна; как счётчик — сверяться с `baseguid`.

### 7.1 Слоты 17–19 по семействам, и что будет, если ошибиться

Дампы сняты с `.rdata` соответствующих DLL (`uvrun vtdump --type "<имя>"`).

| Семейство | слот 17 (`+0x44`) | слот 18 (`+0x48`) | слот 19 (`+0x4c`) |
|---|---|---|---|
| `Aco_FloatChannel` | `GetFloat` → float | `GetOldFloat` | `SetFloat(float)` |
| `Aco_StringChannel` | `GetString` → `const char*` | `SetString(const char*)` | `SetSingeLine(bool)` |
| `Aco_VectorChannel` | `GetVector` → `D3DXVECTOR3` | `SetVector(D3DXVECTOR3)` | **`SetFloat(int, float)`** |
| `Aco_MatrixChannel` | `GetMatrix` → `D3DXMATRIX` | `GetOldMatrix` *(тот же адрес)* | `SetMatrix(D3DXMATRIX)` |
| `Aco_DX8_D3DDeviceUse` | `InvalidateDeviceObjects` | `InvalidateDirectGraphics` | *(конец таблицы)* |
| `Aco_DX8_Texture` | `InvalidateDeviceObjects` | — | **`Release`** |
| `Aco_DX8_ObjectDataChannel` | `InvalidateDeviceObjects` | — | **`Release`** |
| `Aco_DX8_ObjectChannel` (`3D Object`) | **`DrawSurfaces`** | `DrawTransparentSurfaces` | `GetObjectMatrix` → `D3DXMATRIX` |

**Матричный ABI закрывает открытый вопрос §10 engine-журнала.** Таблица `Aco_MatrixChannel`
кончается на слоте 20 (дальше в `.rdata` строка `"Matrix"`), своих слотов ровно три, `D3DXMATRIX`
(64 байта) передаётся и возвращается по значению — то есть возврат идёт через скрытый указатель
первым стековым аргументом, как у `GetVector` и `GetChannelType`.

И — то, ради чего эта таблица здесь целиком. Правило «проверяй тип до вызова» до сих пор
иллюстрировалось тем, что `GetFloat` на `Text` вернёт не то число. Теперь видно, что это **самая
мягкая** из возможных ошибок:

- `GetFloat` (слот 17) на канале `Texture` или `3D ObjectData` — это `InvalidateDeviceObjects()`:
  игра теряет ресурс устройства;
- `GetFloat` на `3D Object` — это `DrawSurfaces()`: объект **рисуется** посреди чужого кадра;
- `SetFloat(v)` (слот 19) на `Texture` или `3D ObjectData` — это `Release()`, и вдобавок с лишним
  аргументом на стеке;
- `SetFloat(v)` на `3D Object` — это `GetObjectMatrix()`, которая примет наше `v` за указатель на
  приёмник 64-байтной матрицы и **напишет туда**;
- `SetFloat(v)` на `Value Vector` — рассогласование стека (`SetFloat(int, float)`), как уже
  зафиксировано в §2.2.2 engine-журнала.

Ни одна из этих ошибок не выглядит как ошибка в момент совершения.

### 7.2 Побочный результат: `3D Object` — готовая точка перехвата отрисовки

Слот 17 у `Aco_DX8_ObjectChannel` — `DrawSurfaces`, слот 18 — `DrawTransparentSurfaces`. То есть
механизм `channel_shim` (подмена vptr одного объекта) применим к **любому 3D-объекту игры**
напрямую: подменив слот 17 у конкретного `3D Object`, можно не рисовать его, нарисовать до/после
него или подменить состояние устройства вокруг. Это дешевле и точнее, чем `mute` на
`ChannelCaller` выше по дереву, и относится к той же работе, что §8 `reversing-journal-render.md`.
Здесь — только запись факта; в план работ по скриптам это не входит.

---

## 8. Сводка для плагина

Всё — экспорт по имени из `HighPoly.dll`, то есть `GetProcAddress` без сканирования сигнатур.

```
Кадр и движок
  ?EngineLoop@EngineControl@@UAEXXZ                        спина кадра (детур), RVA 0x10b10
  ?GetEngineInterface@EngineControl@@UAEPAVEngineInterface@@XZ   EngineInterface* из this
  GetEngineControlInstance                                 ФАБРИКА, не аксессор - не звать
  ?GetTreeCalculateCount@EngineInterface@@UAEHXZ           номер кадра графа (кольцевой)
  ?GetTreeCalculateCount@A3d_ChannelGroup@@QAEHXZ          то же + собственный счётчик группы
  ?GetGraphRunning@EngineInterface@@UAE_NXZ                флаг ext->byte[+5]

Стартовая группа
  ?GetStartGroup@EngineInterfaceExt@@UAEHXZ                индекс текущей стартовой группы
  ?GetChannelGroup@EngineInterface@@UAEPAVA3d_ChannelGroup@@H@Z
  ?GetChannelGroupFileName@A3d_ChannelGroup@@UAEPBDXZ
  ?GetPoolName@A3d_ChannelGroup@@UAEPBDXZ
  ?GetStartChannel@A3d_ChannelGroup@@UAEHXZ                mov eax,[ecx+0x38]; ret
  ?GetQ3DStartGroup@EngineInterfaceExt@@UAEPAVA3d_ChannelGroup@@XZ    ГРУЗИТ при промахе - не звать

Группы
  ?GetChannelGroupCount@EngineInterface@@UAEHXZ
  ?GetChannelGroup@EngineInterface@@UAEPAVA3d_ChannelGroup@@PBDH@Z
  ?GetPoolGroup@EngineInterface@@UAEPAVA3d_ChannelGroup@@PBDH@Z       резолв по имени ПУЛА
  ?Release@A3d_ChannelGroup@@UAEXXZ                        детур: инвалидация хендлов и шимов
  ?DeleteChannelGroup@EngineInterface@@UAEXH@Z             то же, узкий путь (Remove Group)

Поля, подтверждённые дизассемблером
  A3d_Channel      +0x10  channelCalculatedAtCount_   признак "посчитан в этом кадре"
                   +0x60  ingoreTreeCountState_ (CHIC) 1 => поле +0x10 не пишется никогда
  A3d_ChannelGroup +0x38  startChannel     +0x54  собственный tree count
  EngineControl    +0x04  EngineInterface*  +0x08  текущая стартовая группа
                   +0x3f4 running           +0x3fc inLoop
```

---

## 9. Что этим уточняется в `reversing-journal-engine.md`

- **§4.1.** Объект главного цикла — `EngineControl`, не `EngineInterface`; `+0x0c` — это
  `EngineLoop`, `+0x24` — `GetIfKeepRunningApplication`.
- **§4.2.** «Ровно один вызов на группу за кадр… по числу живых групп» — неточно.
  `CallStartChannel` зовётся один раз за кадр, **для стартовой группы**; остальные достигаются по
  ссылкам. Детур туда стоит один переход за кадр. Плюс `CallStartChannel` умеет звать игровая Lua.
- **§7, факт 4.** «Отложенный захват `EngineInterface*`» — свойство выбранного способа, а не движка.
  Через `EngineControl::EngineLoop` указатель доступен с первого кадра, и детур на
  `A3d_Channel::CallChannel` (трамплин на 22 655 каналах) не нужен вовсе.
- **§10, «кто и как часто инкрементит глобальный tree count»** — закрыт: `EngineLoop`, ровно один
  раз за кадр, безусловно.
- **§10, «матричный ABI»** — закрыт, см. §7.1.
- **§10, «сбив мемоизации записью в `+0x10`»** — по-прежнему не проверен, но само поле теперь имеет
  второе, безопасное применение: **чтение** его как признака живости канала.

---

## 10. Открытые вопросы

- **Порядок обхода групп внутри кадра** по-прежнему не выписан. Теперь понятно, что вопрос звучит
  иначе, чем казалось: это не «в каком порядке движок зовёт группы», а «в каком порядке стартовый
  канал `XX_StartHere` доходит до чужих групп по ссылкам». Ответ даёт детур на `CallChannel`
  конкретных `Do_*`, а не наблюдение за группами.
- **`load group sequence::Progress (0-1) val` на живой игре не читался.** Индекс канала внутри группы
  не фиксирован, имя — `Progress (0-1) val`, группа называется `load group sequence`. Ответ зависит
  от следующего пункта: если загрузчик выгружается, читать нечего и вопрос закрывается вместе с ним.
- **Выгружается ли `start - project loader.cgr` после передачи управления.** Если да — это ещё один
  бесплатный и очень чистый признак «загрузка кончилась».

  **Ф2 плана поставила измерение вместо предположения.** `engine_state` при переходе в `ready` пишет
  в `TweakerStuff\Logs\TweakerPlugin.log` строку `engine: at ready the loader group is still
  loaded | gone` — один поиск по уже собранному ростеру групп, ничего не грузит и никого не будит.
  Ответ придёт с любого запуска игры, а не только с нашего.
- **`SeizureWarning_SecondsViewed` и `Quickstart?`** — второй путь передачи управления
  (`Do_Quickstart`) срабатывает по параметру командной строки; на нём ли запускается игра из Steam,
  не проверялось. На признак из §2.4 это не влияет: оба пути идут через тот же
  `SetNewStartChannel #256`.
- **`Preloader.cgr` живой или мёртвый.** `Q3DStart.q3d` называет `start - project loader.cgr`;
  остаётся ли `Preloader.cgr` рабочим путём для какой-то конфигурации (`testapp.exe`,
  `Audiosurf.exe` как лаунчер) — не выяснялось.
- **`EngineControl +0x3f4` («running»)** опознан по употреблению в `EngineLoop`, не по независимому
  подтверждению.
- **Почти всё в этом журнале получено статическим реверсом.** Проверено на живой игре (23.09.2026,
  Ф1 плана) только §1: `EngineControl` действительно тот класс, что драйвит главный цикл — его vptr
  сошёлся с экспортированным `??_7EngineControl@@6B@`; `EngineLoop` зовётся и зовётся раз за кадр;
  `GetEngineInterface()` отдаёт рабочий указатель на первом же кадре, без ввода. Объект один на
  процесс: поздний инжект увидел те же адреса, что ранняя загрузка. Всё остальное — §2 (порядок
  загрузки), §3 (значения из редактора), §6 (жизненный цикл групп), §7 (семейства) — по-прежнему
  только статика.
- **Частота кадров движка непостоянна, и верхняя граница неожиданно высока.** В одном из сеансов Ф1
  насчитано 130 кадров за 208 мс — **625 Гц**, при том что в меню кадр движка и кадр отрисовки идут
  один к одному. Сходится с §1.2: главный цикл зовёт `EngineLoop` всякий раз, когда очередь
  сообщений пуста, а тормозит его только `Present` внутри графа — значит там, где граф считается, но
  не презентуется (генерация трассы, загрузочный экран), он гонит свободно. **Не измерено прямо**:
  что именно игра делала в тот момент, из лога не видно.

  **Ф2 плана меряет это сама, и Lua-пробой это уже не измеришь.** Скрипты не запускаются, пока слой
  не скажет `ready`, то есть ровно в интересующем окне их нет. Поэтому длительность фаз `booting` и
  `starting` считает `engine_state` — в кадрах и в миллисекундах, отдельно по фазам — и пишет
  строкой `engine: graph rate while loading - booting N frame(s) in M ms (H Hz), starting ...`.
  При позднем инжекте оба промежутка близки к нулю, и это тоже ответ: грузиться было нечему.

---

## 11. Как это воспроизвести

Всё ниже — `Tools/CgrPy/` (см. его `README.md`); игра находится сама или через `AUDIOSURF_DIR`.

```bash
cd Tools/CgrPy
./uvrun.bat build_proj                       # один раз, кэш графа

# кадр и стартовая группа
./uvrun.bat vtdump HighPoly.dll              # EngineControl @ 0x13660, EngineInterfaceExt @ 0x13260
./uvrun.bat disasm HighPoly.dll 0x10b10      # EngineControl::EngineLoop
./uvrun.bat disasm HighPoly.dll 0x9620       # GetStartChannel: mov eax,[ecx+0x38]

# передача управления
./uvrun.bat chtypes SetNewStartChannel       # -> 3FF51E2F-....dll
./uvrun.bat disasm "3FF51E2F-6D04-4297-BC69-079C555FF765.dll" 0x10f0

# выгрузка группы
./uvrun.bat chtypes "Remove Group"           # -> BAC7326D-....dll
./uvrun.bat disasm "BAC7326D-6DDC-4ECF-B821-6A52C8287DC7.dll" 0x10e0

# слоты семейств
./uvrun.bat vtdump --type "Matrix"
./uvrun.bat vtdump --type "Texture"
./uvrun.bat vtdump --type "3D Object"
```

Стартовая группа из `Q3DStart.q3d` — zlib-поток со смещения 56, имя лежит открытым текстом:

```python
import zlib, pathlib
data = pathlib.Path("engine/Q3DStart.q3d").read_bytes()
print(zlib.decompress(data[56:])[:200])
```

Перепись семейств (§7) — скан импортов всех channel-DLL на `??0Aco_*Channel@@QAE@XZ` через
`cgr.pe.load(...).imports()`; двадцать строк, файл выбрасывается после ответа (нормальный режим
работы с этими инструментами, см. `Tools/CgrPy/examples/`).

Граф загрузчика (§2.3) — `cgr.decomp.Decompiler` по группе `start - project loader.cgr`, каналы
`#0`, `#39`, `#310`, `#334`, `#335`, `#256`; передача управления видна как внешняя ссылка
`#257 -> ('Do_StartFromExternal', 'StartGroup')` в `cgr.project.ext_target`.

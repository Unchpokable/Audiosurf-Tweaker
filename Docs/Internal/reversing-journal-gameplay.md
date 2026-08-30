# Полевой журнал реверса Audiosurf — игровая логика и сбор статистики

Сессия 2026-08-29. Продолжение `reversing-journal-lua.md`: тот разбирал контейнер `.cgr` и
Lua-движок, этот — **граф каналов как язык игровой логики** и конкретно `StatCollector.cgr`,
`SpecialPurpose.cgr`, `Puzzle.cgr`, `Player.cgr`, семейство `PlayerCar_*.cgr`.

Правило журнала прежнее: **проверенное отделяется от предположенного**. Всё, что ниже не помечено
как предположение, подтверждено байтовым разбором, декомпиляцией графа, экспортами PE или сверкой
с исходником Lua.

---

## Итог одним абзацем

Вся игровая логика Audiosurf живёт в **графе каналов Quest3D**, и он полностью читаем: формат
`.cgr` разобран (161 группа, 131 356 каналов; **у 159 из 161 групп число разобранных записей
канала сходится с `CHCO` точно**, у 144 из 161 — вдобавок ноль неразобранных байт), а
семантика узлов восстановлена настолько, что граф декомпилируется в читаемый псевдокод. Сбор
статистики заезда сосредоточен в `Scores/StatCollector.cgr` (2088 каналов): он держит вектор из
**20 сбрасываемых per-run счётчиков**, публикует **79 имён** наружу и принимает **4 входных
параметра** (`ReportMatch_TotalMatchedBlocks`, `ReportCarCollected_Color`, `UpdatePoints_Delta`,
`UpdatePoints_Color`), через которые в него стучатся все остальные группы. Игровые
режимы/персонажи — это **не** `PlayerCar_*.cgr` (те чисто визуальные), а ветки
`CallSelected` в `Actors/SpecialPurpose.cgr`, диспетчеризуемые по `StartGroup::PlayerVehicleType`
(18 персонажей, 0..17). Для чтения всего этого в реальном времени ничего изобретать не нужно:
`EngineInterface::GetChannelGroup(name)` → `A3d_ChannelGroup::GetChannel(name)` →
`Aco_FloatChannel::GetFloat()`, все три экспортируются **по имени** из `HighPoly.dll` и DLL канала.
Одна засада, на которой трекер обжигается сразу: **`Stats.CollectedColorCounts` нельзя показывать
как есть** — он считает не собранный трафик, а все блоки, попавшие в сетку, и способности
Eraser/Pointman загоняют процент за 100 %. Разбор и точный дискриминатор — §3.5.

---

## 1. Уточнения к спецификации `.cgr`

`reversing-journal-lua.md` описал контейнер (zlib → XOR 0x04 → поток чанков). Здесь — то, что
нужно, чтобы разобрать **граф**, а не просто найти в нём строки. Три уточнения к прошлой записи,
каждое ловится на молчаливой потере данных.

### 1.1 Не все теги имеют поле длины

Поток чанков — не гомогенный `TAG + u32 len + payload`. Есть **голые маркеры** длиной ровно
4 байта, без длины и без payload:

| Маркер | Значение |
|---|---|
| `A3DG` | начало графа каналов (сразу после `QVRS`) |
| `CHES` | «у канала есть внешняя связь» (см. §1.4) |
| `STNW` | флаг внутри канала `Text` |

Парсер, который на `A3DG` прочитает следующие 4 байта (`CGGG`) как длину, получит мусорную длину,
сорвётся в ресинк и с высокой вероятностью «нащупает» ложный тег внутри GUID'а. Именно так
рождается симптом из прошлой записи — пачка чанков с подозрительно одинаковым размером payload.

**`CHES` перегружен**: он встречается и как голый маркер, и как нормальный чанк с длиной (696
байт). Развязывается однозначно одним токеном lookahead: если следующие 4 байта — валидный тег,
это голый маркер, иначе это длина.

### 1.2 Правильный способ разбора — lookahead, а не ресинк

Рабочая схема, дающая 100 % покрытия на всех 161 файле:

1. тег — 4 байта из `[A-Z0-9_]` (структурная проверка, **не** белый список: игра постоянно
   подкидывает новые типоспецифичные теги — `ATTG`, `ACCS`, `ENV1`, `CEXI`, `COVA`, `TCBS`, …);
2. кандидат-длина валиден, если `offset + 8 + len` — конец блоба **или** снова валидный тег;
3. если тег из списка голых маркеров и следующие 4 байта — тег, выбираем голую трактовку;
4. иначе берём фрейм с длиной.

Ключевое: **никогда не терять синхронизацию**. Пока парсер идёт от заведомо корректной границы
чанка и перепрыгивает payload целиком, структурная проверка тега безопасна. Как только произошёл
ресинк «offset += 1» — начинаются ложные срабатывания на байтах GUID'ов. Поэтому нераспознанную
позицию правильнее считать ошибкой и показать её, а не молча искать следующий тег.

Контроль качества, которым это проверяется — два независимых счётчика:

1. **`CHCO` против числа разобранных записей канала.** Сходится точно у **159 из 161** группы.
   Считать надо именно записи, а не уникальные индексы: `CHCO` включает хвостовые заглушки
   редактора с `CHIX = -1` (в `Puzzle.cgr` их 7 из 1509, в `PlayerCar_EraserElite.cgr` — 631 из
   901). Не сходятся только `Intros/XX_OnlineHighScoresTable.cgr` (7167 из 7500) и
   `Render/Render_IndustrialTunnel.cgr` (8021 из 8037).
2. **Счётчик неразобранных байт.** Ровно 0 у **144 из 161** группы. Остальные 17 — сплошь
   ассет-тяжёлые (`Environment/SetPieces*`, `Squid_*`, `XX_StartHere.cgr`): в них лежат вшитые
   геометрия и текстуры, и на границе такого блоба фрейминг иногда срывается. Записи каналов при
   этом почти всегда всё равно сходятся — потери приходятся на хвостовые не-канальные чанки
   (`TENT`/`TECM`/`RHDR`/…).

**Все группы, разобранные в этом документе** (`StatCollector`, `Puzzle`, `SpecialPurpose`,
`Player`, `PlayerCar*`, `Highway`, `Achievements`, `TrafficCommander`, `AwardFare`) дают **0
неразобранных байт и точное схождение `CHCO`**. Единственное исключение — `XX_StartHere.cgr`
(20 968 неразобранных байт в хвостовых блобах), но и там 7167 каналов + 8 заглушек = 7175 = `CHCO`,
так что сам граф прочитан целиком; из него здесь и берутся только имена.

### 1.3 Запись канала

```
CHIX u32     индекс канала внутри группы
CHID guid16  GUID ТИПА канала -> channels.lst (имя типа + DLL)
[CHES]       голый маркер: у канала есть внешняя связь
CHIC u8      "ignore tree count": 1 = канал НЕ мемоизируется, пересчитывается на каждое
             обращение. Грузится в A3d_Channel+0x60, читается CheckRenderCount.
             См. reversing-journal-engine.md §4.4
CHNA str     имя канала (то самое, что Lua видит как chunkname)
CHIT u32
[CHES len]   696-байтная запись внешнего источника (см. §1.4)
CHLC u32     число слотов связей
(CHLI i32, CHUL u32, [CHRP u32])*
CHUP u8
<типоспецифичные чанки: FLVA, STVA, ATRS/ATRD, LUSL/LUSC, ...>
```

**Главное для понимания графа — что такое `CHUL` и `CHRP`:**

- `CHLI` — индекс канала-ребёнка, `-1` = пустой слот-заглушка редактора;
- **`CHUL` — номер входного ПОРТА** (какой «сокет» канала), а не флаг;
- **`CHRP` — номер слота внутри порта** (порядок детей в этом сокете).

Без этого граф выглядит плоским списком детей, и вся логика теряется. С этим — порты дают прямую
семантику типов каналов (§2).

### 1.4 Запись `CHES` — карта межгрупповых связей

696 байт, дамп сырой C++-структуры (в хвосте видно неинициализированную кучу — утёкшие указатели,
обрывки строк вроде `OnGetChild`; полезны только первые ~240 байт):

```
+0    char[80]   имя канала в группе-источнике
+80   char[80]   файл группы-источника: "Highway.cgr", "Puzzle.cgr", ...
+160  char[~64]  путь авторской машины:
                 C:\Documents and Settings\Dylan\My Documents\Audiosurf\Project\...
```

Разрешение — **по имени** внутри названной группы. Имя источника даётся либо файлом
(`Highway.cgr`), либо голым именем группы (`Achievements`, `StatCollector`), либо специальным
токеном **`StartGroup`** — это псевдоним корневой группы проекта, `XX_StartHere.cgr`.

На `StatCollector.cgr` из 143 импортов разрешаются 142; единственный промах — стаб `Do` в
`SpecialPurpose.cgr`, где в группе-источнике такого имени действительно нет (мёртвая ссылка
редактора).

### 1.5 Соглашение о вызове между группами

Канал с голым `CHES` **без** записи-источника — это **входной параметр группы**. Вызывающая
сторона привязывает фактические значения через дополнительные порты на стабе импорта:
**порт N привязывает параметр N**, где параметры упорядочены по индексу канала.

Проверено на `StatCollector.cgr`, у которого ровно 4 параметра:

| Ordinal | Параметр | Канал | Порт на стабе |
|---|---|---|---|
| 0 | `ReportMatch_TotalMatchedBlocks` | #92 | `Do_ReportMatch` → порт 0 |
| 1 | `ReportCarCollected_Color` | #96 | `Do_ReportCarCollected` → порт 1 |
| 2 | `UpdatePoints_Delta` | #154 | `Do_UpdatePoints` → порт 2 |
| 3 | `UpdatePoints_Color` | #155 | `Do_UpdatePoints` → порт 3 |

Сходится на всех десяти площадках вызова `Do_UpdatePoints` в `Puzzle.cgr` независимо.

**Независимое подтверждение из бинаря**: `HighPoly.dll` экспортирует
`A3d_ChannelGroup::GetParameterChannel(int)`, `GetParameterCount()`, `GetParameterInfo(int)` и
`AddParameterInfo(ParameterInterfaceInfo)`. То есть у группы действительно есть **упорядоченный
список параметров, адресуемый целым индексом** — ровно то, что даёт нумерация `CHUL`.

### 1.6 Таблицы (`Array Table`)

```
ATTG guid16  GUID таблицы
ATTN str     имя таблицы ("Stats", "Puzzle_State", ...)
ATCC u32     число колонок
  per column: ATCN str (имя), ATCW u32 (ширина в редакторе),
              ATCT guid16 (тип), ATUI guid16 (UID колонки), ATRC u32 (число строк)
ATRS/ATRD    строки (только у таблиц, заполненных на этапе авторинга)
```

Канал **`Array Value` / `Array Vector` — это курсор в ячейку**: его чанк `TIST` = GUID таблицы,
`TISC` = UID колонки, а ребёнок на порту 0 даёт индекс строки. Отсюда читается
`Stats.CollectedColorCounts[Index_CollectedColorCounts]` и подобное.

У живых игровых таблиц `ATRC = 0`: они пустые в файле и наполняются в рантайме.

---

## 2. Семантика типов каналов

Имя типа берётся из `channels.lst` по `CHID` (226 типов). Восстановленная семантика портов —
этого достаточно, чтобы декомпилировать граф:

| Тип | Порт 0 | Порт 1 | Порт 2 | Данные |
|---|---|---|---|---|
| `ChannelCaller` | список вызовов по порядку `CHRP` | — | — | — |
| `Set Value` / `Set Text` | источник (если нет — берётся `FLVA`) | список целевых каналов | — | `FLVA`/`STVA` |
| `If` | условие | тело | — | — |
| `IfElse` | условие | then | else | — |
| `ForLoop` | счётчик | выход индекса | тело | — |
| `Expression Value` | операнды A, B, C… по `CHRP` | — | — | **`FLVA` = текст формулы** |
| `ValueOperator` | операнды | — | — | `VEOT` (имя канала уже описательное) |
| `ChannelSwitch` | селектор | ветки по значению | — | — |
| `CallSelected` | селектор | ветка по умолчанию | ветки по значению | — |
| `Array Value/Vector` | индекс строки | — | — | `TIST`/`TISC` |
| `Array Command` | цель | — | — | `ACCS` (1 = очистить колонку, 7 = очистить таблицу) |
| `Value` / `Text` | источник значения | — | — | `FLVA`/`STVA` |
| `Lua Script` | дети = `channel.GetChild(i)` | — | — | `LUSL`/`LUSC` |

Две ловушки при чтении:

1. **`FLVA` у `Expression Value` — это строка, а не число.** Формулы записаны как
   `MIN(B,MAX(0,A))`, `A>B`, `-1*ROUND(A*B)`, `(A==2&&(!B))?C*1.2:C`, а буквы `A`, `B`, `C`
   ссылаются на детей по порядку слотов.
2. **`FLVA`/`STVA` у именованного `Value`/`Text` — не константа**, а последнее значение,
   сохранённое редактором. Для именованных каналов это просто переменная; константы имеет смысл
   читать только у безымянных.

`Lua Script` подтверждён сверкой: у канала `Lua Script_FindMatches` (`Puzzle.cgr` #235) 18 детей,
и все 18 индексов `channel.GetChild(i)` в выгруженном исходнике совпали с восстановленной по графу
привязкой один в один (`GetChild(1)` = `Puzzle: Blocks` = `aBoard`, `GetChild(17)` =
`StoneColorID` = `stoneIDChannel`, и так далее). Это независимая проверка всей цепочки разбора.

---

## 3. `Scores/StatCollector.cgr`

2088 каналов, GUID группы `FFC814FD-9178-4700-84F0-13D763299203`.

> Рядом в `engine/` лежит **`unprotected - StatsCollector.cgr`** (602 733 байта) — та же группа
> (тот же GUID), но **в открытом виде**: без zlib и без XOR, `QVRS` прямо со смещения 0, 2071
> канал против 2088. Это забытая в дистрибутиве авторская копия и идеальный эталон для проверки
> парсера: всю раскладку формата можно верифицировать на ней, не расшифровывая ничего.

### 3.1 Вектор per-run статистики

Мастер-сброс — `Set Value` #11 внутри `Do_ResetScores` (#1852): один узел, обнуляющий **20**
каналов. Это и есть канонический список счётчиков заезда:

| Канал | # | Кто пишет |
|---|---|---|
| `TimeOnShoulder` | 9 | `Do_ShoulderTimer` (каждый кадр, `+PausableTickCount/25`) |
| `TimeInFirstPerson` | 10 | `Do_FirstPersonTimer` (каждый кадр) |
| `LargestMatch` | 12 | `Do_ReportMatch`: `MAX(ReportMatch_TotalMatchedBlocks, LargestMatch)` |
| `Timer` | 13 | `Do` (#0), каждый кадр |
| `Points` | 152 | `Do_UpdatePoints`: `MAX(0, Points + Delt)` |
| `NumCombos` | 180 | `Do_AddCombo` |
| `NumChainReactions` | 183 | `Do_AddChainReaction` |
| `Points_L` / `Points_R` | 651 / 652 | `Do_UpdatePoints` (левая/правая половина трассы) |
| `JollyRogersCreated` | 759 | `Do_ReportJollyRogerCreated` |
| `HitsWithMouseControl` | 1226 | `Do_ReportCarCollected`, если `Player::CurrentInputMethod < 1` |
| `HitsWithoutMouseControl` | 1225 | `Do_ReportCarCollected`, иначе |
| `NumOverfills` | 1249 | **никто** — мёртвый канал, реальный счётчик это `Overfills` #340 |
| `YellowsPassed` | 1251 | `Do_ReportCarPassed`, если `PassedCarColor == 3` |
| `RedsPassed` | 1252 | `Do_ReportCarPassed`, если `PassedCarColor == 4` |
| `NonRedNonYellows_Hit` | 1263 | `Do_ReportCarCollected`, если цвет не 3 и не 4 |
| `Scored_A_21_Red_Earned?` | 1213 | `Do_ReportMatch`, если `NumRedsMatched > 20` |
| `FreeRideCarsHit` | 1737 | `Do_ReportFreeRideCollision` |
| `HitLastBlock?` | 1755 | извне (`TrafficCommander`, `Achievements`) |
| `EarnedRawGold?` | 1764 | `Do_CalculateMedalEarned` |

Вне этого вектора, но тоже сбрасываются в `Do_ResetSimpleStats` (#345): `Overfills` (#340),
`TotalCollisions` (#354), `WasInFirstPersonTheWholeTime?`, `PerfectNinjaRun?` (ставится в 1
только если `SpecialPurpose::Ninja?`).

Забавная деталь: там же `if (AllGreyMode? && Ninja?) Points := 1000` — то есть в этом режиме заезд
стартует не с нуля.

### 3.2 Таблицы группы

| Таблица | Колонки |
|---|---|
| `Stats` | `TrafficColorCounts`, `TrafficPattern`, `CollectedColorCounts`, `ColorPointValues`, `TrafficChainMax`, `TrafficChainMin`, `QuestionBoxState`, `Old Color`, `Traffic_isChainMax`, `Old Lane`, `Traffic_bEfficient` |
| `MusicChain` | `ChainNodeColor`, `ChainNodeRing`, `ChainNodeLane`, `ChainNodeTempRings` |
| `statsTrafficBuilder` | `TrafficPattern`, `QuestionBoxState`, `isChainMax`, `ChainStart`, `ChainEnd`, `FullTraffic_ArrayID`, `bEfficient` |
| `PointsByColor` | `Points`, `NumActiveMoneyParticles`, `LastKnownPoints` |
| `ScoreBackers` | `score` |

### 3.3 Входные точки (публичный API)

79 экспортируемых имён. Событийные (`ChannelCaller`) — вот полный контур сбора статистики:

| Хендлер | # | Кто зовёт | Что делает |
|---|---|---|---|
| `Do` | 0 | `XX_StartHere` | покадрово: `Timer`, таймеры first-person и обочины |
| `Do_ResetStats` | 1 | `XX_StartHere`, `Highway` | полный сброс + генерация трассы |
| `Do_ResetSimpleStats` | 345 | `XX_StartHere` | сброс только счётчиков |
| `Do_UpdatePoints` | 153 | Puzzle ×10, QuestionBox ×2, Boss, BusyBee, MoneyFloater, SpecialPurpose, PlayerLevel | **центральная точка начисления очков** |
| `Do_ReportMatch` | 91 | `Puzzle` | размер матча → `LargestMatch` |
| `Do_ReportCarCollected` | 95 | `Puzzle` | собранный блок: цвет, ниндзя-стрик, ввод |
| `Do_ReportCarPassed` | 735 | `TrafficCommander` | пропущенный блок |
| `Do_ReportOverfill` | 342 | `Puzzle` | `Overfills += 1` |
| `Do_ReportJollyRogerCreated` | 760 | `Puzzle` | |
| `Do_AddCombo` / `Do_AddChainReaction` | 185 / 184 | `Puzzle` | |
| `Do_ReportFreeRideCollision` | 1738 | `TrafficCommander` | |
| `Do_CalculateFinalStats` | 103 | `AwardFare` | **финальный подсчёт, бонусы, медаль** |

Начисление очков целиком:

```
Do_UpdatePoints(UpdatePoints_Delta = Delt, UpdatePoints_Color = c):
    Points := MAX(0, Points + Delt)
    PointsByColor.Points[c] += Delt
    if IncomingPointsAreSharedMatch?:  Points_L += Delt/2;  Points_R += Delt/2
    elif IncomingPointsAreRight?:      Points_R += Delt
    else:                              Points_L += Delt
```

### 3.4 Финальный подсчёт (`Do_CalculateFinalStats`)

```
PercentTimeInFirstPerson = WasInFirstPersonTheWholeTime? ? 1
                         : MAX(0, MIN(0.99, TimeInFirstPerson/(Timer-FirstPersonGracePeriod)))
PercentTimeOnShoulder    = TimeOnShoulder / Timer

for c in 0..5:                       # доля собранного по каждому цвету
    TotalCollisions += CollectedColorCounts[c]
    CollectedColorCounts[c] /= TrafficColorCounts[c]

# бонусы (каждый добавляет строку в Feat String)
Puzzle::Fetch_NumberBlocksInPlay == 0        -> Clean Finish : += Points * GridBonusMultiplyer
Ninja? && PerfectNinjaRun? && !PreventMono   -> Stealth      : += Points * PerfectNinjaMultiplier
LargestMatch > 20                            -> Match21      : += Points * Match21BonusScaler
LargestMatch > 10  (и нет Match21)           -> Match11      : += Points * Match11BonusScaler
LargestMatch > 6   (и нет Match11/21)        -> Match7       : += Points * Match7BonusScaler
!Ninja? && CollectedColorCounts[3] >= 0.95   -> Butter Ninja : += Points * YellowNinjaBonusPoints
!Ninja? && CollectedColorCounts[4] >= 0.95   -> Seeing Red   : += Points * RedNinjaBonusScaler

Points              = ROUND(Points)
PointsWithGridBonus = Points + BonusPoints + BonusPoints_CleanFinishOnly
FinalScore_PPM      = FLOOR(ROUND(PointsWithGridBonus / (VisMusic::SongLength/60)))

HighestMedalEarned = 0
  >= StartGroup::BronzeRequirement -> 1
  >= StartGroup::SilverRequirement -> 2
  >= StartGroup::GoldRequirement   -> 3
EarnedRawGold? = (Points >= GoldRequirement)      # без бонусов
```

Обратить внимание: **Clean Finish кладётся в отдельный аккумулятор** `BonusPoints_CleanFinishOnly`,
а не в общий `BonusPoints` — отсюда и отдельный экспорт для `AwardFare`.

Множители — обычные `Value`-каналы в `StatCollector` с уникальными именами, значения лежат в чанке
`FLVA` и читаются в рантайме. Хардкодить их незачем:

| Канал | # | Значение |
|---|---|---|
| `Match7 Bonus Scaler` | 954 | 0.07 |
| `Match11 Bonus Scaler` | 963 | 0.11 |
| `Match21 Bonus Scaler` | 819 | 0.21 |
| `YellowNinjaBonusPoints` | 979 | 0.05 |
| `RedNinja Bonus Scaler` | 989 | 0.05 |
| `GridBonusMultiplyer` | 163 | 0.25 |

Два следствия, которые легко упустить:

- **Медаль присуждается по `PointsWithGridBonus`, а не по `Points`.** А вот кольца медалей в HUD
  заезда (§3.6) сравнивают именно `Points`, без бонусов. То есть внутриигровой индикатор
  систематически занижает — и любой внешний трекер, считающий бонусы, будет точнее штатного.
- **Тест «95 %» берёт `CollectedColorCounts`, то есть `Achievements::YellowsHit` / `RedsHit`**
  (см. ниже, `ChannelSwitch #1915`), а не количество снятого с трассы трафика из §3.5. Это разные
  величины, и предсказывать бонус нужно по той, которую игра действительно проверит.

Ниже по тексту — разблокировки (`Gold_Keyboard_Earned?`, `Gold_no_Overfill_Earned?`,
`OnlyAll_RedYellow_Earned?`, `Overfill4_no_Points_Earned?`, `Chain5_Whites_Earned?`), затем
`LocalScores::Do_RecordCurrentRide` и два вызова в `Achievements`.

---

### 3.5 Ловушка: `CollectedColorCounts` считает **не** собранный трафик

Найдено эмпирически на живом трекере каналов (наблюдался процент собранного трафика **> 100 %** у
Eraser и Pointman при активном использовании способностей), затем подтверждено по графу.

Знаменатель и числитель считаются **несимметрично**:

- **`Stats.TrafficColorCounts[c]`** пишется ровно в одном месте — `Do_CountColors` внутри
  `Do_CharacterAndLeagueTrafficMods` ← `Do_ResetStats`, то есть **один раз при генерации трассы**.
  Это статическая величина.
- **`Stats.CollectedColorCounts[c]`** инкрементится в `Do_ReportCarCollected`, у которого
  **единственный** вызывающий во всём проекте — `Puzzle::Do_HittingNonFullColumn` (#395). А тот
  вызывается **безусловно, для любого блока, попадающего в сетку**, откуда бы он ни взялся:

```
Do_HittingNonFullColumn (#395):
    ... запись Puzzle_State.TB_* ...
    call StatCollector::Do_ReportCarCollected(ReportCarCollected_Color = LaneCrashColor)
```

Никакой проверки источника в этом пути нет. Общая воронка — `Puzzle::Do_ResovleLaneCrash` (#364),
и в неё стучатся:

| Источник | Путь | Настоящий сбор? |
|---|---|---|
| столкновение с трафиком | `TrafficCommander::Do_CreateTransitionBlock` (#735) | **да** |
| **Eraser: undo стёртого (ПКМ)** | `EraserEffect` #163 → … → `Do_LaunchTransitionBlock` #178 | нет |
| **Pointman: выброс блока из «багажника» (ПКМ)** | `SpecialPurpose::Do_CutPaste` #177 → `Trigger` #183 → #210 | нет |
| `Do_GrabArrowedDuplicates` (мёртвый код, см. ниже) | `SpecialPurpose` #850 | — |
| Pusher: `Do_ThrowRandomTile` | `SpecialPurpose` #253 | нет |
| Harvester: выгрузка | `SpecialPurpose::Do_Harvester` → #425 | нет |
| Vegas: бесплатный паверап | `Do_VegasRandomQueue` → `Trigger` #183 → #210 | нет |
| гранаты | `GrenadeManager::Do_LaunchTransitionBlock` ×3 | нет |
| combat-режим, груз врага | `Enemies_Flight::Do_FireShot` | нет |

Отсюда наблюдаемое поведение: Eraser стёр 15 жёлтых и вернул их на ПКМ → `+15` к
`CollectedColorCounts[yellow]`, хотя трафика не прибавилось. Pointman: подбор блока — `+1`
(законно, это столкновение с трафиком), выброс обратно — ещё `+1` (уже нет), итого **×2**. При
активной игре способностями отношение `CollectedColorCounts[c] / TrafficColorCounts[c]` уверенно
уходит за единицу.

**Как отличать — точный дискриминатор.** `Puzzle::LaneCrash_CarID` (#26) выставляется **всегда до**
вызова `Do_ResovleLaneCrash`, и ровно один источник кладёт туда реальный индекс:

```
TrafficCommander #735:   LaneCrash_CarID := CurrentTraffic     (>= 0)
все остальные источники: LaneCrash_CarID := -1
```

Проверено по порядку слотов у **всех девяти мест записи** `LaneCrash_CarID` (восемь пишут `-1`,
одно — `CurrentTraffic`): паттерн везде одинаков — `[0] LaneCrashColor := …`,
`[1] LaneCrash_CarID := …`, и только потом вызов `Do_ResovleLaneCrash`. Что это и есть задуманная
семантика поля, косвенно подтверждает имя канала в `Enemies_Flight.cgr` —
`Do_SetCarID_SoCollectibleCheckCanBeMade`.

Единственное исключение — ветка `Do_GrabArrowedDuplicates` (`SpecialPurpose` #840 → … → #850): она
ставит `LaneCrashColor`, но `LaneCrash_CarID` не трогает вовсе, так что дискриминатор там остался
бы протухшим значением от предыдущего события. На дискриминатор это не влияет, потому что ветка
**мертва**: её `Trigger` #865 не имеет ни одного родителя, а заготовленный сеттер `-1` (#854) —
тоже осиротевший `Set Value`. Если ветку когда-нибудь оживят патчем, дискриминатор в ней сломается
— это единственное место, за которым стоит следить.

То есть в момент срабатывания `Do_ReportCarCollected` достаточно прочитать
`Puzzle::LaneCrash_CarID`: `>= 0` — настоящий сбор трафика, `-1` — синтетическая ре-инъекция блока.

#### Правильная воронка — `TrafficCommander::Do_CollectCar`

Всё вышесказанное объясняет, как чинить `CollectedColorCounts`. Но если вопрос ставить иначе — **не
«что попало в сетку», а «сколько трафика игрок снял с дороги»** — то `Do_ReportCarCollected` для
этого вообще не годится, и дискриминатор не нужен.

`TrafficCommander::Do_CollectCar` (#763) — единственная воронка «машинка игрока коснулась блока
трафика». Она один раз читает цвет и дальше ветвится натрое:

```
Do_CollectCar (#763):
    TrafficType := Traffic.Type[CurrentTraffic]        # цвет, всегда
    Traffic.State[CurrentTraffic] := 0                 # блок снят с дороги
    if SpecialPurpose::DumptyScoopDown?:               # Pointman: в буфер
        SpecialPurpose::IN: DumptyAddOccupant := ...
        call SpecialPurpose::Dumpty_AddOccupant
    elif SpecialPurpose::ShatterStorming?:             # Eraser: стереть
        call EraserEffect::Do_LaunchErasers(LaunchErasers_Color = TrafficType)
        call SpecialPurpose::Do_Report_JustErased
    else:                                              # обычный сбор: в сетку
        Puzzle::LaneCrashColor  := TrafficType
        Puzzle::LaneCrash_CarID := CurrentTraffic
        call Puzzle::Do_ResovleLaneCrash(...)
    call Do_DestroyCar
```

Отсюда сразу три вещи:

1. **Ветки Pointman и Eraser не доходят до `Do_ReportCarCollected` вовсе.** Блок, ушедший в буфер
   или стёртый, в сетку не попадает — значит, трекер на базе `Do_ReportCarCollected` показывает у
   Pointman'а, играющего через буфер, около нуля собранного, а стирание не видит совсем. При том что
   стирание — основа геймплея Eraser'а (набрать 95 % красных/жёлтых), и трафик оно расходует.
2. **Дискриминатор здесь не нужен.** `Do_CollectCar` управляется `CurrentTraffic`, то есть реальным
   индексом трафика; ре-инъекции способностями идут напрямую в `Do_ResovleLaneCrash` и сюда не
   заходят. Проблема §3.5 в этой точке просто не возникает.
3. **Цвет берётся из `TrafficCommander::TrafficType`** — и вот тут ловушка, см. ниже.

#### Ловушка: имена каналов внутри группы **не уникальны**

В `TrafficCommander.cgr` есть **два** канала `Value` с именем `TrafficType` — **#631 и #900**.
`Do_CollectCar` пишет **#900**, а `A3d_ChannelGroup::GetChannel(const char*)` — линейный скан,
возвращающий **первый** по порядку, то есть **#631**. Читать по имени здесь означает читать не тот
канал (и получать 0, то есть «всё фиолетовое»).

Это общее свойство формата, а не частный случай: имя канала — не ключ. Там, где имя неоднозначно,
адресоваться надо по индексу (`GetChannel(int)`).

#### Ловушка: канал-параметр группы нельзя читать напрямую

`StatCollector::ReportCarCollected_Color` (#96) — это **параметр группы** (ordinal 1, см. §1.5), то
есть канал с `interfaceType_ == ACO_PARAMETER`. Значение в него не пишется: `A3d_Channel::GetChild`
разрешает параметр через **вызывающую** сторону (`case 3` → `group->GetParameterChannel(ordinal)`),
и только когда обращение идёт по дочерней связи. Прочитать такой канал снаружи через `GetFloat` —
получить его собственное протухшее поле, а не аргумент вызова.

Практически: читать надо **источник**, а не параметр. Все без исключения места вызова передают
`ReportCarCollected_Color = Puzzle::LaneCrashColor`, так что настоящий цвет живёт в
`Puzzle::LaneCrashColor` (#431) — обычном `Value`, который читается штатно.

#### Подтверждение на живой игре

Разбор выше был чисто графовым. Перехват `Do_ReportCarCollected` из скриптового слоя
(`lua-scripting.md`, `assets/scripts/traffic.lua`) дал возможность посмотреть, когда хендлер реально
зовётся. Наблюдения полностью совпали с предсказанием:

| Режим | Действие | `Do_ReportCarCollected` |
|---|---|---|
| **Pointman Elite** | блок подобран сразу на доску (без ЛКМ) | **да** — настоящий сбор |
| | блок подобран **при зажатой ЛКМ** (способность активна) | **нет** — блок уходит в стек, а не в сетку, то есть до `Do_HittingNonFullColumn` не доходит |
| | блок сброшен из стека (ПКМ) | **да** — и это ре-инъекция, путь `Do_CutPaste` |
| **Eraser Elite** | блок подобран с дороги | **да** — настоящий сбор |
| | сброс стёртых блоков из «памяти» (undo) | **да, на каждый блок**: сбросили 20 — получили 20 вызовов |

Последняя строка — самая наглядная иллюстрация проблемы: одно нажатие ПКМ у Eraser'а накручивает
`CollectedColorCounts` на два десятка, и без дискриминатора процент собранного трафика улетает
куда угодно. Ветка `EraserEffect` → `Do_LaunchTransitionBlock` действительно прогоняет каждый блок
через общую воронку по отдельности, как и следовало из графа.

Побочно подтвердилось и то, что «подбор при активной способности» вообще не проходит через сетку —
в таблице выше это единственный случай, где события нет. В графе это ветка, уводящая блок в буфер
персонажа мимо `Do_ResovleLaneCrash`.

**Практический вывод для трекера:** не показывать `CollectedColorCounts / TrafficColorCounts`
как есть, и **не** пытаться чинить это дискриминатором на `Do_ReportCarCollected`. Таблица выше
показывает, почему такой ремонт неполон: у Pointman'а строка «подобран при зажатой ЛКМ» —
настоящий сбор трафика, которого хендлер вообще не видит, а в реальной игре через буфер уходит
до 90 % трассы. Дискриминатор убрал бы ложные срабатывания и оставил бы пропуски.

Правильный ответ — воронка `TrafficCommander::Do_CollectCar` выше по этому же разделу: она
покрывает все три ветки (сетка, буфер, стирание) и не требует дискриминатора вовсе, потому что
управляется реальным индексом трафика. Клампить отношение к 1.0 — маскировка, а не решение: она
прячет расхождение, но проценты по цветам всё равно останутся неверными относительно друг друга.

Отдельно стоит держать в голове, что **сама игра этой ошибкой не страдает по-настоящему**: в
`Do_CalculateFinalStats` доля используется только в порогах `>= 0.95` для бонусов Butter Ninja и
Seeing Red, и оба они выдаются лишь при `!Ninja?`, а завышение играет в пользу игрока. То есть это
дефект отображаемой статистики, а не эксплуатируемая дыра в подсчёте очков.

### 3.6 Медали заезда: пороги, лига и HUD

Три числа, которые игрок видит как медали, живут в `XX_StartHere.cgr` (пул — `StartGroup`) и
пересчитываются на старте заезда в `Do_CalcMedalRequirements` (#4267):

```
StartGroup::BronzeRequirement (#404) := StatCollector::TotalCarCount * ChannelSwitch#4762(LeagueID)
StartGroup::SilverRequirement (#405) := StatCollector::TotalCarCount * ChannelSwitch#4767(LeagueID)
StartGroup::GoldRequirement   (#406) := StatCollector::TotalCarCount * ChannelSwitch#4772(LeagueID)
```

То есть порог — это «сколько очков за блок в среднем» × число блоков на трассе, а множитель
выбирается по `StartGroup::LeagueID` (#4488, 0-based: casual / pro / elite). Имена всех четырёх
каналов внутри группы уникальны, читаются обычным `GetFloat`, и уже посчитаны игрой — своя
арифметика тут не нужна.

Заработанность медали — простое сравнение, без промежуточного состояния:

```
EarnedBronze? = StatCollector::Points >= StartGroup::BronzeRequirement
EarnedSilver? = StatCollector::Points >= StartGroup::SilverRequirement
EarnedGold?   = StatCollector::Points >= StartGroup::GoldRequirement
```

**Весь внутризаездный HUD медалей висит на одном канале** — `gui/XX_gui.cgr`,
`Do_LadderMedalRequirements` (#3464, имя уникально). Путь до него: `Do_GUI` (#384) → `If` (#5457,
`!SpecialPurpose::Freeride?`) → #3464 → `Do` (#3470) → `If` (#5242,
`!DoInitialRun? && !DoVisualizerDemo?`) → `Do` (#3469). Ниже #3469 — ровно шесть детей:

| Канал | Что делает |
|---|---|
| `Medals_Dashboard` (#4904) | подложка |
| `DoRenderCompletedMedalRings` (#5040) | три `medalWhiteRing` под `If` на `Earned*?` |
| `Z - Buffer Clear` (#12415) | |
| `MedalCoverShape` (#4939) | |
| `Do_TriggerMedalEarnedEvents` (#5279) | `Trigger` на каждый `Earned*?` → мигание; плюс `Do_TimeoutMedalFlashes` (#5298), гасящий `Blink*?` по таймеру |
| `MedalCelebrationAnimation` (#5244) | анимация `MedalCelebration`, ужимает `CelebrateSize` |

Важное для перехвата: **ни один из шести ничего не отдаёт наружу**. `Do_TriggerMedalEarnedEvents`
выглядит как «событие», но его триггеры пишут только `BlinkBronze?/BlinkSilver?/BlinkGold?`, которые
читает та же ветка отрисовки. Ни ачивки, ни `AwardFare`, ни `LocalScores` сюда не заглядывают —
они считают медали независимо, через `StatCollector::Do_CalculateMedalEarned` (#845) и
`HighestMedalEarned` (#834).

Стоит заметить, что этот HUD ещё и **врёт в меньшую сторону**: он сравнивает `StatCollector::Points`,
а медаль по итогам заезда присуждается по `PointsWithGridBonus` — очки плюс бонусы (§3.4). Пока
бонусы не начислены, кольца показывают заниженный прогресс, и трекер, считающий бонусы на лету, даёт
более точную картину, чем то, что он заменил.

Поэтому `Do_LadderMedalRequirements` — **чистая ветка отрисовки**, и подавление её `CallChannel`
убирает медали с экрана, не задевая ничего больше. Это и делает `assets/scripts/traffic.lua` через
`tw.mute` (см. `lua-scripting.md` §8.5). Замечу, что «чистота» тут — свойство именно этого узла, а
не общее правило: соседний `Do_MedalMarkers` (#1225/#1707/#1883, метки порогов на шкале очков)
подавлять так же безопасно, а вот `Do_CalcMedalRequirements` — уже нет, он пишет пороги.

### 3.7 Живая палитра цветов блоков

Цвета, в которые игрок видит блоки, берутся из `XX_StartHere.cgr` через `FetchColorByID` (#882,
`VectorOperator`), а конкретный цвет выбирает `ChannelSwitch` **#1535**: связь [0] — селектор
(`ColorID`), связи [1..] — случаи по порядку.

| ColorID | Канал | |
|---|---|---|
| 0 | #1542 | |
| 1 | #1538 | |
| 2 | #1534 | |
| 3 | #1530 | |
| 4 | #1526 | |
| 5 | #1526 | тот же, что и 4 |
| 6 | #2663 | `Wild` |
| 7 | #2667 | `Stone` |
| 8 | #2664 | `Bomb` |
| 9 | #2574 | `Black` |
| 10 | #878 | `white` |

Все они — `Value Vector`, читаются `GetVector` (слот 17, см. `reversing-journal-engine.md` §2.2.2).
Статических значений в `.cgr` у них нет: компоненты — отдельные `Value`-каналы с именем `default`,
которые наполняются в рантайме из настроек цвета, поэтому единственный способ узнать реальный цвет —
прочитать канал у живой игры. Именно это делает `tw.vector_ch` в скриптовом слое.

Пространство `ColorID` здесь то же самое, что у трафика (§3.8): совпадает даже дубль — случай 5
ведёт на тот же канал, что и случай 4, ровно как случай 5 ведёт в `RedTotal` у счётчика трафика.
`Stone` (случай 7) — это и есть белый блок Puzzle, а `white` (случай 10) — что-то другое.

### 3.8 Сколько на трассе трафика какого цвета — и почему не `Stats: TrafficColorCounts`

Колонка `Stats: TrafficColorCounts` (#3) **пишется один раз, в самом конце заезда**, внутри
`Do_CalculateFinalStats`, и только для цветов 0…5:

```
for c in 0..5:
    Stats.TrafficColorCounts[c]   := ChannelSwitch#1924(c)   # Achievements::{Purple,Blue,Green,Yellow,Red}Total
    Stats.CollectedColorCounts[c] := ChannelSwitch#1915(c)   # Achievements::{Purples,Blues,...,Reds}Hit
    Stats.CollectedColorCounts[c] /= Stats.TrafficColorCounts[c]
```

Белые в неё не попадают вовсе: их складывают в `Achievements::WhiteTotal`, которого этот цикл не
касается. Плюс во время заезда колонка ещё не заполнена. Для внешнего трекера это значит, что она
не годится ни как источник знаменателей по ходу заезда, ни как полный список цветов.

Источник истины — `Achievements::Do_GetTrafficCounts` (#1236), и он элементарен:

```
PurpleTotal … WhiteTotal := 0
for StatCollector::Index_TrafficPattern in 0..StatCollector::TotalCarCount:
    ChannelSwitch#1248(Get vector x (StatCollector::Stats: TrafficPattern)) += 1
```

То есть игра просто **пересчитывает сгенерированную трассу**: `Stats: TrafficPattern` (#57,
`Array Vector`, курсор `Index_TrafficPattern` #58) — по строке на блок, `ColorID` в компоненте `x`.
Раскладка `ChannelSwitch #1248`:

| ColorID | Куда |
|---|---|
| 0 | `PurpleTotal` |
| 1 | `BlueTotal` |
| 2 | `GreenTotal` |
| 3 | `YellowTotal` |
| 4, 5 | `RedTotal` |
| 6 | *(никуда)* |
| 7, 8 | `WhiteTotal` |
| 9, 10+ | *(никуда)* |

Отсюда всё, что нужно знать про белые: **`ColorID` 7 и 8 — белые**, а 6 не считает никто, ни игра,
ни кто-либо другой.

Практический вывод: трассу можно пересчитать самому, тем же обходом, и получить корректные
знаменатели **с первого кадра заезда** — таблица заполнена в момент генерации трассы, задолго до
того, как игра до неё доберётся. Цена — `TotalCarCount` чтений через курсор (пара тысяч), поэтому
обход стоит размазать по кадрам. Имя `Stats: TrafficPattern` в группе не уникально (их четыре), так
что адресоваться нужно по индексу.

---

### 3.9 Играет ли игрок прямо сейчас: `StartupState`

Верхний уровень состояния игры — `StartGroup::StartupState` (`XX_StartHere.cgr`), обычный числовой
канал. Константы лежат там же:

| Канал | # | Значение | Что это |
|---|---|---|---|
| `State_MainMenu` | 81 | 2 | главное меню |
| `State_CharacterSelect` | 4384 | 2 | выбор персонажа — **то же число**, что и меню |
| `State_SongSelector` | 5470, 5846 | 3 | выбор трека |
| `State_Gameplay` | 352 | 5 | заезд |
| `State_Resetter` | 3282 | 7 | сброс между состояниями |
| `State_TeamManager` | 4391 | 8 | |

Канонический тест «идёт заезд» пишет сама игра — `Stage/XX_WindowState.cgr`:

```
InGameplay? = (StartGroup::StartupState == StartGroup::State_Gameplay)
```

и на нём же висят `Do_ForcePause` (сворачивание окна ставит игру на паузу, но не во Freeride) и
`Do_PlayerJustResumedPlayFromPause`. Сравнивать два канала, а не зашивать `== 5`, дешевле на одно
чтение и переживает перенумерацию.

**Пауза** — отдельный флаг, `gui/XX_PauseScreen.cgr`: `GamePaused?` (#28), плюс `Do_Pause` (#169),
`Do_Unpause` (#170) и `Do_TogglePauseState` (#173).

Осторожно: имя `StartupState` в группе **не уникально** (#855, #2373, #4165). Разрешается это в
нашу пользу: запись `CHES` хранит имя канала и файл группы, то есть импорт из чужой группы
резолвится тем же линейным поиском по имени, что и `GetChannel(const char*)` — значит внешний
потребитель гарантированно получает тот же канал, что и сама игра. Начальное значение у #855 — 2
(меню), что с этим сходится.

**Событие «начался заезд»** — `StatCollector::Do_ResetStats` (#1, имя уникально). Он чистит
`Stats: TrafficPattern`, обнуляет `TotalCarCount`, `Points`, `LargestMatch`, `Timer` и остальное,
после чего трасса генерируется заново. Это настоящая точка старта прогона; отслеживать её по откату
таймера назад — догадка о том же самом.

Отсюда практическое правило для внешних инструментов: **состояние читать, переходы — хукать**.
Флаг, собранный из хуков на `Do_StartSong` / `Do_Pause` / `Do_Unpause`, верен только если наблюдал
все рёбра — инструмент, подключившийся посреди заезда, стартует с неверным значением, а один
непойманный путь в меню оставляет его неверным до следующего. Чтение состояния верно с первого
кадра. Хуки при этом незаменимы там, где нужен именно момент (`Do_ResetStats`).

---

## 4. Режимы и персонажи

### 4.1 Где они на самом деле

**Не в `PlayerCar_*.cgr`.** Проверено: все 15 групп `PlayerCar_*` содержат почти исключительно
`Surface` / `Material` / `3D ObjectData` / `Motion` / `Envelope` / `Value Vector`, объявляют
**0 параметров** и экспортируют ровно три вещи — `Do_Render`, `Do_RenderForRadialBlur`,
`Do_RenderReflection`. Это модели машинок и ничего больше. Единственное исключение —
`PlayerCar_Sword.cgr` импортирует `PerfectNinjaRun?` и `Points`, чтобы подсветить корпус.

`Actors/PlayerCar.cgr` — тоже не логика, а **диспетчер отрисовки**: три `ChannelSwitch` по
`PlayerCarType`, каждый на 16 веток, каждая ветка — вызов `Do_Render*` соответствующей
`PlayerCar_X`.

Логика персонажей — в **`Actors/SpecialPurpose.cgr`** (1574 канала, 107 экспортируемых имён).

### 4.2 Диспетчер

```
Do_SpecialPurpose (#1)
  -> Do_SetAllConinuousPowers_OFF (#28)   # сбрасывает ~31 флаг способностей в 0
  -> Do_ForceCharacterChanges (#1432)     # теги в названии трека: [as-monoonly] и т.п.
  -> If (#1103) -> Do_AllCharacters (#1079)
       CallSelected (#4) по StartGroup::PlayerVehicleType
```

Порядок важен: сначала **всё выключается**, потом ветка персонажа включает своё. Поэтому набор
способностей персонажа читается прямо из его ветки, без учёта наследования.

Демо-версия использует отдельный `CallSelected` (#1073) с урезанным набором.

### 4.3 Таблица персонажей

`PlayerVehicleType` → ветка в `SpecialPurpose` → `PlayerCarType` → группа модели:

| PVT | Ветка | Ключевые флаги | PCT | Модель |
|---|---|---|---|---|
| 0 | `Do_Pointman` | `Is_a_Pointman?`, `RunHinter?`, буфер тайлов (`DumptyScoopDown?`), обочина | 9 | Rocket2 |
| 1 | `Do_Cooperative` | `useClone?`, `NumLanes=4`, `CooperativeMatchMultiplier=1.75` | 13 | Boomer2 |
| 2 | `Do_Vegas` | `canShuffle?`, шафл-грид, случайные паверапы | 7 | DragWarp |
| 3 | `Do_Pusher` | `ThrowTilesDirection`, `RunButler?`, `BufferRowCount=8` | 4 | Ovol |
| 4 | `Do_EraserMedium` | `ShatterStorming?`, `HasEraser?` | 12 | Vegas |
| 5 | `Do_InitialExperienceGuy` | `ThinTraffic?` (обучение) | 5 | Sword |
| 6 | `Do_AresBattleMode` | `UseMinions?`, `UseMissiles?`, `UseBoss?` | 0 | Rocket2 |
| 7 | `Do_Ares2` | то же + обочина | 0 | Rocket2 |
| 8 | `Do_Freeride` | `Freeride?`, `StoneHitsAllowed?=0`, `MatchCollectionTicks=50` | 2 | Beatmobile |
| 9 | `Do_DVCasual` | `useClone?`, множитель 1.5, `ThinTraffic?`, 6 рядов | 11 | Boomer |
| 10 | `Do_PointmanElite` | пойнтмен без хинтера и без обочины | 8 | Sword2 |
| 11 | `Do_NinjaPro` | `Ninja?`, `LoseAtLevel0?`, `MatchCollectionTicks=17` | 14 | RocketPro |
| 12 | `EraserElite` | ластик + батлер, 8 рядов, `MatchCollectionTicks=25` | 15 | EraserElite |
| 13 | `Do_NinjaMono` | `Ninja?`, `MatchCollectionTicks=28`, стоун-ластик | 0 | Rocket2 |
| 14 | `Do_DoubleVisionElite` | `useClone?`, множитель **2.4**, без обочины | 13 | Boomer2 |
| 15 | `Do_PointmanCasual` | пойнтмен + `ThinTraffic?` + хинтер по SPACE | 5 | Sword |
| 16 | `PusherElite` | пушер + батлер | 10 | Ovol2 |
| 17 | `Do_EasyNinja` | ниндзя, облегчённый | 6 | Rocket |

`PlayerCarType` дополнительно перекрывается: `ChannelSwitch` #188 в `PlayerCar.cgr` подменяет
модель на Beatmobile во фрирайде, а `SpecialPurpose::isMechMode` — на свою.

### 4.4 Ортогональные модификаторы

Персонаж — не единственная ось. Поверх него действуют:

- **`StartGroup::LeagueID`** — лига (casual/pro/elite). Через `ChannelSwitch` управляет шансами
  спавна паверапов (`Chance_Spawn*`), `MatchCollectionTicks`, штрафом за overfill.
- **`SavedConstants::ironmode?`** — айронмод: запрещает обочину, `Do_IronmodeOverfillPunish`
  снимает `-99999` очков и триггерит проигрыш.
- **`StartGroup::FreerideType`**, **`SpecialPurpose::Freeride?`** — фрирайд (42 потребителя).
- **`StartGroup::IsCombatMode?`** = `LadderLevelBeingPlayedRightNow == 17 || PVT == 6 || PVT == 7`.
- **`SpecialPurpose::PortalMode?`** (24 потребителя), `GreyMode` / `AllGreyMode?`,
  `PreventMonoStealthAndCumulativePoints?`.
- **Теги в названии трека**: `Do_ForceCharacterChanges` ищет в `CurrentSongTitle` подстроки вида
  `[as-monoonly]` и принудительно меняет персонажа (Mono / Pointman для Portal, Pusher для Mech).

---

## 5. `Environment/Puzzle.cgr` — сетка и матчи

1503 канала, 7 входных параметров (`AddBlock_Col`, `AddBlock_Color`, `FetchBlockMotion_BlockID`,
`ResloveLaneCrash_CrashLocation`, `ResolveLaneCrash_LaneID`, `ResolveLaneCrash_Color`,
`Fetch_ColCount_Col`).

Состояние сетки — таблица **`Puzzle_State`**, 20 колонок: `ColCount`, `Blocks`, `TB_Alive?`,
`RingParent`, `TB_DestMotion`, `TB_LifeSpent`, `TB_Pos`, `TB_CrashPos`, `TB_DestID`, `TB_DestPos`,
`TB_Alive?`, `TB_Color`, `Temp`, `TexID`, `MatchID`, `MatchedColors`, `TempMatchStorage`,
`BlockScreenPos`, `TileAge`, `SlidThisFrame?`.

Шесть Lua-скриптов делают то, что в графе выражать неудобно (поиск связных областей, циклы по
сетке), и общаются с графом **только** через `channel.GetChild(i)`:

| Скрипт | # | Детей | Назначение |
|---|---|---|---|
| `FindMatches` | 235 | 18 | поиск матчей заливкой по 4 соседям, учёт wild/stone |
| `SingleRowLowerBlocks` | 291 | 8 | осыпание блоков на один ряд |
| `DetonateBomb` | 538 | 5 | подрыв по цвету |
| `FindJollyRoger` | 800 | 8 | пиратские фигуры (candy cane / sword / hook / skull) |
| `FindPirateShapes` | 1008 | 0 | (детей нет — не подключён) |
| `UNUSED_FullyLowerBlocks` | 290 | 4 | помечен автором как неиспользуемый |

Начисление очков из Puzzle всегда идёт через стаб `Do_UpdatePoints` с привязкой параметров, а
цвет часто дополнительно пишется напрямую:

```
Do_HitColor:
    StatCollector::UpdatePoints_Color := 11
    call StatCollector::Do_UpdatePoints(
        UpdatePoints_Delta = SpecialPurpose::NinjaBonusPointsOnCarHit,
        UpdatePoints_Color = 11)
    call SpecialPurpose::Do_IncrementNinjaBonusPoints
```

Полная россыпь точек начисления — в `Temp/Dumped/_work/analysis/puzzle_logic.txt`.

---

## 6. `Actors/Player.cgr`

1002 канала, 0 параметров, 59 экспортируемых имён. Это **позиция/движение/ввод игрока**, не
статистика. Практически ценные каналы:

| Канал | # | Потребителей | Что это |
|---|---|---|---|
| `CurrentPlayerRing` | 574 | 133 | текущее кольцо трассы — главный «курсор» заезда |
| `PlayerRingInterpolation` | 571 | 46 | дробная позиция между кольцами |
| `PlayerLateralOffset` | 50 | 30 | поперечное смещение (из него `OnShoulder?`) |
| `CarBodyMotion` | 5 | 60 | матрица кузова |
| `CurrentInputMethod` | 827 | 2 | мышь (<1) или клавиатура — делит `HitsWith*MouseControl` |
| `AutopilotOn?` | 977 | 2 | автопилот |

---

## 7. Как отслеживать всё это в реальном времени

Ничего реверсить дополнительно не нужно — движок отдаёт доступ по именам.

### 7.1 Цепочка доступа

```
EngineInterface::GetChannelGroup(const char* name, int)   -> A3d_ChannelGroup*
      ?GetChannelGroup@EngineInterface@@UAEPAVA3d_ChannelGroup@@PBDH@Z   (HighPoly.dll)
A3d_ChannelGroup::GetChannel(const char* name)            -> A3d_Channel*
      ?GetChannel@A3d_ChannelGroup@@UAEPAVA3d_Channel@@PBD@Z            (HighPoly.dll)
Aco_FloatChannel::GetFloat() / SetFloat(float)            -> float
      ?GetFloat@Aco_FloatChannel@@UAEMXZ                                (BE69CCC4-....dll)
```

Плюс уже используемые в плагине `?GetChannelName@A3d_Channel@@QAEPBDXZ` и
`?GetChild@A3d_Channel@@QAEPAV1@H@Z`. Все — **экспорт по имени**, то есть `DetourFindFunction`
без сканирования сигнатур, ровно как в `framework/texture_hook.cxx`.

Параметры группы адресуются `GetParameterChannel(int)` — тем же ordinal'ом, что и порт на стабе
(§1.5).

### 7.2 Событийный поток

> **ОТМЕНЕНО. Этот раздел неверен** — см. `reversing-journal-engine.md` §7.
>
> `?CallChannel@A3d_Channel@@UAEXXZ` — пустая заглушка базового класса (один байт `c3`), которую
> переопределяют 159 типов каналов из 226, что покрывает **82.7 % всех каналов проекта**. Все
> перечисленные ниже хендлеры `Do_*` — это каналы типа `ChannelCaller`, и **ни один из них через
> базовую заглушку не проходит**. Детур в `framework/channel_hook.cxx` делает ровно то, ради чего
> написан (ловит `EngineInterface*`), но событийным потоком служить не может.
>
> Рабочий механизм — подмена vtable у **конкретных** объектов, разрешённых по имени один раз:
> `reversing-journal-engine.md` §2.4 и `lua-scripting.md` §3. Таблица имён каналов ниже при этом
> остаётся верной — меняется только способ до них дотянуться.

`TweakerPlugin` **уже** хукает `A3d_Channel::CallChannel` (`framework/channel_hook.cxx`, сейчас
только чтобы поймать `EngineInterface*`). Этого хука достаточно для полного событийного потока:
фильтровать по `GetChannelName(self)` и реагировать на нужные имена.

| Имя канала | Событие |
|---|---|
| `Do_ResetStats` / `Do_ResetSimpleStats` | начало заезда — сбросить своё зеркало |
| `Do_UpdatePoints` | очки изменились |
| `Do_ReportMatch` | матч собран |
| `Do_ReportCarCollected` / `Do_ReportCarPassed` | блок собран / пропущен |
| `Do_ReportOverfill` | overfill |
| `Do_AddCombo` / `Do_AddChainReaction` | комбо / цепная реакция |
| `Do_CalculateFinalStats` | заезд закончен, финальные значения готовы |

Важная оговорка про порядок: хук в плагине вызывает оригинал **до** своего кода
(`true_call_channel(self); ...`), то есть на момент реакции значения уже обновлены — для
`Do_UpdatePoints` и `Do_CalculateFinalStats` это именно то, что нужно.

### 7.3 Минимальный набор каналов для панели заезда

Всё в `StatCollector`, если не указано иное:

*В процессе заезда:* `Points` (152), `Points_L`/`Points_R` (651/652), `Timer` (13),
`LargestMatch` (12), `NumCombos` (180), `NumChainReactions` (183), `Overfills` (340),
`TotalCarCount` (60), `OnShoulder?` (364), `PerfectNinjaRun?` (727),
`Player::CurrentPlayerRing`, `Highway::NumRings` (прогресс трассы),
`Puzzle::Fetch_NumberBlocksInPlay` (заполнение сетки).

*После финиша:* `PointsWithGridBonus` (151), `FinalScore_PPM` (164), `HighestMedalEarned` (834),
`Feat String` (792, `Text`), `BonusPoints` (795), `BonusPoints_CleanFinishOnly` (1717),
`PercentTimeOnShoulder` (135), `PercentTimeInFirstPerson` (136), `TotalCollisions` (354),
`GotCleanFinish?` / `GotStealth?` / `GotMatch21?` / `GotMatch11?` / `GotMatch7?` /
`GotYellowNinja?` / `GotRedNinja?`.

*Контекст режима:* `StartGroup::PlayerVehicleType`, `StartGroup::LeagueID`,
`StartGroup::FreerideType`, `StartGroup::IsCombatMode?`, `SavedConstants::ironmode?`,
`SpecialPurpose::Ninja?` / `Freeride?` / `PortalMode?` / `NumLanes`,
`StartGroup::Gold/Silver/BronzeRequirement`.

Массивы (`Stats.CollectedColorCounts[i]`, `Stats.TrafficColorCounts[i]`) читаются иначе — через
канал-курсор: выставить индекс в `Index_CollectedColorCounts` (#16) и прочитать `Array Value` #8.
**Это состояние гонки с игровым потоком** — курсор общий, игра его двигает сама. Безопасно только
из хука на `CallChannel`, то есть на движковом потоке, и только когда игра сама не в середине
цикла по этому же индексу. Для панели проще снимать эти доли один раз после
`Do_CalculateFinalStats`.

**И, что важнее гонки: показывать `CollectedColorCounts` напрямую нельзя** — он считает все блоки,
попавшие в сетку, включая возвращённые способностями Eraser/Pointman и прочие ре-инъекции, из-за
чего процент собранного трафика уходит за 100 %. Правильный путь — свои счётчики по цветам,
инкрементируемые из хука на `Do_ReportCarCollected` при `Puzzle::LaneCrash_CarID >= 0`. Разбор —
§3.5.

---

## 8. Инструментарий

`Temp/Dumped/_work/cgrpy/` (gitignored) — Python-пакет под `uv`, Python 3.13:

| Модуль | Что делает |
|---|---|
| `cgr/core.py` | контейнер: `load()` (zlib + XOR), `iter_chunks`, `load_channel_types` для `channels.lst` |
| `cgr/graph.py` | парсер графа с lookahead-валидацией; `Channel`, `Link`, `parse_graph` |
| `cgr/project.py` | все 161 группа разом, разрешение `CHES`-импортов, `group_params` |
| `cgr/decomp.py` | декомпилятор графа в псевдокод (значения + операторы) |
| `genall.py` | генерация всего набора артефактов |

Выгрузки — `Temp/Dumped/_work/analysis/`:

| Файл | Содержимое |
|---|---|
| `statcollector_logic.txt` | декомпиляция всех `Do*` StatCollector'а (3226 строк) |
| `puzzle_logic.txt` | то же для Puzzle (3565 строк) |
| `characters.txt` | все 18 веток персонажей |
| `stat_writers.txt` | для каждого счётчика — все места записи |
| `public_api.csv` | экспортируемые имена 8 ключевых групп + потребители |
| `group_params.csv` | входные параметры каждой группы с ordinal'ами |
| `crossgroup_edges.csv` | граф зависимостей групп |
| `tables.csv` | схемы всех `Array Table` проекта |
| `lua_bindings.csv` | `GetChild(i)` → канал для всех Lua-каналов |

Дамп экспортов PE (`pe.py`) заменил `strings`/Ghidra для задачи «какие функции доступны по имени»:
чистый Python, без сборки бинарей — та же причина, что и в прошлой сессии (Defender карантинит
свежие неподписанные managed-сборки).

Ghidra в этой сессии не понадобилась: граф каналов оказался самодостаточен, а всё, что требовалось
от нативной стороны, читается из таблицы экспортов.

---

## 9. Открытые вопросы

- **`AVFR`** у `Array Value` — предположительно кэш числа строк/последнего индекса; значения
  выглядят как рантайм-мусор (3110, 646, 735). Не проверено.
- **Вторая пара `ATCT`/`ATCI`** в записи колонки таблицы: на колонку приходится два `ATCT`, и у
  второго содержимое похоже на неинициализированную память. Не разбиралось.
- ~~**`CHIC` / `CHIT` / `CHTM`**~~ — частично закрыто в `reversing-journal-engine.md` §4.4:
  `A3d_Channel::LoadChannel` грузит **`CHIC` → `+0x60` (`ingoreTreeCountState_`)** и
  **`CHTM` → `+0x69` (`treeCountMode_`, по умолчанию 1)**. То есть `CHIC` — флаг «не мемоизировать
  канал», выставляемый автором проекта поканально; у `Array Value` он стоит в 400 случаях из 431.
  Не разобран по-прежнему только **`CHIT`**.
- **`Do` в `SpecialPurpose.cgr`** — единственный неразрешённый импорт StatCollector'а. Похоже на
  мёртвую ссылку редактора, но не доказано.
- **Порядок вызова групп за кадр** не восстанавливался: известно, что `XX_StartHere` дёргает `Do`
  у большинства групп, но точная последовательность (а значит, и то, в каком порядке за кадр
  обновляются `Points` и позиция игрока) не выписана.
- **`Aco_FloatChannel::SetFloat` для записи** — путь существует и экспортируется, но запись в
  игровые каналы снаружи не пробовалась и заведомо опаснее чтения.
- **`Achievements.cgr`** (1431 канал) и его `Do_FinalizeAndStringEncodeExtendedRideStats` —
  расширенная статистика заезда кодируется в строку; формат не разбирался. Для Quick Player это,
  вероятно, самое интересное из неразобранного.

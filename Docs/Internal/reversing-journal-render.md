# Полевой журнал реверса Audiosurf — графическое окружение трассы

Сессия 2026-08-31, вторая половина. Пятый файл журнала. Предыдущий
(`reversing-journal-geometry.md`) разобрал **статические меши** — как они лежат в `.cgr` и как их
менять. Этот разбирает **всё остальное, что видит игрок на трассе**: спрайтовые эффекты
(«фейерверки»-партиклы, неоновые кольца, ячейки решётки, tile flyup), саму дорогу и тоннель, и
порядок, в котором кадр собирается.

Правило журнала прежнее: **проверенное отделяется от предположенного**. Ключевые выводы здесь
подтверждены с трёх независимых сторон — декомпиляцией канальных DLL, именами каналов, которые
автор игры дал портам в графе, и уже работающим кодом плагина (Skybox Replacer), — и там, где эти
три сходятся, оговорок нет.

Два раздела в конце стоят особняком. **§7** — разобранный до конца прикладной кейс («отключить
окраску партиклов»), доведённый не до патча, а до вывода о том, где именно упирается скриптовый
слой. **§8** — вытекающий из него список того, чего не хватает Lua API. Работа над самим API здесь
не ведётся; §8 существует как вход для будущего захода на `lua-scripting.md`, чтобы не выяснять это
заново. Общая позиция документа: **правильная реакция на «не хватает рычага» — расширять скриптовый
API, а не патчить `engine/`** (см. конец §6).

---

## Итог одним абзацем

Всё «плоское» в игре рисует **один-единственный тип канала — `Tune_Forest`**, и это не «лес», а
**сортированный батчер билбордов**: он берёт число спрайтов, курсор-индекс и таблицы
позиций/размеров/цветов, сортирует спрайты по расстоянию до камеры, строит камеро-ориентированные
квады и **пишет их напрямую в D3D-вершинный буфер** (`Lock`/`memcpy`/`Unlock`) — одним draw-call'ом
на батч. Фейерверки (`Debris`), неоновые кольца, ячейки решётки, брызги, тяга ракет — это всё он, с
разными входами. UV берутся либо целиком (0..1), либо из **атласа 2×2** — порт так и называется
`QuadDivideTexture?`, и у `tiles.png` он включён, то есть четыре картинки тайлов лежат в одном
файле. Дорогу, тоннель и разметку строит второй кастомный тип — **`Tune_Wall`**, экструзия
поперечного профиля (`Array_CirclePoints`) вдоль хребта трассы из `Highway.cgr`. Текстуры грузит
Lua-скрипт `q.LoadTextureDQ(<InstallPath> + "textures\<имя>", <канал Texture>)`, и **тот же
`LoadTextureFromMemory`-хук, на котором уже стоит Skybox Replacer, ловит их все** — ключ
сопоставления — имя канала (`particles1`, `Tex_ring1A`, `PuzzleGridTexture`, …). Порядок кадра
выписан целиком: корень группы `Render_IndustrialTunnel` → `IfElse MinimalDetail?` → 49 шагов
`RenderNormal` или 21 шаг `RenderMinimalist`.

---

## 1. Как грузятся текстуры окружения

Идиома одна на всю игру, 83 копии одного и того же скрипта (`_unique/00_02D4E7A7C30B_x83.lua`):

```lua
function CallChannel()
    local filename = channel.GetChild(0):GetText()
    local texture1 = channel.GetChild(1)
    q.LoadTextureDQ(filename, texture1, "")
end
```

Ребёнок 0 — не константа, а `TextureOperator "Merge Text"` из двух операндов:

```
Merge Text
  ├─ Text (пусто)  --EXT--> StartGroup::Install Path      каталог игры, в рантайме
  └─ Text "textures\particles1.png"                        константа в .cgr
```

Сохранённое в `.cgr` значение самого `Merge Text` — `C:\Q3D3\textures\particles1.png` либо
`C:\Program Files (x86)\Act-3D\Quest3D 3.6.6\textures\ring1A.png` — это **снимок с машины автора**,
а не то, что используется (та же оговорка, что и про `FLVA` у `Value` в gameplay-журнале).
Реально путь собирается каждый раз из `Install Path`.

Полная таблица — что, откуда и в какой канал (`engine/textures/`, 20 файлов):

| Файл | Размер | Группа | Lua# | **имя канала `Texture`** |
|---|---|---|---:|---|
| `particles1.png` | 128² | `Actors/Debris.cgr` | 316 | `particles1` |
| `particles2.jpg` | 1024² | `Actors/Debris.cgr` | 321 | `particles2` |
| `particles3.jpg` | 128² | `Actors/Debris.cgr` | 326 | `particles3` |
| `tiles.png` | 1024×512 | `Render/RenderCommon.cgr` | 204 | `PuzzleGridTexture` |
| `tileflyup.png` | 512² | `Actors/MoneyFloaterCommander.cgr` | 260 | `Texture` ⚠ |
| `ring1A.png` | 1024² | `Render/Render_IndustrialTunnel.cgr` | 6195 | `Tex_ring1A` |
| `ring1B.png` | 1024² | `Render/Render_IndustrialTunnel.cgr` | 6204 | `Tex_ring1B` |
| `ring2A.jpg` | 1024² | `Render/Render_IndustrialTunnel.cgr` | 6210 | `Tex_ring2A` |
| `ring2B.jpg` | 1024² | `Render/Render_IndustrialTunnel.cgr` | 6214 | `Tex_ring2B` |
| `cliff1-1.png` | 128² | `Render/Render_IndustrialTunnel.cgr` | 7310 | `FullBox` |
| `cliff1-2.png` | 128² | `Render/Render_IndustrialTunnel.cgr` | 7314 | `NoSides` |
| `cliff2-1.png` | 256² | `Render/Render_IndustrialTunnel.cgr` | 7205 | `NeonLines_Horizontal` |
| `cliff2-2.png` | 256² | `Render/Render_IndustrialTunnel.cgr` | 7306 | `BlackLinesVertical` |
| `hit1.png` | 685×576 | `Effects/HitCrosses.cgr` | 184 | `hit1` |
| `hit2.jpg` | 1024² | `Effects/HitCrosses.cgr` | 190 | `hit2` |
| `traffic.png` | — | `Stage/XX_WindowState.cgr` | 588 | `TrafficBillboardTexture` |
| `Skysphere_White/Grey/Black.png` | 4096×2048 | `Render/CopyPasteBuffer.cgr` | 335/349/362 | `Tex_WhiteSkysphere` / `Tex_GreySkysphere` / `Tex_BlackSkysphere` |

⚠ `tileflyup.png` попадает в канал с **generic-именем `Texture`** — по имени его не отличить от
сотен других. Единственный эффект в этом списке, для которого name-keyed перехват не работает
(см. §8).

**Это уже подключено к плагину.** `framework/texture_hook.cxx` детурит
`Aco_DX8_Texture::LoadTextureFromMemory(char*, int)` и раздаёт подписчикам имя канала;
`skybox.cxx` матчится на подстроку `"skysphere"` — то есть именно на `Tex_*Skysphere` из
`CopyPasteBuffer.cgr`, три канала, которые нашлись этим же обходом. Skybox Replacer работает,
следовательно **`q.LoadTextureDQ` проходит через `LoadTextureFromMemory`** — это эмпирический
факт, а не предположение, и он распространяется на всю таблицу выше.

Строка `Skysphere_Grey_alternative.png` лежит в каталоге, но **ни один канал её не грузит** —
неиспользуемый ассет.

---

## 2. `Aco_Tune_Forest` — спрайтовый батчер (главный вывод сессии)

Тип `CCF7CF78-0931-41B9-9EA6-D58C4B7F5704`, `Tune_Forest.dll`, семейство `3D ObjectData`.
Переопределяет ровно **четыре** метода базового `Aco_DX8_ObjectDataChannel`, остальные 74 наследует
как есть, и **расширяет vtable до 80 слотов**:

| Слот | Смещение | Метод |
|---|---|---|
| 1 | `+0x04` | `CallChannel` |
| 62 | `+0xf8` | `GetIfObjectIsVisible` → всегда `true` (отсечения нет) |
| **78** | **`+0x138`** | **`UpdateBuffer`** (своё) |
| **79** | **`+0x13c`** | **`InitializeBaseClassBuffers`** (своё) |

Конец таблицы виден так же, как у базы: сразу за слотом 79 в `.rdata` строка `"Tune_Forest"`.
`Tune_Wall`, `Tune_Car` и `Tune_Thruster` устроены **идентично** — те же два добавленных слота, те
же имена.

### 2.1 Контракт портов

Восстановлен из `UpdateBuffer` и **независимо подтверждён именами**, которые автор дал каналам в
графе. Совпадение полное, включая семантику атласа:

| Порт | Как используется в коде | Имена в графе |
|---|---|---|
| 0 | `GetFloat` → **N**, число спрайтов | `NumSprites`, `RingRenderCount`, `maxThrustCount`, `MaxSprayParticles` |
| 1 | `SetFloat(i)` — **курсор**, ставится перед чтением таблиц | `CurrentSprite`, `Index_Debris`, `Index_DistantRings` |
| 2 | `GetVector` → позиция спрайта | `Debris: Pos`, `NeonRings: Pos_Distant` |
| 3 | `GetVector` → размер; берутся `x*0.5`, `y*0.5` как полуоси | `Size Vector`, `NeonRings: Size_Distant`, `ThrustSize` |
| 4 | `GetVector` → **позиция камеры**, читается один раз до цикла | `Get Translation (matrix)`, `Lookat(Unused)` |
| 5 | `GetFloat` → **номер кадра атласа 0..3** | `TextureID`, `Debris: Type`, `Rockets: Thrust_TextureID` |
| 6 | `GetFloat > 0` → пересобирать буфер в этом кадре | `UpdateThisFrame?`, `Update?`, `UpdateDistanctRings?` |
| 7 | `GetFloat < 0` → **атлас выключен**, UV = 0..1 | `QuadDivideTexture?`, `QuadTexture?` |
| 8 | `GetVector` → цвет (RGB, 0..255) | `Debris: Color`, `NeonRings: Color_Distant` |
| 9 | `GetFloat` → альфа (×255) | `Alpha` |
| 10 | `GetVector` → ось билборда; **если нет — берётся `камера − позиция`** | `QuadNormal`, `NeonRings: Lookat_Distant` |

### 2.2 `InitializeBaseClassBuffers` — сборка через сеттеры базы

Ровно тот hot-swap-путь, который `reversing-journal-geometry.md` §6 вывел из кода. Автор игры
пользуется им сам:

```c
N = child(0)->GetFloat();
Release();                       // слот 19
SetVertexCount(N * 4);           // слот 25 — 4 вершины на квад
for (i = 0; i < GetVertexCount(); i++) {
    SetVertexPosition({0,0,0}, i);        // слот 31
    SetVertexTUCoord(0, {0,0}, i);        // слот 35
    if (colors_enabled) SetVertexColor(0xffff21ff, i);   // слот 37
}
SetIndexCount(N * 6);            // слот 27 — 6 индексов на квад
for (q = 0; q < N*6; q += 6)     // паттерн двух треугольников:
    SetIndex(base+0, q+0); SetIndex(base+1, q+1); SetIndex(base+2, q+2);
    SetIndex(base+2, q+3); SetIndex(base+3, q+4); SetIndex(base+0, q+5);
CreateVertexBuffer();            // слот 24
```

Наличие/отсутствие цветов решает `child(8)`, и через `CreateVertexBuffer`'ов пересчёт FVF
(geometry-журнал §4.1) это даёт **два размера вершины**:

| Цвета | FVF | Stride |
|---|---|---|
| нет | `XYZ \| TEX1` | **0x14 = 20** байт |
| есть | `XYZ \| DIFFUSE \| TEX1` | **0x18 = 24** байта |

### 2.3 `UpdateBuffer` — что происходит каждый кадр

1. **Сортировка по глубине.** Первый проход: для каждого `i` ставится курсор, читается позиция,
   считается `|камера − позиция|`; пары (расстояние, индекс) складываются в контейнер и
   **сортируются**. Второй проход идёт в отсортированном порядке. Это back-to-front для
   корректного альфа-блендинга — цена, о которой стоит помнить: **сортировка каждый кадр на каждый
   батч, у которого взведён `UpdateThisFrame?`**.
2. **Построение базиса билборда.** Нормаль (порт 10 либо `камера − позиция`) нормализуется; затем
   выбирается менее вырожденная опорная ось (сравнением `|y|` против `|x|+|z|`), два `cross`
   дают `right` и `up`, они масштабируются на полуразмеры.
3. **Четыре угла:** `c + r + u`, `c − r + u`, `c − r − u`, `c + r − u`.
4. **UV.** Если `child(7)->GetFloat() < 0` — полный квадрат `(0,0)…(1,1)`. Иначе **атлас 2×2** по
   номеру кадра `f` из порта 5:

   | `f` | U | V | квадрант |
   |---|---|---|---|
   | 0 | 0 … 0.5 | 0 … 0.5 | верх-лево |
   | 1 | 0.5 … 1 | 0 … 0.5 | верх-право |
   | 2 | 0 … 0.5 | 0.5 … 1 | низ-лево |
   | 3 | 0.5 … 1 | 0.5 … 1 | низ-право |

5. **Цвет** пакуется в `D3DCOLOR` как `(alpha*255)<<24 | r<<16 | g<<8 | b` и кладётся во все
   четыре вершины квада. **Это и есть «игра красит сама»** — тайлы и партиклы тонируются вершинным
   цветом поверх серой базовой текстуры.
6. **Запись.** `vertexBuffer->Lock(0, 0, &p, D3DLOCK_DISCARD); memcpy(p, staging, N*4*stride);
   Unlock();`

**Критично для модинга:** шаг 6 идёт **мимо CPU-массивов базового класса**. Правки в
`VPPI`/`VNNI`/`PODA` (хоть холодные, хоть горячие) для `Tune_Forest` **бесполезны** — их затрёт
прямая запись в VB на ближайшем кадре с `UpdateThisFrame? > 0`. Единственные рычаги — входные
каналы (§6, разобранный пример — §7).

`CallChannel` связывает всё вместе:

```c
if (наличие child(8) изменилось)              инвалидировать;
if (child(0)->GetFloat() != закэшированное N) инвалидировать;   // N изменилось -> перестроить
if (не инициализирован)  InitializeBaseClassBuffers();          // слот 79
else {
    if (child(6) && child(6)->GetFloat() > 0) UpdateBuffer();   // слот 78
    StreamVertexData();                                          // слот 22 — DRAW
}
```

---

## 3. `Aco_Tune_Wall` — дорога, тоннель и разметка

Тот же каркас (`UpdateBuffer` слот 78, `InitializeBaseClassBuffers` слот 79), но это **экструзия**:
поперечный профиль протягивается вдоль хребта трассы. Порты — с реального экземпляра
`Render_IndustrialTunnel #26`:

| Порт | Канал | Смысл |
|---|---|---|
| 0 | `Highway.cgr::NumRings` | число сегментов хребта |
| 1 | `Highway.cgr::RingIndex` | курсор сегмента |
| 2 | `Add Two Vectors` | позиция сегмента |
| 3 | `Highway.cgr::Rings: Rot` | поворот сегмента |
| 5 | `RowCountRoad_HighDetail` | тесселяция вдоль |
| 7 | `NumCirclePoints` | число точек **профиля** |
| 8 | `Index_CirclePoints` | курсор профиля |
| 9 | `Array_CirclePoints` | **сам поперечный профиль** |
| 10 | `SegmentTexture?` | строка `"Texture Per Segment?"` в DLL |
| 12 | `roadStartoffset` | сдвиг начала |
| 16 | `UpdateRoadThisFrame?` | пересобирать в этом кадре |

Отсюда: **вся форма трассы — это `Highway.cgr` (хребет: позиция + поворот на кольцо) плюс
`Array_CirclePoints` (профиль сечения)**. 44 экземпляра `Tune_Wall` в `Render_IndustrialTunnel` —
это слои дороги, обочин, стен тоннеля и линий разметки; ещё 3 в `FixedChainspans`, по одному в
`RenderCommon`, `LaserShot`, `RouteMap`.

Диагностические строки — `"TuneWall indices attempted to go higher than index count on buffer
creation"` и `"Tune_Wall: failed to find base vertex buffer"`.

Распределение всех кастомных типов:

| Тип | Где | Что |
|---|---|---|
| `Tune_Forest` | `Render_IndustrialTunnel` 5, `RenderCommon` 4, `Debris` 3, `XX_gui` 2, `Rocket` 1, `SkySpray` 1 | все спрайты |
| `Tune_Wall` | `Render_IndustrialTunnel` 44, `FixedChainspans` 3, `RenderCommon` 1, `LaserShot` 1, `RouteMap` 1 | дорога/тоннель/линии |
| `Tune_Thruster` | `RenderCommon` 2, `TuneThruster` 2, `Boss_TrafficRunner` 1, `Enemies_Flight` 1, `Render_IndustrialTunnel` 1 | выхлоп |
| `Tune_Car` | `RenderCommon` 2 | корпус машины |

---

## 4. Порядок кадра

Движок зовёт `A3d_ChannelGroup::CallStartChannel` раз на группу за кадр (engine-журнал §4.2).
Корень группы `Render/Render_IndustrialTunnel.cgr` — `#1 ChannelCaller 'Do'`:

```
#1 Do
 ├─ [0] Set Value
 ├─ [1] IfElse (XX_gui::MinimalDetails?)
 │        ├─ true  -> #7357 RenderMinimalist   (21 шаг)
 │        └─ false -> #1320 RenderNormal       (49 шагов)
 ├─ [2] Do_TickFramesAndReReset
 ├─ [3] If
 ├─ [4] Do_ScreenEffect
 └─ [5] Set Value
```

### 4.1 `RenderNormal` — полный проход

`#1320` = `Do_SetConstants` → ветка A `#905` (49 шагов) → ветка B `#6189` (пост-обработка).

```
 0 Do_RedrawExtrusions          <- пересборка Tune_Wall-буферов ДО отрисовки
 1 Set Value
 2 DX8 ClearScreen
 3 Do_GameplayCam
 4 Do_RadialBlurRender
 5 Do_RenderBloomPass
 6 Do_PsychadelicBackgroundWind
 7 Do_RenderSkySphere           <- Tex_*Skysphere; сюда врезан Skybox Replacer
 8 Do_GameplayCam
 9 Do_RenderWireGlobe
10 Do_RenderDebris              <- ФЕЙЕРВЕРКИ (Tune_Forest x3, particles1-3)
11 Do_GameplayCam
12 Do_RenderRoadSupports
13 Do_RenderColorRunners
14 Do_RenderBackgroundSetPieces
15 Do_RenderStations            (+ Z-Buffer Clear)
16 Do_renderBoss
17 Do_renderMinions
18 Do_RenderFinishLine
19 Do_RenderSetPieces
20 Do_LaneLines                 <- Tune_Wall: LeftShoulder / CenterLane / Lane1 / RightShoulder
21 Do_RenderTrafficPowerups
22 Do_GameplayCam
23 Do_ShoulderLines
24 (ChannelCaller #368)
25 Do_RenderChainspans
26 Do_GameplayCam
27 RenderTraffic
28 Do_RenderPlayerCarReflection
29 Do_GameplayCam
30 Do_RenderRoadclif            <- cliff1-1/1-2/2-1/2-2
31 Do_RenderRoad                <- ДОРОГА (Tune_Wall)
32 Do_RenderTrafficBoxBlurShadows
33 Do_RenderNinjasLittleHelper
34 Do_RenderPlayer
35 Do_Thrusters                 <- Tune_Thruster
36 Do_RenderCopyPasteBuffer_DumptyPointman
37 Do_RenderPuzzleBlocks        <- РЕШЁТКА / ТАЙЛЫ
38 Do_RenderTileNumbers
39 Do_RenderEraserEffect
40 Do_RenderHitCarHusk
41 Do_RenderPlayerShadow
42 Do_RenderCloudParticles
43 Do_RenderMorpheusArrows
44 Do_RenderScoreMutiplier
45 Do_RenderNeonRings           <- КОЛЬЦА (Tune_Forest, Tex_ring*)
46 Do_renderMissiles
47 Do_RenderLaserBlasts
48 Do_RenderBloom
```

Ветка B: `Do_FullSceneRadialBlur`, затем `Do_ScreenSpaceGraphics`
(→ `Do_RenderCollisionCrosses` — это `hit1`/`hit2`).

Три вещи стоит заметить. Во-первых, **`Do_GameplayCam` вызывается семь раз** — камера
переустанавливается между группами объектов, то есть часть слоёв рисуется с другой проекцией.
Во-вторых, **экструзии пересобираются шагом 0**, до любой отрисовки — то есть `UpdateRoadThisFrame?`
взводится раньше по кадру. В-третьих, **прозрачное идёт последним** (тайлы 37, кольца 45, блум 48)
— порядок в этом списке и есть порядок блендинга.

### 4.2 `RenderMinimalist` — 21 шаг

`Do_RedrawExtrusions`, `ClearScreen`, `Do_GameplayCam`, `Do_LaneLines`,
`Do_RenderTrafficPowerups`, `Do_RenderTraffic`, `Do_RenderRoad`, `Do_ShoulderLines`,
`Do_RenderPlayer`, `Do_Thrusters`, `Do_RenderPuzzleBlocks`, `Do_RenderEraserEffect`,
`Do_RenderNinjasLittleHelper`, `Do_RenderMorpheusArrows`,
`Do_RenderCopyPasteBuffer_DumptyPointman`, `Do_RenderNeonRings`, `Do_RenderScoreMutiplier`,
`Do_ScreenSpaceStuff`. Нет неба, дебриса, клиффов, блума, отражений.

---

## 5. Четыре эффекта поимённо

### 5.1 Фейерверки / партиклы — `Actors/Debris.cgr`

Три `Tune_Forest` (#94, #233, #234) с бюджетами **200 / 25 / 50** спрайтов, все читают одни и те же
таблицы `Debris: Pos`, `Debris: Type` (кадр атласа), `Debris: Color`, курсор `Index_Debris`;
различаются текстурой на своём `Surface` (`particles1` / `particles2` / `particles3`) и выбираются
`ChannelSwitch`'ами. Управляющие каналы, видимые снаружи:
`Debris::Do_Render`, `Debris::Do_Impact` (порождение при столкновении — зовут `TrafficCommander`,
`SpecialPurpose`, `QuestionBoxOverlord`), `Debris::DebrisParticleTexture` (какая из трёх),
`Debris::SquareParticles` (режим атласа), `Debris::DebrisYvelocity`, `Debris::Notes`.
Рисуется шагом 10 кадра.

### 5.2 Неоновые кольца — `Render/Render_IndustrialTunnel.cgr`

`Do_RenderNeonRings` (#776 в `RenderNormal`, #7361 в `RenderMinimalist`). Геометрия —
`Tune_Forest #662` c таблицами `NeonRings: Pos_Distant / Size_Distant / Color_Distant /
Lookat_Distant` и счётчиком `RingRenderCount`. Порт 10 (`Lookat_Distant`) задан явно — кольца
**не** разворачиваются на камеру, а лежат в плоскости трассы.

Текстура выбирается `ChannelSwitch #6224` по **`StartGroup::Detail` → `SavedConstants.cgr::RenderDetail`**:
`Tex_ring1A` / `Tex_ring1B` (и вторая пара `2A`/`2B`). Сопутствующие ручки: `RingIntensity`,
`RingSkip`, `ringVerticalSpacing`, `RingScaler`, `Do_LoadRingTextures` / `Do_ReloadRingTextures`,
`Do_StoreRingColorsForRoadSurfaceNeonRingShadows`.

Не путать с **кольцами хребта** (`Highway.cgr::NumRings` / `Rings: Rot`) — это сегменты экструзии
дороги, другая сущность с похожим словом.

### 5.3 Решётка / тайлы — `Render/RenderCommon.cgr`

**Два разных пути**, и это важно:

- **Батч-путь (кэшированная графика тайлов).** Четыре `Tune_Forest` (#3691, #3905, #4076, #4247),
  у каждого `NumSprites = 16`, `QuadDivideTexture? = 1` (**атлас включён**), `TextureID` считается
  `Expression Value`. Все четыре берут `PuzzleGridTexture` через `ChannelSwitch #5347`. То есть
  `tiles.png` (1024×512) — **атлас 2×2 из четырёх картинок 512×256**. Вызывается из
  `Do_RenderCachedTileGraphics` (#1322).
- **По-тайловый путь.** `Do_RenderPuzzleBlocks` (#2 → #155) — это `ForLoop`:

  ```
  for i in 0..Puzzle::NumBlocks {
      Puzzle::Blocks_Index, Puzzle::TexID_Index := i;
      Index_PuzzleBlockScreenPos, Puzzle::FetchBlockMotion_BlockID := i;
      Puzzle_State.BlockScreenPos[i] := ProjectToScreen(GetTranslation(Puzzle::Fetch_BlockMotion));
      ...
  }
  ```

  Внутри рисуется `3D Object PuzzleBlock #164` → `Surface #170` → геометрия **`Primitive`** (квад),
  текстура через `ChannelSwitch #200` по `Puzzle.cgr::Puzzle: TexID`. Варианты `puzzleBlockFilled`
  (#1257) и `puzzleBlockFrame` (#1953, меш `TileFrame.3ds`) выбираются по
  `Puzzle_State: TileAge` выражением `A<5?0:(A<13?1:(A<20?2:(A<26?3:4)))` — **пять стадий
  «возраста» тайла**. Цвет — `GridColor` (`Value Vector` #202/#2119/#2121).

  Вся модель сетки живёт в `Environment/Puzzle.cgr` (`Puzzle: Blocks`, `Puzzle: TexID`,
  `Puzzle_State: TileAge`, `Puzzle_State: ColCount`, `NumBlocks`) — см. gameplay-журнал.

> **Исправление (проверено записью в живой игре).** Фраза «Цвет — `GridColor`» выше **неверна**, и
> держалась она на близости канала в графе, а не на трассировке. `GridColor` #202 потребляют ровно
> четыре `Material`: два на `Surface "ro_01 1"` под `3D Object "static parts"` и два под
> `CarHitBlurBar` (через `Do_RenderColorCarHitBar`). Дорожная обвязка и вспышка удара — **не тайлы**.
>
> У `Surface #170`, которой рисуется сам тайл, детей ровно два: `Primitive` и `ChannelSwitch` с
> текстурой. **Material'а на ней нет вообще**, так что цвет приходит не отсюда, и запись в
> `GridColor` на решётку не влияет — что и подтвердилось: в Mono тайлы окрашены по окружению, в
> Puzzle — по цвету занявшего ячейку блока.
>
> Откуда берётся по-ячеечный цвет — **не выяснено**. Кандидаты: вершинный цвет `Primitive`, Material
> выше по цепочке `3D Object PuzzleBlock #164`, либо выбор текстуры по `Puzzle: TexID`. Это открытый
> вопрос, и до его закрытия строку «перекрасить тайлы» в §6 читать нельзя.

### 5.4 Tile flyup — `Actors/MoneyFloaterCommander.cgr`

Единственный из четырёх, который **не** `Tune_Forest`: `Surface #117` и `#234` (выбор через
`ChannelSwitch #237`) с геометрией `Primitive` и текстурой `tileflyup.png` в канале с именем
`Texture`. Обычный текстурированный квад на всплывающую плашку.

---

## 6. Что из этого следует для модификации

| Хочу | Как | Осуществимость |
|---|---|---|
| Заменить картинку эффекта | подписаться на `framework/texture_hook` и матчить **имя канала** из §1 | **готово** — тот же путь, что у Skybox Replacer |
| Заменить `tileflyup.png` по имени | — | **нельзя по имени**: канал зовётся `Texture`; нужен другой дискриминатор (порядок загрузки, размер буфера, указатель на канал) |
| Поменять относительный путь текстуры | холодный патч `Text`-константы `"textures\..."` в `.cgr` (сплайс чанка `STVA`, geometry-журнал §5.2) | реалистично; длина строки меняется свободно |
| Перекрасить дорожную обвязку и вспышку удара | `GridColor` — обычный `Value Vector` | **можно**, `vector_ch:set` (Ф5-A). Тайлов это НЕ касается — см. исправление в §5.3 |
| Перекрасить тайлы | источник по-ячеечного цвета не найден | **открытый вопрос**, см. §5.3 |
| Отключить окраску партиклов | хук `after` на `Set Vector` #48 + `array_vec:set` строки в белый | **готово и проверено в игре** — `assets/scripts/particles.lua`, см. §7. Работает и в Puzzle, и в Mono |
| Перекрасить партиклы **своим** атласом | то же плюс подмена `particles*.png` через `texture_hook` | **готово**: снятие модуляции — это и есть разрешение красить атлас, см. §7.4 |
| Сменить число/размер спрайтов | писать в порт 0 (`NumSprites`) и порт 3 (`Size Vector`) | работает: смена N сама инвалидирует буфер через `CallChannel` |
| Подменить **вершины** спрайтов | — | **бесполезно**: `UpdateBuffer` пишет прямо в VB (§2.3) |
| Подменить геометрию дороги | входы `Tune_Wall`: `Array_CirclePoints` (профиль) и `Highway.cgr` (хребет) | путь понятен, **в живой игре не проверялось** |
| Отключить/переставить слой кадра | подмена vtable на `ChannelCaller`-узле `Do_Render*` (`channel_shim`) | механика есть (engine-журнал §7), на этих узлах не пробовалась |

Общее правило, вытекающее из §2.3: **у процедурных типов (`Tune_*`, `Primitive`) менять надо
входные каналы, а не буферы.** Это ровно тот случай, который geometry-журнал §11 оставлял открытым
вопросом («процедурные типы под горячей заменой») — теперь на него есть ответ для `Tune_Forest`:
CPU-массивы затираются, точка приложения силы — порты.

**Про холодный патч отдельно.** Строки этой таблицы, где предлагается править `.cgr`, оставлены
как описание того, что технически возможно, а **не** как рекомендация. Патч файлов игры ломается о
любое обновление, невидим для игрока и не откатывается; для геометрии он оправдан (там менять
нечего, кроме данных в файле), а для спрайтов и окружения — почти никогда, потому что всё
управление здесь и так вынесено в каналы. Правильная реакция на «этого не хватает» — расширить
скриптовый API, а не трогать `engine/`. §7 — разобранный до конца пример того, как этот вывод
получается, §8 — что именно из этого следует добавить.

---

## 7. Разобранный кейс: отключить окраску партиклов

Задача-образец: заставить «фейерверки» рисоваться **строго тем, что лежит в текстурном атласе**, без
собственной тонировки игрой. Разбор доведён до конца, потому что он показывает и цепочку цвета
целиком, и границу текущего скриптового API.

### 7.1 Откуда берётся цвет

```
Tune_Forest #94 / #233 / #234   порт 8  ->  #47 Array Vector 'Debris: Color'
                                порт 9  ->  CHLI = -1   (альфы нет -> UpdateBuffer берёт 255)

Set Vector #48:  Debris: Color[Index_Debris] := ChannelSwitch #294
    селектор: FLOOR(CollisionColor) == 6
      случай 0: clamp01( FetchColorByID(CollisionColor) ± RAND * MaxColorVariation )  покомпонентно
      случай 1: (RAND, RAND, RAND)                                    -- радужный режим, цвет 6

Set Value #67:   CollisionColor := EXTERN::Impact_Color                -- игра пишет на каждый удар
```

Ключевые каналы: `Debris: Color` #47 (`Array Vector`), курсор `Index_Debris` #15,
`CollisionColor` #62 (`Value`), `MaxColorVariation` #300 (`Value`, 0.3).

### 7.2 Почему через Lua API это не делалось — и что закрыло каждый упор

**Кейс закрыт: работает в живой игре**, скриптом `assets/scripts/particles.lua` (путь (в) из §7.3).
Ниже — три упора в том виде, в каком они были, потому что именно они определили, какой формы вырос
скриптовый API; после каждого сказано, чем он снят.

Три независимых упора, и все в одно:

1. **Цвет живёт в векторном массиве, а API пишет только числа.** В `src/lua/lua_channels.cxx` весь
   write-интерфейс — это `set_float`, рядом с `get_text` / `get_float` / `get_vector`. Векторной
   записи нет **намеренно**: у векторного канала слот 19 — `SetFloat(int, float)`, а не
   `SetFloat(float)` (engine-журнал §2.2.2), и вызов не по той сигнатуре — рассогласование стека,
   а не «запись не туда».
2. **`tw.mute` не по адресу.** Он гасит `CallChannel` — путь действия. Порт 8 читается по пути
   данных (`GetVector`), мьют его не видит. Замутить писателя (`Set Vector #48`) можно, но тогда
   массив замирает на старых значениях, а несозданные строки остаются нулями — получаются **чёрные**
   частицы, а не «как в атласе».
3. **Косвенные float-рычаги есть, но задачу не решают.** `MaxColorVariation` убирает только разброс
   вокруг базового цвета. `CollisionColor` переключает палитру и радужный режим, но игра пишет его
   на каждый удар — это ровно та гонка, о которой предупреждает `Docs/scripting/limits.md`, и
   действует он лишь на вновь порождённые частицы.

**Чем каждый снят** (ступенями Ф5-A и Ф5-B/C, `lua-scripting.md`):

1. Появилась запись в векторный канал (`vector_ch:set`, слот 18), а затем и в строку `Array Vector`
   (`array_vec:set`). Опасение про слот 19 подтвердилось и осталось верным — просто пишем не через
   него. Дополнительно выяснилось, что у `Aco_Array_Vector` слот 18 **свой** и кладёт значение
   именно в строку, а не в скаляр канала (engine-журнал §2.2.3); без этого путь (в) был бы закрыт.
2. Упор про `tw.mute` остался в силе и **подтвердился делом**: гасить писателя нельзя, это даёт
   чёрные частицы, а не нетонированные. Решение не в мьюте, а в том, чтобы дать писателю отработать
   и переписать результат следом.
3. Косвенные рычаги не понадобились.

Цена оказалась ровно предсказанной: одна векторная запись на **порождённую** частицу. В живой игре
хича нет, счётчик отказов — ноль: курсор всегда указывает на существующую строку, потому что её
заполняет сам `Set Vector` #48 непосредственно перед нашим хуком.

**И одна вещь, которой в разборе не было.** Тонировка снимается **и в Puzzle, и в Mono**, хотя
красятся они от разного: в Puzzle цвет берётся от собранного блока, в Mono — от цвета трассы. Из
цепочки §7.1 это следует, но следует не сразу: оба режима различаются только тем, что попадает в
`CollisionColor`, а `ChannelSwitch #294` и `Set Vector #48` ниже по течению общие для обоих. То есть
точка вмешательства выбрана ниже развилки режимов — и поэтому одно исправление покрывает оба. Это
общий приём, а не частность: **править надо там, где сходятся варианты, а не там, где они
расходятся** — иначе на каждый режим нужен свой обход.

### 7.3 Как это устроено «правильно» — и что движок умеет сам

`InitializeBaseClassBuffers` смотрит `GetChild(8)`. Если NULL: `SetVertexColor` не вызывается,
массив цветов не аллоцируется, `CreateVertexBuffer` не добавляет `D3DFVF_DIFFUSE`, stride 24 → 20,
диффуз по умолчанию белый, текстура рисуется немодулированной. `CallChannel` каждый кадр сверяет
наличие child(8) с закэшированным флагом (`+0x1cc`) и при расхождении сам сбрасывает `+0x1c4` →
полная пересборка.

**Это не гипотеза про поведение fixed-function пайплайна, а наблюдаемый факт:** в шипнутых данных
уже есть три `Tune_Forest` вообще без порта 8 — `Actors/Rocket.cgr #47` (тяга ракеты),
`Effects/SkySpray.cgr #64` (брызги), `Render/Render_IndustrialTunnel.cgr #3475`. Плюс у самих
Debris-каналов порт 9 уже отцеплен (`CHLI = -1`). Бесцветный путь — рабочий продакшн-путь игры.

Отсюда три способа, и **первый приведён для полноты, а не как рекомендация** (см. конец §6):

| | Способ | Оценка |
|---|---|---|
| а | Холодный патч `Debris.cgr`: у #94, #233, #234 поставить `CHLI = -1` в link-записи порта 8 | 4 байта на канал, размер файла не меняется. **Правит файлы игры — нежелательно** |
| б | Плагин: обнулить элемент `inputChannelsList_` (`A3d_Channel +0x24`) для порта 8 | обратимо на лету, движок сам пересоберёт. C++, не Lua |
| в | Расширить Lua API (§8), затем `tw.on_call("Debris.cgr", 48, "after", …)` и переписывать только что записанную строку в белый | одна запись на порождённую частицу; `on_call` принимает индекс канала, так что generic-имя «Set Vector» не мешает. **← сделано и проверено в игре**, `assets/scripts/particles.lua` |

### 7.4 Что означает результат — и почему шипнутый набор текстур это карта

Замер по самим текстурам: **`particles1.png`, `particles2.jpg`, `particles3.jpg` строго
монохромны** — средняя цветность (`max−min` по каналам на непустых пикселях) ровно **0.0**. У
`tiles.png`, для сравнения, цветность **18.1**.

Читать это надо в обратную сторону, чем кажется на первый взгляд. Атласы партиклов монохромны **не
потому, что автор так захотел, а потому, что выбора не было**: движок красит их сам, и любой цвет,
положенный в атлас, домножается на вершинный. Положишь туда цветную картинку — получишь кашу из
случайных оттенков, а не свою палитру. Монохромность здесь — вынужденная, и ровно её и снимает §7.

Отсюда правильная формулировка результата: **белёсые частицы — это не разочарование, а сигнал
успеха.** Он означает, что модуляция снята и с этого момента цвет берётся оттуда, откуда его положил
автор набора. То есть «выключить окраску» и «рисовать своими цветами» — не разные задачи, а две
половины одной: первую закрывает §8, вторую — уже существующий `texture_hook` по имени канала
(`particles1` / `particles2` / `particles3`, §1).

И то же измерение объясняет, почему `tiles.png` цветной: решётку движок не тонирует так же
агрессивно, и там вольность раскрашивать атлас у автора **уже есть**. Так что двадцать файлов в
`engine/textures/` — это готовая карта того, где автору текстур-пака можно красить, а где до сих пор
было нельзя: **монохромный шипнутый атлас = движок красит сам**. `Docs/texturing.md` предупреждает
об этом ровно в тех же местах («game colors it too, and overlaying your and game' color might look
not as good as you imagine») — и после §8 это предупреждение станет условным, а не безусловным.

Практический вывод для §8: ценность разобранного кейса не в самих партиклах, а в том, что он
**снимает ограничение с формата текстур-паков**. Это стоит отразить и в `Docs/texturing.md`, когда
возможность появится.

**Возможность появилась, и в `Docs/texturing.md` это отражено** (раздел про партиклы): предупреждение
«игра красит их тоже» теперь названо условием, а не законом, со ссылкой на скрипт, который это
условие снимает. Подтверждено в живой игре — тонировка уходит и в Puzzle, и в Mono, где красится она
от разного (§7.2).

---

## 8. Что из этого следует для Lua API

Раздел был **входом для будущей работы над скриптовым слоем, а не описанием существующего** — списком
того, обо что упёрся §7. Три из четырёх пунктов с тех пор сделаны; таблица оставлена целиком, потому
что колонка «осторожность» писалась до реализации и её стоит сверять с тем, что вышло на самом деле.

| Чего нет | Что бы это дало | Осторожность |
|---|---|---|
| ~~запись в **векторный** канал~~ **есть** (Ф5-A) | перекраска `GridColor`, любого `Value Vector` | слот 19 у векторных — `SetFloat(int, float)`; писать только через слот 18 `SetVector`, и только после проверки `baseguid` (engine-журнал §2.2.2, §3.2). **Оправдалось**; добавилось непредвиденное — `SetVector` пробрасывает запись в числовых детей |
| ~~запись в **`Array Vector`**~~ **есть** (Ф5-C) | §7 целиком: точечная перезапись `Debris: Color[i]` | курсор обязан сохраняться и восстанавливаться, как уже делает `array:get`; слот 18 у этого типа **свой** и пишет именно в строку — engine-журнал §2.2.3 |
| ~~запись в **`Array Value`**~~ **есть** (Ф5-B) | симметрия с предыдущим | индекс — `float`, усекаемый в `int`, и границы не проверяет **никто**: строки за пределом таблицы движок молча **создаёт**, удлиняя её. Проверять индекс обязаны мы — `array:set` это и делает, движковым не-создающим `GetRow` (engine-журнал §2.2.3) |
| отцепить/подменить **ребёнка канала** (`GetChild(n) -> NULL`) | способ (б) из §7.3 без C++; общий рычаг для всех `Tune_*` | правит `inputChannelsList_`; нужен честный откат при выключении скрипта, как у `channel_shim` с vtable |

Первые две строки закрывали §7 полностью — и закрыли: с ними кейс работает в живой игре. Четвёртая
осталась несделанной. Она самая мощная и самая опасная: превращает «отключить окраску» в однострочник
без хука вообще, но открывает и способ оторвать каналу что угодно.

**Стоит отметить, что она больше не нужна для §7** — только для того, чтобы сделать то же самое
дешевле и обобщённее. Из «необходимости» она стала «оптимизацией», и решать про неё теперь можно без
давления флагманского кейса.

**План реализации этих четырёх пунктов — `lua-scripting.md`, Ф5.** Там же разобрано, почему они
делаются в три ступени, какие ABI-вопросы надо закрыть до кода и почему четвёртый пункт требует
другого обращения с откатом, чем `channel_shim`.

Сопутствующее, всплывшее по ходу и не относящееся к цвету:

- **`tw.on_call` уже умеет адресовать канал по индексу**, не только по имени — без этого `Set Vector
  #48` был бы недостижим (в группе десятки каналов с этим именем). Это стоит явно упомянуть в
  `Docs/scripting/api-reference.md`: сейчас сказано одной строкой в преамбуле и легко пропускается.
- **`tileflyup.png` грузится в канал с generic-именем `Texture`** (§1) — единственный эффект,
  который не адресуется по имени ни из скрипта, ни из `texture_hook`. Нужен другой дискриминатор.

---

## 9. Открытые вопросы

- **`Tune_Wall::UpdateBuffer` разобран только по портам, не по коду.** Порты сняты с экземпляра и
  подтверждены именами, но сама развёртка профиля вдоль хребта (как считаются UV по сегментам, что
  делает `SegmentTexture?`) не читалась.
- **`Tune_Thruster` и `Tune_Car`** не разбирались вообще — известно лишь, что каркас у них тот же.
- **Как `tileflyup` отличить в хуке.** Нужен дискриминатор помимо имени канала; вариант — сравнивать
  указатель на канал, разрешённый один раз по группе `MoneyFloaterCommander`, но
  `GetChannel(const char*)` по generic-имени `Texture` вернёт не тот. Не решено.
- **Порядок вызова *групп* за кадр** по-прежнему не выписан (переходит из engine-журнала §10).
  Внутри `Render_IndustrialTunnel` порядок теперь известен целиком, но как он чередуется с
  `RenderCommon`, `Debris`, `Puzzle` — нет. Детур на `CallStartChannel` покажет сразу.
- **`Do_SetConstants` (#2602)** — что именно ставится перед кадром, не смотрелось.
- **Числа в `PONM`** (geometry-журнал §11) могут оказаться связаны с `MapTexture`/`PlanarTexture`
  у `Tune_*`; не проверялось.
- **Ничего из §6 не проверено в живой игре** — всё выведено из кода и графа. Единственное
  исключение — текстурный хук, работоспособность которого доказана Skybox Replacer'ом.

---

## 10. Как это воспроизвести

Инструмент этой сессии — `Tools/CgrPy/rdy2use/render.py`:

```bash
uv run python render.py chain    <group.cgr> <channel>   # вниз: Surface -> геометрия/текстура
uv run python render.py callers  <group.cgr> <channel>   # вверх: кто зовёт, включая импорты
uv run python render.py texusers <group.cgr> <channel>   # какие Surface используют текстуру
```

Декомпиляция кастомных каналов — по рецепту engine-журнала §11, модули
`channels/{Tune_Forest,TuneWall,Tune_Car,TuneThruster,CustomGeometry}.dll`. Ghidra деманглит имена
сама, так что `Aco_Tune_Forest::UpdateBuffer` в дампе называется по-человечески. Дамп vtable —
тем же PE-ридером, что и в прошлой сессии; конец таблицы опознаётся по строке с именем типа сразу
за последним слотом.

Поиск загрузчиков текстур — запрос по графу: `Lua Script`, у которого ребёнок 0 — текстовый канал,
а ребёнок 1 — `Texture`; относительный путь берётся со **второго** операнда `Merge Text`, а не с
сохранённого значения самого оператора.

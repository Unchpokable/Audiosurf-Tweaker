# Полевой журнал реверса Audiosurf — 3D-геометрия

Сессия 2026-08-31. Четвёртый файл журнала. Предыдущие три разбирали контейнер `.cgr` и Lua
(`reversing-journal-lua.md`), граф каналов как язык игровой логики
(`reversing-journal-gameplay.md`) и нативное ядро движка (`reversing-journal-engine.md`). Этот
разбирает **геометрию**: в каком виде модели лежат в `.cgr`, как движок превращает их в
D3D-буферы, и что из этого следует для замены моделей — холодной (патч файла) и горячей (подмена
в работающей игре).

Правило журнала прежнее: **проверенное отделяется от предположенного**. Всё, что ниже не помечено
как предположение, подтверждено декомпиляцией `LoadChannel`/`CreateVertexBuffer`, дампом vtable из
`.rdata` по таблице экспортов, либо пересчётом на всём корпусе (930 мешей, 52 файла) с нулём
расхождений.

Инструменты этой сессии — `cgr/mesh.py`, `cgr/container.py`, `cgr/patch.py` и CLI `meshes.py` в
`Temp/Dumped/_work/cgrpy/`, см. §10.

---

## Итог одним абзацем

Геометрия хранится **не в виде файлов моделей, а прямо в графе каналов**: канал типа
`3D ObjectData` (и любой его наследник) несёт вершинные и индексные массивы обычными чанками L2
(`VRCO`/`VPPI`/`VNNI`/`VTD0..2`/`PODA`), де-интерливленными по атрибутам. Формат — буквально
D3D: `VRFL` это FVF, `POTY` это `D3DPRIMITIVETYPE`, и во всём корпусе он равен 4
(`D3DPT_TRIANGLELIST`). Разбор оказался полностью механическим: 930 блобов, все размеры сходятся
до байта, все индексы в границах. Замена возможна обоими способами. **Холодная** — потому что L2
это плоский самоограниченный поток `TAG+len+payload` без единого абсолютного смещения: меш другого
размера просто вклеивается на место старого, и всё ниже по потоку сдвигается; контейнер (zlib +
XOR 0x04) пересобирается байт-в-байт. **Горячая** — потому что `Aco_DX8_ObjectDataChannel`
экспортирует полный набор сеттеров (`SetVertexCount`, `SetVertexPosition`, `SetIndex`, …) с
проверкой границ внутри, а `InvalidateDeviceObjects` (слот 17) сбрасывает флаг `+0xbc` и заставляет
движок пересобрать VB/IB из CPU-массивов на следующем кадре. Главная неочевидность:
**`CreateVertexBuffer` пересчитывает FVF заново из того, какие массивы ненулевые, и загруженный
`VRFL` при этом игнорирует** — `VRFL` управляет только тем, что аллоцирует загрузчик.

---

## 1. Где геометрия и сколько её

Семейство `21A8923D-B908-4104-AE88-B6718D8A8678` (`3D ObjectData`) — 15 типов каналов, все
наследуют `Aco_DX8_ObjectDataChannel` и, значит, **один и тот же `LoadChannel`**. Поэтому
запечённую геометрию несёт не только сам `3D ObjectData`, но и производные.

| Тип канала-владельца | Блобов |
|---|---|
| `3D ObjectData` | 808 |
| `Tune_Wall` | 50 |
| `HLSLObject` | 26 |
| `Tune_Forest` | 16 |
| `CustomGeometry` | 10 |
| `Array Table` (вложенные, см. §1.1) | 10 |
| `Tune_Thruster` | 5 |
| `Tune_Car` | 2 |
| `MorphObject`, `Value`, `Texter_Unicode` | по 1 |

**Итого 930 блобов в 52 файлах: 612 466 вершин, 777 446 треугольников.** Полный перечень —
`out/meshes.csv` (генерируется `meshes.py inventory`), сводка по файлам — §9.

Типы того же семейства, которые геометрию **не** хранят, а строят в рантайме: `Primitive` (949
экземпляров), `3DTextFromTexture` (706), `SkinnedCharacter` (3), `ParticleObject` (2),
`FastMultiObjectData` (1), `3DText` (1). Холодный патч к ним неприменим; горячая замена — вполне
(§6), они те же `Aco_DX8_ObjectDataChannel`.

### 1.1 Меш ≠ канал: блобы бывают вложенными

Ловушка, на которую легко наступить. `Render/FixedChainspans.cgr`, канал **#72** — это
`Array Table` с именем `Chainspans Table`, и внутри **его же записи**, после колонок таблицы,
лежат **десять полных мешей** `Tune_Wall`, каждый со своим `CHNA`, но **без** своего `CHIX`.
Проверено раскладкой байт: `CHIX 72` @21087, десять `VRCO` на 23507…37307, следующий `CHIX 73`
только @39129. При этом `CHCO` сходится с числом разобранных каналов (330), то есть парсер не
терял синхронизацию — структура действительно такая.

**Практический вывод:** адресовать меши надо **порядковым номером вхождения `VRCO` в потоке L2**,
а не индексом канала. Ключевание по каналу молча склеило бы эти десять в один. Именно так устроен
`cgr/patch.py`: `MeshRef.ordinal`.

---

## 2. Формат: чанки одного меша

Подтверждено декомпиляцией `Aco_DX8_ObjectDataChannel::LoadChannel` (RVA `0x4140` в
`channels/21A8923D-….dll`) и пересчётом на всех 930 блобах.

| Чанк | Тип | Смысл | Поле объекта |
|---|---|---|---|
| `VRCO` | `u32` | число вершин | `+0x98` |
| `VRFL` | `u32` | **D3D FVF** | `+0xb8` |
| `VPPI` | `f32[3n]` | позиции | `+0x9c` (гейт `FVF & 0x002` `XYZ`) |
| `VNNI` | `f32[3n]` | нормали | `+0xa0` (гейт `FVF & 0x010` `NORMAL`) |
| `VTD0` | `f32[2n]` | texcoord 0 | `+0xa4` (гейт `FVF & 0x100`) |
| `VTD1` | `f32[2n]` | texcoord 1 | `+0xa8` (гейт `FVF & 0x200`) |
| `VTD2` | `f32[2n]` | texcoord 2 | `+0xac` (гейт `FVF & 0x300`) |
| `VTO0/1/2` | 24 байта | `VertTUVOffset` на набор | `+0xc4 + set*0x18` |
| `VCNC` | `u32[n]` | цвета вершин `D3DCOLOR` | `+0xb0` |
| `POCO` | `u32` | **число индексов**, не треугольников | `+0x94` |
| `PODA` | `u16[POCO]` | 16-битные индексы | `+0x8c` |
| `PO32` | `u32[POCO]` | 32-битные индексы | `+0x90` |
| `POTY` | `u32` | `D3DPRIMITIVETYPE` | `+0xc0` |
| `PONM` | `u32[3]` | режим текстурного маппинга по стадиям | `+0xc8` |
| `POTT` | `u32[3]` | текстурный трансформ по стадиям | `+0x194` |

Порядок в файле — ровно порядок чтения в `LoadChannel`: `VRCO VRFL VPPI VNNI VTD0 VTO0 [VTD1 VTO1]
[VTD2 VTO2] [VCNC] POCO PODA POTY PONM POTT`. Он же и порядок записи; иначе загрузчик пропустит
чанк (`GetInfoIfTag` идёт по потоку последовательно).

**Данные де-интерливлены**: каждый атрибут — свой сплошной массив, а не один interleaved-буфер.
Интерливинг делается один раз при создании VB (§4).

Значения, встречающиеся в корпусе:

| Поле | Значения |
|---|---|
| `VRFL` | `0x312` (585), `0x112` (215), `0x212` (6), `0x012` (2) — то есть `XYZ\|NORMAL` плюс `TEXn<<8` |
| `POTY` | **4 у всех 901 блоба с геометрией** — `D3DPT_TRIANGLELIST` |
| `VTOn` | у всех 1982 наборов одно и то же: `(0,0,0,0,1,1)` |
| `PONM` | `(7,7,7)` в 794 из 808; изредка `(5,0,7)`, `(3,7,7)`, `(2,7,7)`, `(4,7,7)` |
| `POTT` | `(0,0,0)` в 801 из 808; изредка `(0,1,0)`, `(0,2,0)` |

Теги, которые загрузчик принимает, но шипнутый корпус не использует: `VPDA`/`VNDA`/`VTDA`/`VTOP`
(написания более ранней ревизии — читаются в те же поля), `PO32`, `VCDA`, `POTM`.

### 2.1 `VRFL` недоговаривает про цвета

70 мешей несут `VCNC` (цвета вершин), и **ни один из них не выставляет `D3DFVF_DIFFUSE` (0x040) в
`VRFL`**. Значит, `LoadChannel` читает `VCNC` без гейта по FVF — в отличие от позиций, нормалей и
UV. Противоречия тут нет: цвет всё равно попадёт в отрисовку, потому что FVF на этапе создания
буфера пересчитывается из указателей (§4), а не берётся из `VRFL`.

Отсюда правило для писателя: **синтезировать бит `0x040` при записи не надо** — шипнутые данные
его не ставят, эффект такой правки не проверен, а движку он не нужен. `cgr/mesh.py:recompute_fvf`
это соблюдает намеренно.

---

## 3. Раскладка объекта и vtable

`??_7Aco_DX8_ObjectDataChannel@@6B@` @ RVA `0x7178`, **78 слотов**. Конец таблицы виден глазом:
сразу за слотом 77 в `.rdata` лежит строка `"3D ObjectData"` — тот же приём, что и в
`reversing-journal-engine.md` §11.

Слоты 0–16 — база `A3d_Channel` (см. engine-журнал §2.1). Слоты 17–19 приходят от
`Aco_DX8_D3DDeviceUse`, собственные начинаются с 20.

| Слот | Смещение | Метод |
|---|---|---|
| 1 | `+0x04` | `CallChannel` |
| **17** | **`+0x44`** | **`InvalidateDeviceObjects`** |
| 19 | `+0x4c` | `Release` |
| 20 | `+0x50` | `ReleaseVertexData` |
| 21 | `+0x54` | `UpdateData` |
| 22 | `+0x58` | `StreamVertexData` |
| 24 | `+0x60` | `CreateVertexBuffer` |
| 25 / 26 | `+0x64` / `+0x68` | `SetVertexCount` / `GetVertexCount` |
| 27 / 28 | `+0x6c` / `+0x70` | `SetIndexCount` / `GetIndexCount` |
| 29 / 30 | `+0x74` / `+0x78` | `SetIndex` / `GetIndexes` |
| 31 / 32 | `+0x7c` / `+0x80` | `SetVertexPosition` / `GetVertexPosition` |
| 33 / 34 | `+0x84` / `+0x88` | `SetVertexNormal` / `GetVertexNormals` |
| 35 / 36 | `+0x8c` / `+0x90` | `SetVertexTUCoord` / `GetVertexTUV` |
| 37 / 38 | `+0x94` / `+0x98` | `SetVertexColor` / `GetVertexColor` |
| 39 / 40 | `+0x9c` / `+0xa0` | `SetPolygonType` / `GetPolygonType` |
| 52 / 53 | `+0xd0` / `+0xd4` | `GetFVFlags` / `GetVertexSize` |
| 61 | `+0xf4` | `CalculateBoundingBox` |
| 71 | `+0x11c` | `RenderForShader` |
| 75 / 76 | `+0x12c` / `+0x130` | `GetVertexBuffer` / `GetIndexBuffer` |

**Важно для безопасности.** Слот 17 у этого семейства — `InvalidateDeviceObjects`, а не `GetFloat`.
Это ещё одно подтверждение правила engine-журнала §3.2: у слота 17 смысл задаёт производный тип.
Позвать `GetFloat` (`+0x44`) на геометрическом канале — значит **уничтожить его D3D-буферы**, а не
получить неверное число. Проверять `baseguid` перед вызовом обязательно.

Поля объекта, помимо перечисленных в §2:

```
+0x84  IDirect3DVertexBuffer9*      +0x88  IDirect3DIndexBuffer9*
+0xb4  vertex stride (байт)         +0xbc  bool «буферы валидны»
+0x7c  Aco_DX8_D3DDeviceUse*        +0x130 D3DXMATRIX world (64 байта)
```

Все методы **ещё и экспортируются по декорированному имени**, поэтому доступны и через
`GetProcAddress` с явным `this` — ровно как в `framework/texture_hook.cxx`. Правило выбора между
экспортом и vtable — engine-журнал §2.5.

---

## 4. Как из чанков получается draw call

### 4.1 `CreateVertexBuffer` (слот 24) — интерливинг и **пересчёт FVF**

```c
this[0xbc] = 0;                                  // буферы невалидны
if (vertexCount == 0) return false;
if (vertexCount > 0xffff && caps.MaxVertexIndex < vertexCount) return false;

this->fvf = 0;  this->stride = 0;                // <-- ЗАГРУЖЕННЫЙ VRFL ЗАТИРАЕТСЯ
if (positions) { fvf |= 0x002; stride += 12; }
if (normals)   { fvf |= 0x010; stride += 12; }
if (colors)    { fvf |= 0x040; stride +=  4; }
if      (uv2)  { fvf |= 0x300; stride += 24; }
else if (uv1)  { fvf |= 0x200; stride += 16; }
else if (uv0)  { fvf |= 0x100; stride +=  8; }
```

Затем вершины **интерливлятся в порядке FVF**: `position(12) normal(12) diffuse(4) uv0(8) uv1(8)
uv2(8)`, копируются в залоченный VB, и создаётся IB:
`CreateIndexBuffer(count*2, D3DUSAGE_WRITEONLY, D3DFMT_INDEX16)` для `PODA` либо
`(count*4, …, D3DFMT_INDEX32)` для `PO32`. В конце `this[0xbc] = 1`.

Два следствия, оба практические:

- **`VRFL` в файле — только для загрузчика.** На отрисовку идёт FVF, выведенный из наличия
  массивов. Значит, испортить `VRFL` нельзя «наполовину»: он либо описывает те чанки, что реально
  лежат в файле (и тогда всё загрузится), либо нет (и тогда массив просто не будет прочитан).
- **Индексы валидируются.** Перед заполнением IB движок проходит по всем индексам и при
  `vertexCount < index` печатает `Aco_DX8_ObjectDataChannel::CreateVertexBuffer Invalid index data`
  и возвращает `false` — то есть **не рисует**, а не падает. Ошибка в индексах даёт исчезнувшую
  модель, а не краш. (Сравнение строгое `<`, так что `index == vertexCount` проскакивает — но
  писать такое всё равно нельзя.)
- `CreateVertexBuffer` **не освобождает** предыдущие `+0x84`/`+0x88` перед перезаписью. Значит,
  вызывать его на объекте с живыми буферами — это утечка. Правильный порядок — сначала
  `InvalidateDeviceObjects` (§6).

### 4.2 Отрисовка

`CallChannel` (слот 1) → `StreamVertexData` (слот 22): `SetStreamSource(0, VB, 0, stride)`,
`SetIndices(IB)`, `DrawIndexedPrimitive`. Ветка по `POTY`: `4` → `primCount = POCO / 3`.
Реализованы только типы 1–4; `TRIANGLESTRIP`/`FAN` (5/6) в `switch` отсутствуют, что согласуется с
тем, что корпус содержит только `POTY = 4`.

`RenderForShader` (слот 71) — второй путь, для `HLSLObject`/`ShaderSurface`; та же связка буферов.

`GetVertexBuffer`/`GetIndexBuffer` перед возвратом дёргают `UpdateData` (слот 21), а тот, если
`+0xbc == 0`, вызывает `CreateVertexBuffer`. То есть **пересборка ленивая и происходит сама** —
достаточно сбросить флаг.

### 4.3 Где геометрия в графе

```
3D Object                       (узел сцены, 2019 экземпляров)
  порт 0 -> Motion / Matrix     трансформ  (1870 Motion + 78 матричных)
  порт 2 -> Surface             материал+геометрия
Surface                         (2489 экземпляров)
  порт 0 -> 3D ObjectData       ГЕОМЕТРИЯ  (818 связей; + Primitive 936, 3DTextFromTexture 635, …)
  порт 1 -> Material            параметры освещения
  порт 2 -> Texture             текстура   (1791 Texture, 43 RenderTexture, …)
```

То есть `3D Object` **не** ссылается на геометрию напрямую — только через `Surface`. Это же
объясняет, почему уже существующий `texture_hook` цепляется именно на текстурную ветку.

`Motion` несёт `MATR` — 64 байта, `D3DXMATRIX` (2465 штук). **Осторожно:** это сохранённый
редактором снимок последнего рантайм-значения, а не авторский локальный офсет — у `PlayerCar_Skinny`
там мировые координаты вида `(8202, -55, 3359)`. Та же оговорка, что и для `FLVA` у `Value` в
gameplay-журнале. Для статического просмотрщика как стартовая точка годится, как истина — нет.

---

## 5. Холодная замена: патч `.cgr`

### 5.1 Контейнер пересобирается байт-в-байт

Перепись census по 161 файлу:

| Слои | Файлов |
|---|---|
| `ACTF/ZIOS/ZINS/ZICB` → `ACTF/NECL/NECT/NEOS/NECB` → `QVRS` | 109 |
| `ACTF/ZIOS/ZINS/ZICB` → `QVRS` (без XOR) | 42 |
| `QVRS` открытым текстом | 10 |

Поля: `ACTF` = GUID канала-кодека (`7DFC389A-…` `ZipCompression`, `FBB1D22B-…` `NoEditorLoad`),
`ZIOS` = размер до сжатия, `ZINS` = после (`== len(ZICB)`), `NECL` = `len(NECT)`,
`NECT` = `b"Protected files"`, `NEOS` = `len(NECB)`, `NECB` = payload XOR `0x04`.

**Проверено:** `rewrap(unwrap(f))` воспроизводит внутренний слой **побайтово на всех 109**
XOR-файлах. Внешний zlib-поток отличается только уровнем компрессии, которого загрузчику
безразлично.

### 5.2 Сплайс безопасен по построению

L2 — плоский самоограниченный поток `TAG + u32 len + payload`. **Абсолютных смещений нет нигде**:
запись канала ограничена только следующим `CHIX`, групповые счётчики (`CHCO`) считают каналы, а не
байты. Поэтому меш другого размера вклеивается на место старого, и всё ниже по потоку просто
сдвигается. Каналы не добавляются и не удаляются — `CHCO` не трогается.

Проверки, которые это подтвердили:

- **Идентичный патч на всём корпусе байт-в-байт:** 901 меш с геометрией, `replace_mesh(core, i,
  read_mesh(core, i)) == core` — 893 совпали побайтово, остальные 8 честно отвергнуты валидатором
  (§5.3); ни одного расхождения.
- **Патч с изменением размера:** `PlayerCar_Skinny.cgr` #0, 155 верш./306 тр. → куб 8/12. Файл
  21 349 → 18 350 байт; при переразборе — 285 каналов (как было), `CHCO = 285`, 0 непрочитанных
  байт, новая геометрия на месте, **остальные 284 канала побитово идентичны**.

### 5.3 Чего патчить не надо

8 блобов — `CustomGeometry` (плюс один `Square_Custom`), у которых `POTY = 4`, но `POCO` не делится
на 3: `AwardFare` / `RouteMap` / `ServerMessages` / `VehicleSelector` / `SongSelector` /
`start - project loader` (по 4 вершины, `POCO = 2`), `Render_IndustrialTunnel` #100 (16 вершин,
`POCO = 8`), `RoadSupports` #0 (550 вершин, `POCO = 440`). Это сохранённые снимки каналов, которые
геометрию **генерируют в рантайме**, — движок их всё равно перезапишет. Читать можно, патчить
бессмысленно. Валидатор `Mesh.validate()` их отклоняет, и это правильное поведение.

Ещё 29 блобов — «объявление без данных»: есть `VRCO`/`VRFL`/`POCO`/`POTY`, нет ни `VPPI`, ни
`PODA` (`HLSLObject` в `Render_IndustrialTunnel`, `CustomGeometry` в `RouteMap`, два в
`XX_StartHere`). У них `Mesh.has_geometry == False`.

### 5.4 Предел на размер

`PODA` 16-битный, поэтому потолок — **65 535 вершин на меш**. Движок умеет и `PO32`
(`SetIndex` сам выбирает 32-битный путь при `vertexCount >= 0x10000`), но тогда включается
проверка `caps.MaxVertexIndex` в `CreateVertexBuffer`, и на слабом железе меш просто не отрисуется.
Писатель в `cgr/mesh.py` `PO32` намеренно не эмитит и отказывается работать выше 0xFFFF.

---

## 6. Горячая замена: подмена в работающей игре

Всё нужное **уже экспортируется** и снабжено проверками границ внутри — своей арифметики по
указателям не требуется.

`SetVertexCount` внутри зовёт `ReleaseVertexData` (слот 20), а тот освобождает **все** CPU-массивы
(`+0x9c`…`+0xb0`), обнуляет счётчик вершин и сбрасывает `+0xbc`. Индексные массивы он **не**
трогает — их освобождает `SetIndexCount`. `SetVertexPosition`/`SetVertexNormal`/`SetVertexTUCoord`
аллоцируют свой массив сами при первом обращении и проверяют индекс против `vertexCount`;
`SetIndex` — против `indexCount`.

Рабочая последовательность (всё — на потоке движка, он же поток `EndScene`, см. engine-журнал §4.1;
дополнительная синхронизация не нужна):

```c
obj->SetVertexCount(n);                       // слот 25 — попутно чистит старые массивы
for (i) {
    obj->SetVertexPosition(pos[i], i);        // слот 31
    obj->SetVertexNormal(nrm[i], i);          // слот 33
    obj->SetVertexTUCoord(/*stage*/ 0, uv[i], i);  // слот 35
}
obj->SetIndexCount(m);                        // слот 27
for (i) obj->SetIndex(idx[i], i);             // слот 29
obj->SetPolygonType(4);                       // слот 39
obj->CalculateBoundingBox();                  // слот 61 — иначе отсечение по старому боксу
obj->InvalidateDeviceObjects();               // слот 17 — освобождает VB/IB, ставит +0xbc = 0
```

Дальше ничего делать не надо: ближайший `GetVertexBuffer`/`UpdateData` увидит `+0xbc == 0` и
пересоберёт буферы (§4.2).

Что здесь существенно и почему:

- **`InvalidateDeviceObjects` обязателен, а не «на всякий случай».** `CreateVertexBuffer` не
  освобождает старые `+0x84`/`+0x88` перед перезаписью (§4.1) — без инвалидации это утечка
  D3D-ресурсов на каждую подмену.
- **`CalculateBoundingBox` тоже.** Бокс используется `GetIfObjectIsVisible` для отсечения; со
  старым боксом новая модель может пропадать при определённых ракурсах.
- **Порядок `SetVertexCount` → сеттеры → `SetIndexCount` → `SetIndex` менять нельзя.**
  `SetVertexCount` обнуляет массивы, так что вызванный после заполнения он сотрёт работу. А
  `SetIndex` выбирает разрядность индексов по **текущему** `vertexCount`, поэтому вершины должны
  быть посчитаны раньше индексов.
- **Резолвить канал — один раз, на холодном пути.** `A3d_ChannelGroup::GetChannel(const char*)` —
  линейный скан со `_stricmp` (engine-журнал §6). Кэшировать указатель, инвалидировать по
  выгрузке группы.
- **Проверять тип до вызова.** Слот 17 у геометрии — `InvalidateDeviceObjects`, у числового
  канала — `GetFloat` (§3). Дискриминатор — `baseguid == 21A8923D-…` через `GetChannelType`.

Про `Primitive` / `3DTextFromTexture` / `SkinnedCharacter`: они наследуют тот же интерфейс, так что
горячая подмена к ним применима, **но** их собственный `CallChannel` перегенерирует геометрию, и
подменённые данные будут затёрты. Тут понадобится либо подмена vtable у конкретного объекта
(`channel_shim`, engine-журнал §7 «что делать вместо этого»), либо перехват их `CallChannel`.
Не проверялось.

---

## 7. Чтение для стороннего просмотрщика

Достаточно `cgr/mesh.py` — сторонний рендерер получает готовые массивы:

- `positions` — `float3`, локальное пространство меша;
- `normals` — `float3`, единичные;
- `uvs[0..2]` — `float2`, D3D-раскладка (V сверху вниз);
- `colors` — `u32 D3DCOLOR` (ARGB), если есть `VCNC`;
- `indices` — треугольный список, всегда (`POTY = 4` у всех 901).

`Mesh.to_obj()` пишет Wavefront OBJ. Единственное преобразование — **V переворачивается**
(`v -> 1-v`), потому что D3D кладёт начало текстурных координат сверху, а OBJ снизу. Порядок обхода
(winding) не трогается: игра рисует со своим режимом отсечения, и round-trip нетронутого меша обязан
быть тождественным. `Mesh.from_obj()` — обратная операция; она разворачивает независимые
OBJ-индексы `v/vt/vn` в один D3D-индекс на вершину (каждая уникальная тройка становится своей
вершиной) и веером триангулирует многоугольники.

Проверено: **930 из 930 блобов проходят OBJ round-trip**, индексы совпадают точно, расхождение
позиций — не более `4.9e-06` и вызвано только текстовой точностью `%.6g` в самом OBJ.

Для сборки целой модели из частей нужен трансформ из `Motion.MATR` — с оговоркой §4.3 о том, что
это снимок, а не авторский офсет.

---

## 8. Проверки, на которых всё это стоит

Всё воспроизводится `uv run python meshes.py verify`:

| Проверка | Результат |
|---|---|
| блобов найдено сканом потока = сырых сигнатур `VRCO` | 930 = 930, во всех 161 файле |
| размеры массивов сходятся с `VRCO`/`POCO` | 901/901 |
| все индексы `< vertexCount` | 901/901 |
| `POTY` | `4` у 901/901 |
| декодирование → пере-кодирование побайтово | 893 (+29 «объявление без данных», +8 отвергнуты валидатором) |
| OBJ round-trip | 930/930 |
| пересборка контейнера (внутренний слой) побайтово | 109/109 |
| идентичный патч не меняет ни байта | 893/893 |
| патч с изменением размера → переразбор чистый | ✓ (`CHCO` сходится, соседние каналы не тронуты) |

Оговорка про два повреждённых файла: `XX_StartHere.cgr` и `Render/Render_IndustrialTunnel.cgr` у
графового парсера теряют синхронизацию (20 968 и 38 непрочитанных байт соответственно). На
геометрию это **не влияет**: `patch.scan_meshes` / `read_mesh` работают прямо по потоку L2 и от
качества разбора графа не зависят — счёт блобов в этих файлах сходится с сырым сканом сигнатур.
Не восстанавливается там только `channel_type` в инвентаре (колонка покажет `?` или соседний тип —
отсюда `Value` и `Texter_Unicode` среди владельцев в §1).

---

## 9. Инвентарь: где что лежит

Полный машиночитаемый список — `Temp/Dumped/_work/cgrpy/out/meshes.csv`, по строке на блоб:
`file, ordinal, channel, channel_type, name, verts, tris, uv_sets, fvf, poly_type, bytes,
size_x/y/z`. Сводка по файлам (52 файла с геометрией из 161):

| Файл | Мешей | Вершин | Треуг. |
|---|---:|---:|---:|
| `Render/Render_IndustrialTunnel.cgr` | 106 | 331 720 | 457 416 |
| `Environment/SetPiecesBlack2.cgr` | 47 | 76 364 | 101 329 |
| `Environment/Squid_Black.cgr` | 124 | 45 156 | 73 498 |
| `Render/RenderCommon.cgr` | 96 | 48 479 | 54 286 |
| **`Actors/PlayerCar_Sword.cgr`** | 45 | 11 142 | 11 488 |
| `Intros/RouteMap.cgr` | 3 | 5 004 | 7 992 |
| `Environment/Squid_White.cgr` | 114 | 8 957 | 7 424 |
| `Environment/SetPiecesBlack3.cgr` | 28 | 7 327 | 5 262 |
| `Environment/SetPiecesBlack.cgr` | 33 | 7 335 | 4 232 |
| `Actors/Boss_TrafficRunner.cgr` | 5 | 4 095 | 3 434 |
| `Environment/SetPiecesWhite.cgr` / `White2.cgr` | 23 / 23 | 5 708 / 5 708 | 3 164 / 3 164 |
| **`Actors/PlayerCar_Boomer2.cgr`** | 13 | 3 213 | 3 034 |
| **`Actors/PlayerCar_EraserElite.cgr`** | 16 | 4 261 | 2 934 |
| **`Actors/PlayerCar_Vegas.cgr`** | 11 | 3 603 | 2 914 |
| **`Actors/PlayerCar_Sword3.cgr`** | 13 | 4 561 | 2 876 |
| **`Actors/PlayerCar_RocketPro.cgr`** | 21 | 3 309 | 2 706 |
| **`Actors/PlayerCar_Sword2.cgr`** | 13 | 3 436 | 2 558 |
| **`Actors/PlayerCar_Boomer.cgr`** | 12 | 2 144 | 2 336 |
| `XX_StartHere.cgr` | 14 | 4 305 | 2 319 |
| `Environment/SetPieces.cgr` | 22 | 3 750 | 2 224 |
| **`Actors/PlayerCar_Rocket2.cgr`** | 15 | 2 782 | 1 982 |
| **`Actors/PlayerCar_Jetboat.cgr`** | 4 | 955 | 1 926 |
| **`Actors/PlayerCar_Rocket.cgr`** | 15 | 2 578 | 1 832 |
| **`Actors/PlayerCar_Ovol2.cgr`** | 18 | 3 395 | 1 796 |
| **`Actors/PlayerCar_DragWarp.cgr`** | 13 | 2 221 | 1 664 |
| `Render/CopyPasteBuffer.cgr` | 3 | 886 | 1 488 |
| **`Actors/PlayerCar_Skinny.cgr`** | 5 | 733 | 1 482 |
| **`Actors/PlayerCar_Ovol.cgr`** | 18 | 2 280 | 1 324 |
| `Support/QuestionBoxOverlord.cgr` | 8 | 620 | 1 196 |
| остальные 22 файла (`Player_Pullers`, `ScoreFloaterCommander`, `Explosion`, `Debris`, `TuneThruster`, `Enemies_Flight`, `FixedChainspans`, `Rocket`, `XX_gui`, `Sprayer`, `TrafficHusks`, `LaserShot`, `RoadSupports`, `SprayerStraight`, `SkySpray`, `XX_OnlineHighScoresTable`, `DynamicSkyBackground`, `AwardFare`, `ServerMessages`, `VehicleSelector`, `SongSelector`, `start - project loader`) | 45 | 6 731 | 5 782 |
| **ИТОГО** | **930** | **612 466** | **777 446** |

Жирным — модели корабля игрока, то есть основная цель. Их **17 файлов** (по одному на корабль),
плюс `Actors/PlayerCar.cgr` (9 708 байт, геометрии не содержит — это диспетчер) и
`Actors/SpecialPurpose.cgr` (18 персонажей/режимов, см. gameplay-журнал).

Внутри корабля меши именованы осмысленно (`SkinnyHull.3ds`, `Turbine.3ds`, `TurbineWires`,
`Skinny_Fin`) — имя берётся из `CHNA` канала, у многих сохранилось исходное имя 3ds-файла.

---

## 10. Инструменты и как это воспроизвести

В `Temp/Dumped/_work/cgrpy/` добавлено:

| Модуль | Роль |
|---|---|
| `cgr/mesh.py` | `Mesh`: чанки ⇄ массивы, OBJ ⇄ меш, `validate()`, `recompute_fvf()` |
| `cgr/container.py` | `unwrap_file` / `rewrap` — снять и восстановить zlib + XOR (§5.1) |
| `cgr/patch.py` | `scan_meshes` / `read_mesh` / `replace_mesh(es)` по порядковому номеру (§1.1) |
| `meshes.py` | CLI |

```bash
uv run python meshes.py inventory                        # -> out/meshes.csv (930 строк)
uv run python meshes.py exportall <file.cgr> <out_dir>   # все меши файла в .obj
uv run python meshes.py export    <file.cgr> <N> <o.obj>
uv run python meshes.py replace   <file.cgr> <N> <in.obj> <out.cgr>
uv run python meshes.py verify                           # весь набор проверок §8
```

`patch.py`/`container.py` от `proj.pkl` не зависят — работают прямо по файлу, поэтому применимы и к
двум повреждённым группам (§8).

Декомпиляция канальной DLL — по рецепту engine-журнала §11:

```bash
export JAVA_HOME="C:\\Program Files (x86)\\Android\\openjdk\\jdk-21.0.8"
export PATH="/c/Program Files (x86)/Android/openjdk/jdk-21.0.8/bin:$PATH"
export GHIDRA_OUTPUT_DIR='C:/.../Temp/Dumped/_work/ghidra_out/ObjectData'
"C:/Users/Unchp/Ghidra/support/analyzeHeadless.bat" \
  'C:/.../Temp/Dumped/_work/ghidra_proj' ASChannels \
  -import 'C:/.../engine/channels/21A8923D-B908-4104-AE88-B6718D8A8678.dll' \
  -scriptPath 'C:/Users/Unchp/.claude/skills/ghidra/scripts/ghidra_scripts' \
  -postScript ExportAll.java
```

Ghidra здесь деманглит имена сама, так что функции в дампе сразу называются
`Aco_DX8_ObjectDataChannel::LoadChannel` и т.д. Дамп vtable — тридцатистрочным PE-ридером на чистом
Python (`IMAGE_EXPORT_DIRECTORY` + `rva → file offset`), как и в прошлой сессии; для «что лежит в
`??_7X@@6B@`» полный автоанализ избыточен.

---

## 11. Открытые вопросы

- **`PONM` и `POTT` не разобраны по значениям.** Известно, что это `u32[3]` по текстурным стадиям
  и что за ними стоят `GetTextureMapping` (слот 42) и `GetTextureTransform` (слот 70); `PONM = 7`
  почти везде. Что означают 7 / 5 / 4 / 3 / 2 и что означает `POTT = 1 / 2` — не выяснялось. На
  чтение и на патч это не влияет: значения переносятся как есть.
- **`VTOn` (`VertTUVOffset`, 24 байта)** во всём корпусе одинаков — `(0,0,0,0,1,1)`. Раскладку
  структуры (скорее всего offset/scale по UV) можно снять с `GetTUVOffSet`/`SetTUVOffSet`
  (слоты 57/58), но повода не было.
- **Процедурные типы под горячей заменой.** `Primitive`, `3DTextFromTexture`, `SkinnedCharacter`
  перегенерируют геометрию в своём `CallChannel`; подменённые массивы будут затёрты. Обход —
  подмена vtable у объекта; не проверялось (§6).
- **`SaveChannel`** (слот 5) не смотрелся. Если игра когда-либо сохраняет геометрию обратно, это
  единственное место, где это происходит.
- **`AddCustomDataChannel` / `GetCustomDataChannel`** (слоты 72–74) — механизм подвешивания
  произвольных каналов к геометрии. Назначение не выяснено.
- **`MorphObject`** (1 экземпляр, порты 0 и 1 оба на `3D ObjectData`) — очевидно морфинг между
  двумя мешами, но не разбирался.
- **Горячая замена не проверена в живой игре.** Всё в §6 выведено из декомпиляции; последовательность
  вызовов и обязательность `InvalidateDeviceObjects`/`CalculateBoundingBox` — вывод из кода, а не
  наблюдение. Это то же ограничение, что и у остального в песочнице (см. «Текущее состояние» в
  `overview.md`): требует запущенной игры и проверяется только пользователем.

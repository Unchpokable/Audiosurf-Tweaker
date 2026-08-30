# Полевой журнал реверса Audiosurf — нативное ядро движка и ABI каналов

Сессия 2026-08-29. Третий файл журнала. Первые два разбирали **данные**: `reversing-journal-lua.md`
— контейнер `.cgr` и Lua-движок, `reversing-journal-gameplay.md` — граф каналов как язык игровой
логики. Этот разбирает **код**: `HighPoly.dll`, `QuestViewer.exe` и DLL типов каналов, то есть то,
как движок физически исполняет граф, и за какие адреса за это можно зацепиться из плагина.

Правило журнала прежнее: **проверенное отделяется от предположенного**. Всё, что ниже не помечено
как предположение, подтверждено дизассемблером, дампом vtable из `.rdata`, таблицей экспортов PE
или пересчётом смещений против заголовков Quest3D SDK.

Прямое следствие этой сессии — **`Docs/Internal/lua-scripting.md`**: проект встраивания LuaJIT в
`TweakerPlugin`. Весь ABI ниже — его фундамент.

---

## Итог одним абзацем

Граф каналов Quest3D исполняется через **два слота vtable и ничего больше**: слот 1 (`+0x04`,
`CallChannel`) — путь действия, слот 17 (`+0x44`, `GetFloat`) — путь данных. Оба разрешаются
динамически на каждом узле, поэтому граф — это, буквально, интерпретатор, у которого «байткод»
хранится в виде списков детей. Кадр начинается в обычном message pump `QuestViewer.exe` (один
поток, `PeekMessage` → апдейт движка), каждая группа получает `A3d_ChannelGroup::CallStartChannel`,
та инкрементит tree count группы и зовёт `CallChannel` у стартового канала; дальше всё едет по
детям. Повторные вычисления гасит `A3d_Channel::CheckRenderCount` — мемоизация «один раз за tree
count». Раскладка объекта `A3d_Channel` совпадает с заголовком SDK **до `+0x70` включительно**, а
дальше расходится, поэтому читать поля производных классов через SDK-типы нельзя (тихо уедет на
8 байт). И главное практическое: **`?CallChannel@A3d_Channel@@UAEXXZ` — это один байт `c3`**,
пустая заглушка базового класса, свёрнутая COMDAT'ом с четырьмя другими пустышками; её
переопределяют 159 типов каналов из 226, что покрывает **82.7 % всех каналов проекта**. Значит,
детур на этот символ **не может** служить событийным потоком — вопреки §7.2
`reversing-journal-gameplay.md`, которую эта запись отменяет (см. §7).

---

## 1. Кто есть кто среди модулей

| Модуль | Размер | Функций | Роль |
|---|---|---|---|
| `engine/HighPoly.dll` | 131 072 | 653 | **ядро**: `A3d_Channel`, `A3d_ChannelGroup`, `EngineInterface`, списки, загрузчик `.cgr` |
| `engine/QuestViewer.exe` | 69 632 | — | shell: окно, message pump, вызов апдейта движка |
| `engine/channels/<GUID>.dll` | 7–130 КБ × 242 файла | — | 226 типов каналов по `channels.lst`; вся семантика узлов живёт здесь |
| `Audiosurf.exe` | 33 280 | — | лаунчер |

Image base у `HighPoly.dll` и у всех channel-DLL — `0x10000000`. Разрядность — x86, компилятор
VS2003 (по сигнатурам библиотечных функций, опознанным Ghidra).

Важное следствие раскладки: **ядро не знает ни одного типа канала**. Всё, что оно умеет, — дёргать
виртуальные методы. Поэтому любой перехват на уровне ядра либо глобален (детур на `HighPoly.dll`),
либо адресуется через vtable конкретного объекта.

---

## 2. ABI канала — полная vtable

Дампы сняты прямо из `.rdata` по адресам `??_7<Class>@@6B@`, имена восстановлены сопоставлением
адресов с таблицей экспортов.

### 2.1 База: `A3d_Channel` — 17 слотов

`??_7A3d_Channel@@6B@` @ RVA `0x13138` в `HighPoly.dll`:

| Слот | Смещение | Метод | Адрес (RVA) |
|---|---|---|---|
| 0 | `+0x00` | `scalar deleting destructor` | `0x013d0` |
| **1** | **`+0x04`** | **`CallChannel`** | `0x023f0` ← **`ret`** |
| 2 | `+0x08` | `OneTimeInitialize` | `0x023f0` ← **тот же `ret`** |
| 3 | `+0x0c` | `DoEvent` | `0x0afb0` |
| 4 | `+0x10` | `GetChannelType` | `0x0c130` |
| 5 | `+0x14` | `SaveChannel` | `0x0c1a0` |
| 6 | `+0x18` | `LoadChannel` | `0x0cb00` |
| 7 | `+0x1c` | `SetChannelInterfaceType` | `0x0d320` |
| 8 | `+0x20` | `GetChannelFromChannel` | `0x0c840` |
| 9 | `+0x24` | `GetTimeStamp` | `0x0af80` |
| 10 | `+0x28` | `SetTimeStamp` | `0x0af90` |
| 11 | `+0x2c` | `DoDependencyInit` | `0x0b2b0` (пустышка) |
| 12 | `+0x30` | `CheckIndexNrRemoval` | `0x0ca20` |
| 13 | `+0x34` | `GetInternalChild` | `0x0ad70` |
| 14 | `+0x38` | `CheckPublicChannelInstanceRemoval` | `0x0c800` |
| 15 | `+0x3c` | `AddMetaData` | `0x0b2b0` (пустышка) |
| 16 | `+0x40` | `SetChannelGroup` | `0x0b2c0` |

`GetFeedbackClass` из заголовка SDK в vtable **отсутствует** — заголовок новее шипнутого движка
(см. §3.1).

### 2.2 Числовое расширение: `Aco_FloatChannel` — слоты 17–23

Одинаково у всех числовых типов (`Value`, `Expression Value`, `ValueOperator`, `Trigger`,
`Array Value`, `Lua Script`, …):

| Слот | Смещение | Метод |
|---|---|---|
| **17** | **`+0x44`** | **`GetFloat`** — вычислить и вернуть |
| 18 | `+0x48` | `GetOldFloat` |
| **19** | **`+0x4c`** | **`SetFloat`** |
| 20 | `+0x50` | `GetDefaultFloat` |
| 21 | `+0x54` | `SetDefaultFloat` |
| 22 | `+0x58` | `GetDefaultFloatSetting` |
| 23 | `+0x5c` | `SetDefaultFloatSetting` |

### 2.2.1 Строковое расширение: `Aco_StringChannel` — слоты 17–29

Снято с `??_7Aco_StringChannel@@6B@` (RVA `0x2118` в `channels/6E6FB247-….dll`) сопоставлением
адресов слотов с RVA экспортов той же DLL. Таблица кончается на слоте 30 — дальше в `.rdata` лежит
строка `"Text"`.

| Слот | Смещение | Метод |
|---|---|---|
| **17** | **`+0x44`** | **`GetString`** → `const char*` |
| 18 | `+0x48` | `SetString(const char*)` |
| 19 | `+0x4c` | `SetSingeLine(bool)` |
| 20 | `+0x50` | `GetSingeLine` |
| 21 | `+0x54` | `SetMaxCars(int)` |
| 22 | `+0x58` | `SetIfUseWChar(bool)` |
| **23** | **`+0x5c`** | **`GetIfUseWChar`** → `bool` |
| **24** | **`+0x60`** | **`GetWString`** → `const WCHAR*` |
| 25 | `+0x64` | `SetWString` |
| 26 | `+0x68` | `AS2WS` |
| 27 | `+0x6c` | `WS2AS` |
| 28 | `+0x70` | `GetLastRect` |
| 29 | `+0x74` | `SetLastRect` |

Это подтверждает три константы, которые `src/lua/lua_channels.cxx` уже использовал (`0x44`, `0x5c`,
`0x60`), и закрывает пункт «строковый ABI» из §10. Практический вывод про `GetIfUseWChar`: канал
хранит **либо** narrow, **либо** wide, и отдаёт пустую/протухшую строку другого вида не жалуясь —
спрашивать режим обязательно, а не гадать.

### 2.2.2 Векторное расширение: `Aco_VectorChannel` — слоты 17–19

Снято с `??_7Aco_VectorChannel@@6B@` (RVA `0x2130` в `channels/9D045960-….dll`), тем же способом.
Таблица кончается на слоте 20 — дальше строка `"Value Vector"`. Своих слотов ровно три:

| Слот | Смещение | Метод |
|---|---|---|
| **17** | **`+0x44`** | **`GetVector`** → `D3DXVECTOR3` **по значению** |
| 18 | `+0x48` | `SetVector(D3DXVECTOR3)` |
| **19** | **`+0x4c`** | **`SetFloat(int, float)`** ← сигнатура **не та**, что у числового канала |

Два следствия, и оба про безопасность:

- **Слот 17 занят у всех трёх семейств и означает у каждого своё**: `GetFloat` / `GetString` /
  `GetVector`. Это не совпадение, а прямое следствие §2.1 — 17 слотов базы, дальше каждый
  производный тип раскладывает своё. Отсюда и правило §3.2: тип проверяется **до** вызова.
- **Слот 19 занят и у числового, и у векторного, с разной сигнатурой**:
  `SetFloat(float)` против `SetFloat(int, float)`. Позвать числовой сеттер на векторном канале —
  это не «записать не туда», а **рассогласование стека**: аргументов на один меньше, чем callee
  снимет. Открытый вопрос §10 про это был прав, и теперь он подтверждён точным дампом, а не
  подозрением.

`GetVector` возвращает 12-байтную структуру по значению, то есть на x86 MSVC — через скрытый
указатель на приёмник первым стековым аргументом, ровно как `GetChannelType` (§3.2). В плагине это
`tw::lua::channels::get_vector`; записи в векторные каналы нет намеренно.

Зачем это понадобилось: живая палитра цветов блоков лежит именно в `Value Vector`-каналах
(`XX_StartHere`, `ChannelSwitch #1535` — см. `reversing-journal-gameplay.md` §3.6), и без этого ABI
скрипт не может покрасить свой UI в цвета, которые игрок реально видит на трассе.

### 2.3 `Aco_Lua` — слоты 24+

Подтверждает и уточняет §2 `reversing-journal-lua.md` (там слоты были угаданы по декомпиляции,
здесь сняты с vtable):

| Слот | Смещение | Метод |
|---|---|---|
| 24 | `+0x60` | `Luaopen_Quest3DLIB` |
| 25 | `+0x64` | `DoTestFunction` |
| 26 | `+0x68` | `RunScriptCall` |
| 27 | `+0x6c` | `RunScriptValue` |
| 28 | `+0x70` | `GetError` |
| 29 | `+0x74` | `GetScript` |
| 30 | `+0x78` | `SetScript` |
| 31 | `+0x7c` | `OpenNewLua` |
| 32 | `+0x80` | `GetTickCount` |
| 33 | `+0x84` | `GetWindowSize` |

Ключевое наблюдение: **`Aco_Lua` наследуется от `Aco_FloatChannel`**, а не от `A3d_Channel`
напрямую. Поэтому Lua-канал в графе — это полноценный `Value`: его можно и «вызвать» как действие
(слот 1 → `RunScriptCall` → Lua-функция `CallChannel`), и «прочитать» как число (слот 17 →
`RunScriptValue` → `GetValue`). Ровно эта двойственность и делает Lua-канал бесшовным узлом графа —
и ровно её должен воспроизвести наш собственный скриптовый канал, если мы захотим его встроить.

### 2.4 Одна vtable на тип, одна vptr на объект

Наследование у каналов **одиночное** по всей цепочке (`DllInterface` → `A3d_Channel` →
`Aco_*Channel`), множественного нет нигде из просмотренного. Следствия, важные практически:

- у объекта ровно **одна** vptr, по смещению `+0x00`;
- деструктор `A3d_Channel` пишет `*(void***)this = &vftable` — прямое подтверждение;
- объекты создаются через `operator new` (`InitDLL` в каждой channel-DLL), то есть vptr лежит в
  **куче и записываема**, а сама vtable — в `.rdata` и защищена от записи.

Это и есть техническая база для точечного перехвата: копируем vtable типа в свою память, правим
нужные слоты, подменяем vptr **одного объекта**. Каналы, которых мы не трогали, не платят ничего.

### 2.5 Два способа позвать метод канала — и когда какой верен

Виртуальные методы каналов **ещё и экспортируются по имени** из своих DLL
(`?GetFloat@Aco_FloatChannel@@UAEMXZ` и т.д.). Значит, звать их можно двумя путями, и это не
стилистический выбор:

| | Через vtable объекта (`chan->vt[+0x44]()`) | Через экспорт DLL (`GetProcAddress` + явный `this`) |
|---|---|---|
| Что вызовется | **реализация фактического типа** канала | **именно та**, чьё имя разрешили |
| Когда верно | тип канала заранее неизвестен | тип канала точно известен |
| Цена | одна косвенность | прямой вызов |

Разница содержательная. Позвать экспортированный `Aco_FloatChannel::GetFloat` на канале типа
`Expression Value` или `Lua Script` — значит выполнить **базовую** логику («вернуть ребёнка 0»)
вместо формулы или скрипта: не падение, а тихо неверное число. Наоборот, `Aco_Array_Value::GetFloat`
по экспорту корректен ровно потому, что вызывающий уже знает, что перед ним `Array Value`.

Практика показывает оба подхода рабочими: инструменты, разрешающие функции по декорированному имени
из конкретной channel-DLL и подставляющие `this` вручную, работают (референс —
`Temp/Other Source Examples/MemorySurf - Old and shitty tool`; ту же технику использует
`framework/texture_hook.cxx`). **Правило выбора:** знаешь тип — можно по экспорту; принимаешь
произвольный канал от пользователя — только через vtable.

---

## 3. Раскладка объекта `A3d_Channel`

Восстановлена пересчётом заголовков SDK и проверена по коду: каждое смещение ниже реально
используется в декомпилированных функциях именно так.

| Смещение | Поле | Чем подтверждено |
|---|---|---|
| `+0x00` | vptr | `~A3d_Channel`: `*(void***)this = &vftable` |
| `+0x04` | `dllInstance_` | SDK `DllInterface` |
| `+0x08` | **`engine`** (`EngineInterface*`) | `GetChild`: `*(int*)(this+8)`; уже используется плагином |
| `+0x0c` | `channelTypeP_` | порядок SDK |
| `+0x10` | `channelCalculatedAtCount_` | `CheckRenderCount` читает и пишет именно его |
| `+0x14` | `channelIDIndexNr_` | порядок SDK |
| `+0x18` | `interfaceType_` | `GetChild`: `switch(*(int*)(child+0x18))` по значениям `ACO_*` |
| `+0x1c` | `publicCallerInfo_` | порядок SDK |
| `+0x20` | `parameterInfo_` | порядок SDK |
| `+0x24` | `inputChannelsList_` (`A3d_List`, 16 байт) | `GetChild`: `A3d_List::GetItem((A3d_List*)(this+0x24), nr)` |
| `+0x34` | `childCreationList_` | деструктор чистит `(A3d_List*)(this+0x34)` |
| `+0x44` | `linkRequest_` | порядок SDK |
| `+0x48` | `requestList_` | порядок SDK |
| `+0x4c` | `channelInstances_` | порядок SDK |
| **`+0x50`** | **`channelName_`** (`char*`) | `GetChannelName` = **`8b 41 50 c3`** = `mov eax,[ecx+0x50]; ret` |
| `+0x54` | `ourChannelGroup_` | `CheckRenderCount`, `GetChild` (ветка параметра) |
| `+0x58` | `noGroupInterfaceCreated_` | порядок SDK |
| `+0x5c` | `tickCount_` | порядок SDK |
| `+0x60` | `ingoreTreeCountState_` | `CheckRenderCount`: ранний выход при ненуле |
| `+0x64` | `timeStampValue_` | `GetTimeStamp` = `return *(DWORD*)(this+0x64)` |
| `+0x68` | `startedDynamicLoading_` | порядок SDK |
| `+0x69` | `treeCountMode_` | порядок SDK |
| `+0x6a` | `recalculateCount_` | `CheckRenderCount`: `this[0x6a]++` / `= 0` |
| `+0x6c` | `publicInterface_` | порядок SDK |
| `+0x70` | `publicTranslationList_` | порядок SDK |

`A3d_List` — 16 байт (vptr, `list_`, `listLength_`, `realListLength_`), и именно этот размер сводит
цепочку так, что `channelName_` попадает на `+0x50`. Совпадение с однобайтовым `GetChannelName` —
независимое подтверждение всей раскладки разом.

### 3.1 Ловушка: SDK-заголовок расходится с бинарём у производных классов

По расчёту из заголовка `sizeof(A3d_Channel)` = `0x74`, значит `Aco_FloatChannel::channelFloat_`
должен лежать на `+0x74`. В бинаре он на **`+0x7c`**:

```c
// Aco_FloatChannel::GetFloat, RVA 0x1110
child = GetChild(this, 0);
if (CheckRenderCount(this) && child) {
    *(float*)(this + 0x7c) = child->vtable[+0x44]();   // GetFloat ребёнка
}
return *(float*)(this + 0x7c);
```

при том что `InitDLL` этой DLL делает `operator new(0x88)`, а `GetFeedbackClass` в vtable нет.

**Вывод, обязательный к соблюдению в плагине:** заголовки Quest3D SDK описывают более позднюю
ревизию движка. Смещениям **до `+0x70`** верить можно (сверено выше), а вот приводить указатель на
канал к `Aco_FloatChannel*` / `Aco_StringChannel*` из SDK и читать их поля **нельзя** — промах на
8 байт, без всякого падения, просто мусор. Числа и строки брать только через vtable
(`+0x44`/`+0x4c`) либо по явно проверенным константам смещений.

Проверенные константы для шипнутой сборки:

```
0x08  engine                0x50  channelName_          0x54  ourChannelGroup_
0x7c  channelFloat_         (Aco_FloatChannel и все наследники, включая Aco_Lua)
0x80  default_              0x88  sizeof(Aco_FloatChannel)
Aco_Lua: 0x88 lua_State*    0x8c A3d_String error       0xa8 флаг «ошибку уже показали»
```

### 3.2 Тип канала в рантайме: `ChannelType` и почему это вопрос безопасности

Знать тип канала — не удобство, а необходимость: **слот 17 (`+0x44`) существует только у наследников
`Aco_FloatChannel`**. Это первый слот за 17-слотовой базой, поэтому его смысл целиком задаёт
производный тип: у `Text` там `GetString`, а у типа, не добавившего ни одного своего виртуального
метода, там **конец таблицы** — чтение соседних байт `.rdata` и переход по ним как по адресу
функции. Вызвать `+0x44` вслепую — это не «получить неверное число», это выполнить мусор.

`A3d_Channel::GetChannelType` (слот 4, RVA `0xc130`) отвечает на вопрос и есть у **всех** каналов:

```c
// public: virtual class ChannelType __thiscall A3d_Channel::GetChannelType(void)
if (this->channelTypeP_ /* +0x0c */ == 0) {
    this->channelTypeP_ = new ChannelType(0x84);      // sizeof(ChannelType) == 132
    ... заполняется из EngineInterfaceExt::GetDLLType ...
}
<копирование 0x21 dword'ов в скрытый буфер-приёмник>
```

Отсюда три факта:

- **`sizeof(ChannelType) == 0x84`**, копируется 33 dword'а — сходится с заголовком SDK
  (`char name[80]; GUID guid; GUID baseguid; int×3; DWORD; int`).
- Раскладка: `name` на `+0x00`, **`guid` на `+0x50`, `baseguid` на `+0x60`**.
- **`channelTypeP_` (`+0x0c`) — это кэш**, который движок наполняет при первом обращении. Если он
  ненулевой, тип читается прямым разыменованием, вообще без вызова.
- Возврат структуры по значению на x86 MSVC — скрытый указатель на приёмник **первым стековым
  аргументом** (в декомпиляции он виден как `in_stack_00000004`), то есть
  `ChannelType* __fastcall(self, edx, ChannelType* out)`.

**Дискриминатор.** `baseguid` — это GUID базового типа, и он чисто разбивает все 226 типов из
`channels.lst` на семейства:

| `baseguid` | Семейство | Типов |
|---|---|---|
| `BE69CCC4-…` (`Value`) | **числовые — есть `GetFloat`/`SetFloat`** | **30** |
| `6E6FB247-…` (`Text`) | строковые | 19 |
| `21A8923D-…` (`3D ObjectData`) | геометрия | 15 |
| `2F605354-…` (`Matrix`) | матрицы | 8 |
| `BC052C38-…` (`Texture`) | текстуры | 6 |
| `9D045960-…` (`Value Vector`) | векторы | 4 |

Тридцать числовых — это `Value`, `Expression Value`, `ValueOperator`, `Set Value`, `Trigger`,
`Envelope`, `SysInfo`, `UserInput`, `Inertia`, `TickCount`, `Array Value`, `Lua Script`,
`FiniteStateMachine`, `Selector`, `Mersenne_Twister` и ещё пятнадцать. У `Value` `baseguid`
совпадает с собственным `guid`, так что проверка `baseguid == FLOAT_CHANNEL_GUID` покрывает и его.

**Правило:** прежде чем вызвать `+0x44` или `+0x4c` на канале, чей тип не гарантирован конструкцией,
— проверить `baseguid`. Один раз, на этапе резолва; на горячем пути этого быть не должно.

---

## 4. Как исполняется кадр

### 4.1 Message pump — один поток

`QuestViewer.exe`, главный цикл (RVA `~0x401c00`):

```c
for (;;) {
    if (engine->vtable[+0x24]())  ok = PeekMessageA(&msg, 0, 0, 0, PM_REMOVE);
    else                          ok = GetMessageA(&msg, 0, 0, 0);   // свёрнуто / неактивно

    if (ok) { TranslateMessage(&msg); DispatchMessageA(&msg); }
    else if (!<paused> && <should tick>) engine->vtable[+0x0c]();     // апдейт кадра
}
```

Классический игровой pump: **графом занимается тот же поток, который качает очередь сообщений**.
Отдельного «логического» потока нет — `A3d_RunTread` в ядре существует, но кадр едет здесь.

**Практический вывод:** код графа, `WndProc` игры и (следовательно) `EndScene` — один и тот же
поток. Всё, что плагин делает из хука на канал, из хука на `EndScene` и из `wndproc_hub`, взаимно
синхронизировано без единого мьютекса. Синхронизация нужна **только** против IPC-потока
(`overlay_ipc`), ровно как уже устроено в `overlay_state`.

### 4.2 Точка входа в граф группы

```c
// A3d_ChannelGroup::CallStartChannel, RVA 0xa860 — экспортируется по имени
void A3d_ChannelGroup::CallStartChannel() {
    if (this->treeCount /* +0x54 */ < 30000) this->treeCount++;
    else                                     this->treeCount = 0;

    int idx = (this->startChannel /* +0x38 */ == -1) ? 0 : this->startChannel;
    A3d_Channel* start = this->vtable[+0x48](idx);      // GetChannel(int)
    if (start) start->vtable[+0x04]();                  // CallChannel
}
```

Ровно **один вызов на группу за кадр**, с готовым `A3d_ChannelGroup*` в `ecx`. Это самая дешёвая и
самая устойчивая точка для покадровых зацепок: детур сюда стоит один переход на группу, а не на
канал. Имя группы берётся тут же — `A3d_ChannelGroup::GetChannelGroupFileName` (RVA `0x95d0`).

Счётчик кольцевой (обнуление на 30000) — сравнивать tree count'ы на «больше/меньше» нельзя, только
на равенство.

### 4.3 Мемоизация: `CheckRenderCount`

```c
// A3d_Channel::CheckRenderCount, RVA 0xaf20 (protected, но экспортируется)
bool A3d_Channel::CheckRenderCount() {
    if (this->ignoreTreeCount /* +0x60, из CHIC */) { this->recalcCount = 0; return true; }

    int now = group ? group->GetTreeCalculateCount() : engine->GetTreeCalculateCount();
    if (this->calculatedAt /* +0x10 */ != now) {
        this->calculatedAt = now;
        this->recalcCount  = 0;
        return true;            // первый раз за кадр — считай
    }
    this->recalcCount++;
    return false;               // уже считали — отдай кэш
}
```

Это **центральный механизм производительности всего движка**. Узел, к которому за кадр обратились
двадцать раз, считается один раз; остальные девятнадцать — предикат плюс возврат поля. Отсюда же
следует, что граф — DAG с разделяемыми поддеревьями, а не дерево, и что цена «прочитать канал»
асимметрична: первый раз за кадр дорого, дальше почти бесплатно.

Три варианта поведения, которые стоит держать в голове:

- `Value`, `ChannelCaller` — гасятся `CheckRenderCount`, исполняются раз за кадр;
- `Set Value` — **не** зовёт `CheckRenderCount` вовсе, отрабатывает на каждый вызов;
- каналы с `ingoreTreeCountState_ != 0` — тоже всегда, см. §4.4.

### 4.4 Мемоизация отключается **на канал**, флагом из файла — и это массово

Ключ, без которого §4.3 вводит в заблуждение. `A3d_Channel::LoadChannel` (RVA `0xcb00`) читает из
`.cgr` два поля, прямо управляющих мемоизацией:

```c
if (GetInfoIfTag("CHIC")) CopyTagDataToPointer(this + 0x60);   // -> ingoreTreeCountState_
...
if (GetInfoIfTag("CHTM")) CopyTagDataToPointer(this + 0x69);   // -> treeCountMode_
else                      this[0x69] = 1;                       // default TREE_INCREASEPARAMETER
```

То есть **`CHIC` — это не «не разбиралось», это и есть флаг «игнорировать tree count»**, автором
проекта выставляемый прямо в редакторе, поканально. `CHIC = 1` → `CheckRenderCount` всегда
возвращает `true` → **канал не мемоизируется вообще и пересчитывается на каждое обращение**.
(Это отменяет запись «`CHIC`/`CHIT`/`CHTM` не разбирались» из §9 `reversing-journal-gameplay.md`;
`CHTM` там же — `treeCountMode_`, встречается 10 569 раз, почти всегда `1`.)

Насколько это массово — по всему проекту (131 763 канала с полем `CHIC`):

| | Каналов | Доля |
|---|---|---|
| `CHIC = 0` — мемоизируются | 119 337 | 90.6 % |
| `CHIC = 1` — **пересчитываются всегда** | 12 426 | **9.4 %** |

И распределение крайне неравномерное — флаг ставят там, где мемоизация ломала бы логику:

| Тип | С флагом | Всего | Доля |
|---|---|---|---|
| **`Array Value`** | **400** | 431 | **92.8 %** |
| `Array Text` | 124 | 148 | 83.8 % |
| `VectorOperator` | 434 | 1007 | 43.1 % |
| `ValueOperator` | 394 | 1057 | 37.3 % |
| `Expression Value` | 2410 | 8277 | 29.1 % |
| `IfElse` / `If` | 328 / 694 | 1202 / 2687 | ~26 % |
| `ChannelCaller` | 1566 | 8223 | 19.0 % |
| `Value` | 808 | 46 918 | 1.7 % |

`Array Value` — почти поголовно. Логично: канал-курсор, который мемоизирует, бесполезен для
`ForLoop` — а именно циклом игра и читает свои таблицы (`Do_CalculateFinalStats`,
`for c in 0..5`). Вывод для нас в §5.

---

## 5. Семантика узлов на уровне машинного кода

Четыре типа, которых достаточно, чтобы понять весь граф. Все — из соответствующих
`engine/channels/<GUID>.dll`.

**`Value` (`Aco_FloatChannel`, 46 918 экземпляров) — путь данных, pull:**

```c
CallChannel() { this->vtable[+0x44](); }               // «вызвать Value» == «вычислить его»
GetFloat()    { child0 = GetChild(0);
                if (CheckRenderCount() && child0) cache = child0->vtable[+0x44]();
                return cache; }                        // cache == *(float*)(this+0x7c)
SetFloat(v)   { *(float*)(this+0x7c) = v; }            // просто store, без побочных эффектов
```

**`ChannelCaller` (`Aco_QueueChannel`, 8 223) — путь действия, push:**

```c
CallChannel() { if (!CheckRenderCount()) return;
                n = GetChildLinkPositionCount(0);
                for (i = 0; i < n; i++)
                    if (c = GetChildFromLinkPosition(0, i)) c->vtable[+0x04](); }
```

Это и есть «оператор `;`» языка графа — и именно `ChannelCaller`'ами являются **все** хендлеры
`Do_*` из `reversing-journal-gameplay.md`.

**`Set Value` (`Aco_SetFloat`, 6 511) — путь записи:**

```c
CallChannel() { src = GetChild(0);                       // порт 0 — источник
                if (src) src->vtable[+0x44]();
                n = GetChildLinkPositionCount(1);         // порт 1 — цели
                for (i = 0; i < n; i++)
                    if (t = GetChildFromLinkPosition(1, i))
                        t->vtable[+0x4c](src ? src->vtable[+0x44]()
                                             : *(float*)(this+0x7c)); }
```

Обратите внимание: у источника `GetFloat` дёргается **дважды** — один раз «вхолостую» до цикла и по
разу на каждую цель. Благодаря `CheckRenderCount` это дёшево, но факт полезный: чтение канала в
движке ничего не стоит именно потому, что оно мемоизировано.

**`Array Value` (`Aco_Array_Value`, 431) — чтение ячейки таблицы через канал-курсор:**

```c
// Aco_Array_Value::GetFloat, RVA 0x12c0
GetFloat() {
    if (!CheckRenderCount()) return cache;               // <-- мемоизация действует и здесь!
    binder = *(void**)(this + 0xb0);                     // объект привязки к Array Table
    if (!binder->vt[0]()) { binder->vt[4](); if (!binder->vt[0]()) return cache; }
    if (idx = GetChild(0))                               // порт 0 — канал-ИНДЕКСАТОР
        *(int*)(this + 0x90) = (int)idx->vt[+0x44]();    // float -> int, усечением
    if (*(int*)(this + 0x88)) {
        cell = binder->vt[+0x50](*(int*)(this + 0x90));  // GetContent(row)
        if (cell) { cache = *cell; return cache; }
        cache = 0;                                       // null -> ноль, а не падение
    }
    return cache;                                        // cache == *(float*)(this + 0x7c)
}
```

Отсюда рабочая идиома «прочитать `Stats.CollectedColorCounts[i]`», подтверждённая практикой
(референс — `Temp/Other Source Examples/MemorySurf - Old and shitty tool`):

```
1. запомнить текущее значение канала-индексатора  (GetFloat)
2. записать нужный индекс                          (SetFloat)
3. прочитать Array Value                           (GetFloat)
4. вернуть индексатор на исходное значение         (SetFloat)
```

Шаг 4 обязателен: курсор общий с игрой, и оставленный сдвинутым индекс — это порча чужой логики,
а не только своей. Заодно это и ответ на оговорку про гонку в §7.3
`reversing-journal-gameplay.md`: на потоке движка (а другого и нет, §4.1) последовательность
атомарна по построению, и восстановление возвращает мир в исходное состояние.

**Про мемоизацию здесь — тревога ложная, и это стоит объяснить, а не просто снять.** По коду выше
`GetFloat` гейтится `CheckRenderCount`, из чего следовало бы, что второе за кадр чтение того же
`Array Value` вернёт кэш и наш новый индекс проигнорирует — то есть цикл «шесть цветов за один
кадр» отдал бы шесть копий первого значения.

Этого не происходит, потому что **у `Array Value` практически всегда выставлен `CHIC = 1`** (§4.4):
400 каналов из 431 по всему проекту, и оба канала, которые читал MemorySurf, — тоже:

```
StatCollector.cgr  #3  CHIC=1  Array Value  "Stats: TrafficColorCounts"
StatCollector.cgr  #8  CHIC=1  Array Value  "Stats: CollectedColorCounts"
#4/#16 (Index_*)   CHIC=0  Value            каналы-индексаторы, обычные
```

`CHIC = 1` → `ingoreTreeCountState_ != 0` → `CheckRenderCount` возвращает `true` безусловно →
**канал не мемоизируется вообще**. Каждое обращение к `Array Value` честно перечитывает индекс и
лезет в таблицу.

Иначе и быть не могло: игра читает свои таблицы `ForLoop`'ом (`Do_CalculateFinalStats`,
`for c in 0..5`), а `ForLoop` (RVA `0x10c0`) не трогает tree count — он просто пишет индекс в
ребёнка 1 через `SetFloat` и зовёт тело. Мемоизирующий курсор сломал бы **саму игру**, а не только
внешнего читателя.

Подтверждено и практикой: `NecromancyEngine::dump()` в MemorySurf снимает шесть индексов по двум
колонкам в одном кадре подряд и отдаёт наружу — шесть одинаковых чисел заметил бы кто угодно.

Единственный реальный подводный камень остаётся один:

- **Индекс — `float`, усекаемый в `int`, и границы никто не проверяет.** Ноль-указатель `GetContent`
  здесь обработан, но за это отвечает объект колонки, а не сам канал. Выход за границы —
  реалистичный способ уронить игру, и это единственная известная острая грань записи в каналы
  (`lua-scripting.md` §8.1).

**Осторожность, которая из этого следует:** полагаться на «`Array Value` не мемоизируется» вслепую
нельзя — 31 канал из 431 идёт с `CHIC = 0`. Флаг читается из файла, значит правильный способ —
проверять `*(uint8_t*)(chan + 0x60)` в рантайме и сбивать мемо (запись в `channelCalculatedAtCount_`,
`+0x10`) только для тех каналов, где он нулевой.

**`Lua Script` (`Aco_Lua`, 138)** — см. §2.3 и `reversing-journal-lua.md`. Одно уточнение к тому
документу: `RunScriptCall` делает `lua_pushstring(L, "CallChannel"); lua_gettable(L, -10001)`, а
`-10001` в Lua 5.0 — это **`LUA_GLOBALSINDEX`**, не `LUA_REGISTRYINDEX` (`-10000`). То есть точка
входа ищется в глобалах, а в реестр (`-10000`) кладётся указатель на сам канал — как журнал и
описывал. Существенно другое: **поиск имени функции делается заново на каждый вызов** — строка плюс
хеш-лукап в таблице глобалов, каждый кадр, на каждый Lua-канал. Это тот уровень накладных расходов,
который игра уже терпит; полезный ориентир для бюджета в `lua-scripting.md`.

---

## 6. Цена операций (для тех, кто будет писать hot-path)

| Операция | Что происходит на самом деле | Вердикт |
|---|---|---|
| `A3d_Channel::GetChannelName` | `mov eax,[ecx+0x50]; ret` | бесплатно |
| `A3d_Channel::GetTimeStamp` | одно поле | бесплатно |
| `chan->vtable[+0x44]()` (`GetFloat`) | косвенный вызов + `CheckRenderCount` | дёшево |
| `A3d_Channel::GetChild(i)` | `engine[+0x40]->GetIfTrackChannelCalling()` (байт), `A3d_List::GetItem`, `switch` по `interfaceType_` | приемлемо, но не в цикле |
| **`A3d_ChannelGroup::GetChannel(const char*)`** | **линейный скан с `_stricmp` по всем каналам группы, с перечитыванием `GetItemCount()` на каждой итерации** | **~7000 `_stricmp` на один поиск в `XX_StartHere`. Только на этапе резолва, никогда покадрово** |
| `EngineInterface::GetChannelGroup(const char*, int)` | тоже линейный скан по списку групп | то же |

`GetIfTrackChannelCalling` (RVA `0x9970`) — `return this[4]`, флаг профилировщика редактора; в
шипнутом вьювере он выключен, поэтому `GetChild` идёт по короткой ветке.

Отдельно: `EngineInterface` — `+0x40` → `EngineInterfaceExt*`, `+0x44` → внутренний объект-владелец
списка групп и потока исполнения, `+0x48` → `A3d_List` слушателей (§8). Все три подтверждены
использованием; точная идентичность объекта на `+0x44` не устанавливалась.

---

## 7. Отмена §7.2 `reversing-journal-gameplay.md`

Тот раздел утверждает: «`TweakerPlugin` **уже** хукает `A3d_Channel::CallChannel` … Этого хука
достаточно для полного событийного потока: фильтровать по `GetChannelName(self)` и реагировать на
нужные имена». **Это неверно**, и вот почему.

**Факт 1. Символ, на который стоит детур, — пустышка.**

```
?CallChannel@A3d_Channel@@UAEXXZ   RVA 0x23f0:  c3 cc cc cc cc cc cc cc ...
```

Один байт `c3` (`ret`), дальше `int3`-паддинг. Тот же адрес COMDAT'ом делят ещё четыре пустых
метода: `??1MemoryDump@@QAE@XZ`, `?OneTimeInitialize@A3d_Channel@@UAEXXZ`,
`?SortChannelList@ChannelList@@UAEXXZ`, `?TreadFunction@A3d_RunTread@@UAEXXZ`.

**Факт 2. Это виртуальный метод, и его переопределяют почти все.** Пересчёт по всему проекту
(226 типов каналов против 161 группы, 131 282 канала с разрешимым типом):

| | Типов каналов | Экземпляров каналов |
|---|---|---|
| переопределяют `CallChannel` | **159** | **108 627** |
| не переопределяют (доходят до заглушки) | 67 | 22 655 |

То есть **82.7 %** каналов до детура не доезжают вообще. И это не случайные 82.7 %: переопределяют
именно исполняемые типы — `ChannelCaller`, `Set Value`, `If`, `IfElse`, `ForLoop`, `ChannelSwitch`,
`CallSelected`, `Trigger`, `Expression Value`, `Lua Script`, `Value`. Все до единого хендлеры
`Do_UpdatePoints` / `Do_ReportCarCollected` / `Do_CalculateFinalStats` — это `ChannelCaller`, и ни
один из них через базовую заглушку не проходит.

До заглушки доезжают, наоборот, чисто **данные**: `Envelope` (8880), `Text` (4747), `Motion`
(2468), `Texture` (2058), `SysInfo` (1086), `VectorOperator` (1007), `UserInput` (710),
`Array Value` (431)… У этих «вызов канала» не значит ничего.

**Факт 3. Хук при этом делает ровно то, ради чего написан.** В `framework/channel_hook.cxx` он
нужен только чтобы поймать `EngineInterface*`, и для этого годится: во-первых, данные-каналы его
всё же дёргают, во-вторых, на том же адресе висит `OneTimeInitialize`, который движок зовёт каждому
каналу при загрузке группы. **Код плагина не сломан; сломана была бы надстройка**, которую §7.2
предлагала на нём построить.

**Факт 4 — детур на однобайтовую функцию ставится и работает.** Проверено практикой: Detours
собирает трамплин из `ret` плюс следующий за ним `int3`-паддинг, и на этой механике работали
инструменты вне этого репозитория. Из статики этого было не видно, но вопрос закрыт эмпирически.

У захвата `EngineInterface*` через этот хук есть одна особенность, и она **не** дефект: он
**отложенный**. Сработать хук может только когда движок дёрнет канал, не переопределяющий ни
`CallChannel`, ни `OneTimeInitialize`, — а до того указатель остаётся нулевым. На практике это
значит, что игроку может понадобиться что-нибудь кликнуть в меню, прежде чем плагин узнает адрес
движка. Зато **после первого захвата указатель валиден до конца жизненного цикла игры**: движок
один на процесс и не пересоздаётся.

**Факт 5, стоимость — и как её снять.** Пока хук стоит, каждый из 22 655 «данных»-каналов на каждый
свой вызов платит переход на трамплин плюс `nullptr`-проверку. После первого захвата это чистые
накладные расходы навсегда.

Из факта 4 следует и лекарство: раз указатель нужен **один раз** и потом живёт вечно, детур можно
**снять сразу после захвата** (`DetourDetach` из того же `detour_transaction`, на холодном пути).
Свойство «отложенного захвата» при этом сохраняется полностью — хук просто стоит ровно столько,
сколько нужно, — а на hot-path не остаётся ничего. Это стоит сделать в `channel_hook.cxx`
независимо от скриптового слоя.

**Что делать вместо этого** — §3 `lua-scripting.md`: `EngineInterface*` брать из того же хука
(или из `Aco_DX8_D3DDeviceUse`, у которого он тоже есть), а события ловить **подменой vtable у
конкретных объектов**, разрешённых по имени один раз. Тогда цена события пропорциональна числу
инструментированных каналов, а не общему числу вызовов в кадре.

---

## 8. Механизмы, которые в API есть, а в рантайме мертвы

**`EngineListener`.** SDK обещает уведомление об уничтожении канала:

```c
// EngineInterface::AboutToReleaseChannel, RVA 0x5e40 — не virtual, экспортируется по имени
void EngineInterface::AboutToReleaseChannel(A3d_Channel* ch) {
    A3d_List* listeners = (A3d_List*)(this + 0x48);
    for (i = 0; i < listeners->GetItemCount(); i++)
        if (l = listeners->GetItem(i)) l->vtable[+0x04](ch);   // OnAboutToReleaseChannel
}
```

Механизм рабочий, и в него **можно зарегистрироваться** (`AddListener`, RVA `0x5df0`, — просто
`AddItemOrNULL` в тот же список; наш объект-слушатель это две ячейки vtable: деструктор и
`OnAboutToReleaseChannel`). Проблема в другом: **никто его не дёргает**. `AboutToReleaseChannel` не
имеет ни одного вызова внутри `HighPoly.dll`; поиск по всем `.dll`/`.exe` в `engine/` нашёл ссылку
ровно в одном модуле — `2346A6DF-…` (`Array Unique`). `~A3d_Channel` его не зовёт. `AddListener` не
импортирует вообще никто. Похоже, механизм существует для редактора Quest3D, а не для вьювера.

**Вывод:** как средство инвалидации закешированных `A3d_Channel*` `EngineListener` не годится.
Инвалидировать придётся по выгрузке группы — `A3d_ChannelGroup::Release` (виртуальный,
экспортируется) или `EngineInterface::DeleteChannelGroup(int)`. Это редкие, холодные события.

**Событийная шина канала.** `EngineInterface::AddChannelToEvent(channel, eventNr)` /
`RemoveChannelFromEvent` / `SetCurrentEvent` существуют и экспортируются; как игра ими пользуется —
не смотрел.

---

## 9. Что это даёт плагину — сводка адресов

Всё ниже — **экспорт по имени**, то есть берётся через `DetourFindFunction` / `GetProcAddress` без
сканирования сигнатур, ровно как уже сделано в `framework/texture_hook.cxx`.

```
HighPoly.dll
  ?CallStartChannel@A3d_ChannelGroup@@UAEXXZ                       покадровая точка входа в группу
  ?GetChannelGroupFileName@A3d_ChannelGroup@@UAEPBDXZ              имя группы
  ?GetChannel@A3d_ChannelGroup@@UAEPAVA3d_Channel@@PBD@Z           резолв канала по имени (дорого!)
  ?GetChannel@A3d_ChannelGroup@@UAEPAVA3d_Channel@@H@Z             резолв по индексу (дёшево)
  ?GetChannelCount@A3d_ChannelGroup@@UAEHXZ
  ?GetParameterChannel@A3d_ChannelGroup@@UAEPAVA3d_Channel@@H@Z    параметры группы по ordinal
  ?Release@A3d_ChannelGroup@@UAEXXZ                                инвалидация хендлов
  ?GetChannelGroup@EngineInterface@@UAEPAVA3d_ChannelGroup@@PBDH@Z резолв группы по имени
  ?GetTreeCalculateCount@EngineInterface@@UAEHXZ                   глобальный счётчик кадров графа
  ?AddListener@EngineInterface@@QAEXPAVEngineListener@@@Z          (мёртв, см. §8)
  ?GetChannelName@A3d_Channel@@QAEPBDXZ                            один mov
  ?GetChild@A3d_Channel@@QAEPAV1@H@Z
  ?GetChildCount@A3d_Channel@@QAEHXZ
  ?GetChildFromLinkPosition@A3d_Channel@@QAEPAV1@HH@Z              доступ по портам (CHUL / CHRP)
  ?GetChildLinkPositionCount@A3d_Channel@@QAEHH@Z
  ?GetChannelGroup@A3d_Channel@@QAEPAVA3d_ChannelGroup@@XZ

Слоты vtable (и для перехвата, и для вызова)
  +0x04  CallChannel      +0x44  GetFloat      +0x4c  SetFloat
```

---

## 10. Открытые вопросы

- **Где ещё есть «курсорные» каналы.** `Array Value` — точно; `Array Text`, `Array Vector`,
  `Array Matrix` устроены, предположительно, так же (порт 0 — индексатор), но не смотрелись.
  Косвенный аргумент за: у `Array Text` `CHIC = 1` в 124 случаях из 148 — тот же профиль, что у
  `Array Value` (§4.4).
- **Сбив мемоизации записью в `channelCalculatedAtCount_` (`+0x10`)** — выведен из декомпиляции
  `CheckRenderCount` и **не проверен**. Нужен только для тех каналов, у которых `CHIC = 0`; для
  подавляющего большинства курсорных каналов он не понадобится вовсе.
- **Кто и как часто инкрементит глобальный tree count.** `A3d_ChannelGroup::GetTreeCalculateCount`
  возвращает **сумму** глобального счётчика движка и собственного счётчика группы (`+0x54`), а
  `EngineInterfaceExt::IncreaseTreeCount` бампает глобальный **и** проходит по списку групп. Кто и
  сколько раз за кадр это зовёт — не выяснялось. На выводы §4.3/§4.4 не влияет (там всё решает
  `CHIC`), но для точного понимания «что такое кадр с точки зрения мемоизации» это дыра.
- **`CHIT`** (`u32`, идёт сразу после `CHNA`) по-прежнему не разобран — в отличие от `CHIC` и
  `CHTM`, которые §4.4 закрыл.
- ~~**Строковый ABI**~~ — закрыто, см. §2.2.1. Слоты подтверждены; смещение `channelString_` так и
  не понадобилось — всё читается через vtable.
- ~~**Векторный ABI**~~ — закрыто, см. §2.2.2. Подозрение про `SetFloat(int, float)` подтвердилось
  дампом: у векторного канала это слот 19, ровно там же, где у числового `SetFloat(float)`.
  **Перед любым `SetFloat` тип канала обязан быть проверен** — правило остаётся в силе, теперь с
  доказательством.
- **Матричный ABI** (`Matrix`, 359 экземпляров) по-прежнему не смотрел. Способ известен и дешёв —
  см. §11, «Дамп vtable по экспортам»; делать по факту надобности.
- ~~**Как дёшево узнать тип канала в рантайме**~~ — закрыто, см. §3.2.
- **`operator new` каналов** идёт из CRT самой channel-DLL (VS2003, msvcr71). Если мы когда-нибудь
  захотим *создавать* каналы, освобождать их обязана та же DLL — для этого и существует
  `EngineInterface::InitChannelFromType`.
- **Порядок вызова групп за кадр** по-прежнему не выписан (перешло из
  `reversing-journal-gameplay.md` §9). Детур на `CallStartChannel` его немедленно покажет — это
  первое, что стоит выяснить при реализации.
- **Точная идентичность объекта на `EngineInterface+0x44`** (через него идут и группы, и поток
  исполнения графа) не устанавливалась.

---

## 11. Как это воспроизвести

Окружение — как в §6 `reversing-journal-lua.md` (переменные в системе не выставлены):

```bash
export JAVA_HOME="C:\\Program Files (x86)\\Android\\openjdk\\jdk-21.0.8"
export PATH="/c/Program Files (x86)/Android/openjdk/jdk-21.0.8/bin:$PATH"
export GHIDRA_OUTPUT_DIR='C:/.../Temp/Dumped/_work/ghidra_out/<имя>'

"C:/Users/Unchp/Ghidra/support/analyzeHeadless.bat" \
  'C:\...\Temp\Dumped\_work\ghidra_proj' ASChannels \
  -import 'C:/.../Audiosurf_Steam/engine/HighPoly.dll' \
  -scriptPath 'C:\Users\Unchp\.claude\skills\ghidra\scripts\ghidra_scripts' \
  -postScript ExportAll.java
```

Проект `ASChannels` переиспользуется между запусками. На момент этой записи в нём: `HighPoly.dll`,
`QuestViewer.exe`, `Aco_Lua`, `Aco_DynamicLuaLoading`, `Value`, `ChannelCaller`, `Set Value`,
`Expression Value`, `ChannelSwitch`, `If`, `Trigger`.

Две грабли, стоившие времени:

1. **Bash съедает `\\$` внутри двойных кавычек.** `"$DIR\\$name.dll"` разворачивается в
   `...\$name.dll` — литеральный `$name`, и Ghidra падает с «is not a valid directory or file».
   Для путей в командной строке Ghidra проще целиком перейти на прямые слэши: `analyzeHeadless.bat`
   их принимает.
2. **`-overwrite` на уже импортированном файле роняет запуск**, а `-process` без предварительного
   импорта — тем более. Повторный экспорт по уже импортированному модулю делается
   `-process '<имя>.dll' -noanalysis`.

Дампы vtable и таблиц экспортов снимались **не** Ghidra, а тридцатистрочным PE-ридером на чистом
Python (разбор `IMAGE_EXPORT_DIRECTORY` плюс `rva → file offset` по таблице секций). Причина та же,
что и в прошлых сессиях — Defender карантинит свежие неподписанные managed-бинари, — но и по
существу: для вопросов «какие функции экспортируются» и «что лежит в `??_7X@@6B@`» полный
автоанализ избыточен, а скрипт отвечает мгновенно и сразу на всех 242 DLL.

Разбор графа (гистограмма типов каналов, пересчёт переопределений `CallChannel`) — через `cgrpy`
из `Temp/Dumped/_work/`, см. §8 `reversing-journal-gameplay.md`.

### Дамп vtable канального типа по экспортам — рецепт на пять минут

Так сняты §2.2.1 и §2.2.2, и так же снимается любой оставшийся тип. Ghidra тут не нужна вообще:
channel-DLL экспортируют **и** саму vtable, **и** каждый виртуальный метод по декорированному
имени, поэтому таблица читается сопоставлением адресов.

1. Найти DLL. Имя файла — это GUID типа: семейство из таблицы §3.2 (`9D045960-…` — векторы,
   `6E6FB247-…` — текст) даёт `channels/<GUID>.dll` напрямую.
2. `dumpbin /exports <dll>` — оттуда RVA `??_7Aco_XChannel@@6B@` (это и есть vtable) и RVA каждого
   метода.
3. Прочитать `void*` подряд с этого RVA, вычесть image base (`0x10000000` у всех channel-DLL),
   сопоставить с RVA экспортов.

Две вещи, которые делают это надёжным:

- **Конец таблицы виден.** Сразу за последним слотом в `.rdata` лежит имя типа ASCII-строкой
  (`"Text"`, `"Value Vector"`), так что первое значение, не похожее на RVA внутри модуля, — это
  граница, а не догадка. Именно так установлено, что у векторного канала своих слотов ровно три.
- **Слоты 2–16 — переходники.** У производного типа они идут плотной пачкой с шагом 6 байт
  (`jmp` в базовую реализацию), что сразу отличает унаследованную часть от собственной: собственные
  слоты указывают в настоящий код, разбросанный по `.text`.

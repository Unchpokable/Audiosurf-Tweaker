# Полевой журнал аудита Lua-слоя и перехватов движка

Начат 24.10.2026. Ревью `TweakerPlugin/src/lua/`, `src/engine/`, `src/framework/channel_shim` и
смежного на предмет UB и тихой порчи состояния игры/Quest3D. Журнал ведётся по ходу работы: каждая
находка — со статусом, местом в коде, сценарием и способом проверки. Отчёт для владельца собирается
из него, а не наоборот.

**Статусы.** `ПОДТВЕРЖДЕНО` — воспроизведено (офлайн-харнессом или по бинарю); `ПО КОДУ` — следует
из чтения кода однозначно, но не прогнано; `ГИПОТЕЗА` — правдоподобно, требует проверки;
`СНЯТО` — проверено и не подтвердилось (оставляется, чтобы не проверять второй раз).

**Репро-код** — `TweakerPlugin/harness/audit/` (каталог `harness/` вне git, см. CLAUDE.md; поэтому
суть каждого репро продублирована здесь). Сборка — `harness\audit\build.bat` (из cmd: `.\build.bat`).

## Сводка

| # | Что | Где | Статус | Чем грозит |
|---|---|---|---|---|
| A1 | выключение скрипта снимает с канала одну его подписку из нескольких | `channel_shim.cxx: unsubscribe_all_of` | ПОДТВЕРЖДЕНО | висячий подписчик → подавление `Do_ResetStats`/`Do_ResetSimpleStats` (**баг PuzzlePRO**), UAF |
| A2 | отписка, пока канал исполняется через шим | `channel_shim.cxx: shim_call_channel` | ПОДТВЕРЖДЕНО | UAF `binding`, вызов `original_call` из освобождённой памяти |
| A3 | `runnable(owner < 0) == true` | `lua_sched.cxx` | ПО КОДУ | усилитель A1 |
| A4 | `float_ch:set`/`vector_ch:set` на табличной колонке пишет строку без проверки | `fam_number`/`fam_vector` | ПОДТВЕРЖДЕНО (IDA) | удлинение таблиц игры |
| A5 | чтение мемоизируемой колонки оставляет в мемо строку скрипта | `fam_table.cxx: read*` | ПО КОДУ | неверное значение у читателей игры до конца кадра (31 колонка) |
| A6 | `lua_pcall` изнутри FFI-вызова того же `lua_State` | `lua_sched.cxx: dispatch_call` | ГИПОТЕЗА | порча Lua-стека / PANIC |

---

## Известный баг: PuzzlePRO HUD ломает очки и генерацию трассы

Симптом от владельца: вкл/выкл скрипта `puzzlepro_hud` в редких случаях ломает начисление очков и
генерацию трассы, в том числе когда скрипт уже выключен; лечится только перезапуском игры.

**Ведущая гипотеза — A1** (ниже): после выключения скрипта на `StatCollector::Do_ResetStats` и
`Do_ResetSimpleStats` остаётся висячий подписчик, указывающий на освобождённую запись подписки. Как
только эта память достаётся новой записи-глушилке (`tw.mute`) или мусору нужной формы, шим **подавляет
вызов** `Do_ResetStats`/`Do_ResetSimpleStats`. А `Do_ResetStats` — это и есть генерация трассы
(`cgrpy query dec StatCollector Do_ResetStats`): `Do_InitTrack` (очистка `Stats.TrafficPattern`,
`TotalCarCount := 0`), посев обоих Mersenne Twister, `Do_NotATutorial` (генерация машин),
`Do_GenerateColorPointValues`, `Do_ResetScores`, `Do_CharacterAndLeagueTrafficMods`, `Do_CountColors`.
`Do_ResetSimpleStats` — обнуление очков заезда. StatCollector — статическая группа, живёт весь сеанс,
поэтому висячий подписчик тоже живёт до перезапуска.

---

## Находки

### A1. `channel_shim::unsubscribe_all_of` снимает с канала только одну подписку владельца — ПОДТВЕРЖДЕНО

`src/framework/channel_shim.cxx`, `unsubscribe_all_of`:

```cpp
for(std::size_t i = g_bindings.size(); i-- > 0;) {
    for(void* user : users) {
        if(detach(g_bindings[i], user, i)) {
            break;   // "at most one subscription per user" — верно для записи, НЕ для владельца
        }
    }
}
```

`users` — все записи одного скрипта (`api_hooks.cxx: unsubscribe_owner`). Если у скрипта на одном
канале **две и больше** подписок, `break` после первой снятой оставляет остальные. Затем
`unsubscribe_owner` удаляет **все** записи (`delete`), и в `binding::subscribers` остаётся
`subscriber{ dispatch_channel_call, user = <освобождённый subscription*> }`. Последствия:

1. vptr канала **не возвращается**, копия vtable живёт дальше (выключенный скрипт продолжает
   «стоить», `active_count()` > 0 без единого скрипта);
2. на каждом вызове канала `dispatch_channel_call` читает освобождённую запись (use-after-free):
   `record->owner`, `mute`, `enabled`, `after`, `id`;
3. **подавление вызова игры.** `dispatch_channel_call` вернёт `false` (= не звать оригинал), если
   по адресу висячей записи лежит что-то, где `owner` < 0 или номер включённого неприостановленного
   скрипта (`sched::runnable` для `owner < 0` отвечает `true`), байт `mute` ≠ 0 и байт `enabled` ≠ 0.
   Самый вероятный путь — повторное включение скрипта: он заново выделяет ~16 записей того же размера,
   LFH отдаёт освободённые блоки, и висячий указатель начинает **совпадать с новой записью**. Из 16
   записей PuzzlePRO 7 — глушилки (`mutes.lua`), т.е. порядка 40 % на каждый висячий указатель;
4. счётчик висячих растёт на 1 за цикл вкл/выкл на каждом таком канале: включение добавляет в
   существующий binding две новые записи, выключение снимает одну.

Кто попадает: любой скрипт с двумя подписками на один канал. PuzzlePRO — `Do_ResetStats` и
`Do_ResetSimpleStats` (`tally.lua: run_started` + `feed.lua: reset_score_feed`).

**Репро** (`harness/audit/shim_audit.cxx`, сценарий A1): две подписки разных `user` на один фейковый
канал, затем `unsubscribe_all_of({u1, u2, u3})` →
`subscribers left = 1 (expected 0)`, `vptr restored = NO`, `active bindings = 1`.

**Сквозное репро** (`harness/audit/toggle_audit.exe`, `/O2 /MD` — настоящая Release-куча CRT):
настоящий `channel_shim.cxx`; запись `subscription` и `dispatch_channel_call` — копии из
`api_hooks.cxx` (та же раскладка, `generation` = `uint32_t`, тот же порядок регистрации, что в
собранном `puzzlepro_hud.lua`: 7 глушилок, затем `on_call`). 50 циклов «включить → вызвать оба
обработчика → выключить → вызвать ещё раз»:

```
cycle 2: Do_ResetStats SUPPRESSED/runs, Do_ResetSimpleStats SUPPRESSED/runs (enabled/disabled), leftover subscribers 2/2
...      (то же на каждом следующем цикле)
after 50 cycles, script disabled: leftover subscribers on Do_ResetStats 50, on Do_ResetSimpleStats 50, hooked channels 2
```

То есть в чистой куче уже **со второго включения** оба обработчика подавляются на каждом вызове, пока
скрипт включён: освобождённые записи переиспользуются LIFO, и висячий указатель попадает ровно на
новую глушилку. В игре между выключением и включением кучу трогает ещё много кто (строки лога,
`scripts.cfg`, диагностика), поэтому попадание вероятностное — отсюда «в редких случаях». Пока скрипт
выключен, в чистой симуляции вред не проявляется (в освобождённой записи всё ещё `owner` = PuzzlePRO,
который выключен); в игре — проявится, если блок займёт запись другого включённого скрипта или
мусор с `owner < 0` (A3) и ненулевыми байтами `mute`/`enabled`.

**Как подтвердить в игре:** чистая сессия, только PuzzlePRO; выключить → включить → начать новую
песню. Ожидание при верной гипотезе: трасса не перегенерирована (`Do_ResetStats` подавлен), очки не
обнулены. Повторить 2–3 цикла, если с первого раза не вышло. Прямой признак — `active_count()` шима
> 0 при всех выключенных скриптах; наружу он сейчас не выведен (вкладка Scripts показывает только
`shared_channel_count`, который считает по живым записям и висячих не видит).

**Исправление (направление):** убрать `break` (снимать всех `user` из набора с этого binding'а,
следя, что `detach` может удалить сам binding), либо отписывать по записи через `unsubscribe`.
Дополнительно — `runnable(owner < 0)` не должен делать «чужой» мусор исполняемым (см. A3).

### A2. Отписка, пока канал исполняется через шим, освобождает binding из-под `shim_call_channel` — ПОДТВЕРЖДЕНО

`shim_call_channel` держит `binding* item` и итерирует `item->subscribers` до и после вызова
оригинала. Ни одна из функций изменения (`subscribe` → `add_subscriber`, `detach`, `remove`,
`remove_all_in`, `remove_all`, `unsubscribe_all_of`) не знает, что binding сейчас «в полёте». Три
пути попасть в это:

- **хук сам себя снимает / подписывает ещё** во время `before`/`after` (вставка в вектор во время
  range-for — инвалидация итераторов; удаление последнего подписчика — `delete item` под шимом);
- **оригинал канала рекурсивно доходит до кода, который отписывает** этот канал. Главный путь:
  `Present` вызывается **изнутри обхода графа** (`StartGroup::StartChannel … Present`,
  `reversing-journal-boot.md` §2.5), а из `Present` идут и `on_frame` скриптов, и команды хоста
  (`lua_remote::pump`), и кнопки вкладки Scripts (выкл/Reload). Значит, если скрипт подписан на
  любого **предка** канала, вызывающего `Present` (`Do_RenderD3D`, `StartChannel` и т.п.), то его
  выключение/перезагрузка из оверлея освобождает binding, чей `shim_call_channel` висит на стеке, и
  после возврата оригинала шим читает `item->subscribers` из освобождённой памяти;
- `on_frame`-колбэк, вызывающий `tw.on_call` для канала-предка `Present`, который уже подписан, —
  `add_subscriber` в вектор, по которому шим будет итерировать `after` (это безопасно только потому,
  что `after`-цикл начинается заново после возврата оригинала; а вот подписка из `before`-хука того же
  канала — уже инвалидация).

**Репро** (`shim_audit.exe a2`): хук в `before` делает `unsubscribe` своего канала → `Segmentation
fault` (отладочная куча заливает освобождённое `0xDD`). В Release-куче это тихое чтение протухших
указателей: дальше вызывается `item->original_call` из освобождённого блока и `dispatch_channel_call`
с освобождённой записью.

**Направление исправления:** счётчик «в полёте» на binding (или глобальная глубина диспатча) и
отложенное удаление: `detach` под полётом помечает подписчика мёртвым и не трогает вектор, а
освобождение binding'а — когда полёт кончился; `shim_call_channel` копирует `original_call` в локальную
переменную до цикла. Подписка во время полёта — в «ожидающие», сливаются после.

### A3. `sched::runnable(owner < 0)` отвечает `true` — мусор становится «исполняемым» — ПО КОДУ

`lua_sched.cxx: runnable` — `if(owner < 0) return true;` (владелец −1 — сам слой). Само по себе
верно, но в связке с A1 это то, что делает висячую запись опасной: любой мусор с отрицательным
словом на месте `owner` проходит проверку здоровья, и дальше решают байты `mute`/`enabled`. Пока A1
не исправлено, это главный усилитель. После исправления A1 — не баг.

### A4. `float_ch:set` / `vector_ch:set` на табличной колонке — запись строки без проверки границы — ПОДТВЕРЖДЕНО (IDA)

`Array Value` и `Array Vector` — это числовое и векторное семейство (`channel_kind::of` сравнивает
base guid), поэтому `tw.float_ch(g, "<колонка>")` резолвится и `:set(v)` проходит через
`fam_number::set` → слот 19. Снято IDA, `Aco_Array_Value::SetFloat` (RVA `0x1350` в
`DF5BF7F7-….dll`):

```c
if (this->bound /*+0x88*/) {
    if (child0) this->row /*+0x90*/ = (int)child0->GetFloat();   // строка = текущий курсор ИГРЫ
    if (!connect->GetIfValid()) connect->Connect();
    connect->SetData(this->row, &v, 4);                          // +0x58
}
Aco_FloatChannel::SetFloat(v);
```

`ArrayConnectItem::SetData` (RVA `0x1f00` в `2346A6DF-….dll`) — `GetRow(row)`, при отсутствии
`CreateRow(row)` и запись. То есть защита `has_row`, ради которой `fam_table::write` существует,
обходится обычным хендлом: строка — та, на которой **сейчас стоит курсор игры** (после
`Do_InitTrack` это, например, `Index_TrafficPattern = -1`), и если её нет — таблица игры
удлиняется (`CreateRow(-1)`, `CreateRow(<большое>)` — что делает колонка с таким индексом, не
проверено). Для `Array Vector::SetVector` по журналу движка ещё и нет проверки connect на `NULL`.
Из PuzzlePRO такой записи нет — это дыра API, не причина бага.

**Направление:** в `fam_number::set` / `fam_vector::set` (или при резолве в `float_ch`/`vector_ch`)
отказывать каналам с точным типом `Array Value`/`Array Vector` — их запись только через `tw.array`.
Заодно проверить остальные типы числового семейства с собственным слотом 19 (`Trigger`, `Lua Script`,
`Counter` и т.п.) — у них `set` может быть не «store», а действие.

### A5. Чтение мемоизируемой колонки оставляет в мемо **нашу** строку до конца кадра — ПО КОДУ, низкий риск

`fam_table::read`/`read_vector` сбивают мемо **до** чтения и не восстанавливают после. После чтения
`calculatedAt == tree count` текущего кадра, а `+0x7c` — значение строки скрипта; следующий читатель
игры в этом кадре (для колонки с `CHIC = 0`) получит её независимо от своего курсора. Счётчик
группы — `engine count + своё смещение` (`A3d_ChannelGroup::GetTreeCalculateCount`, RVA `0x9840`,
снято IDA), то есть «до конца кадра». Таких колонок в проекте 31 из 431 `Array Value`. Колонки
PuzzlePRO — `StatCollector #57` и `TrafficCommander`-овская — `CHIC = 1` (проверено по `.cgr`), так
что к багу PuzzlePRO отношения не имеет; комментарий в `fam_table.cxx` («`Stats: TrafficPattern`
ships without a CHIC chunk, so it *is* memoised») для `#57` неверен.

**Направление:** сохранять `+0x10` и `+0x7c` колонки до чтения и возвращать после (так же, как курсор).

### A6. Повторный вход в VM: обработчик `on_call` через `lua_pcall` изнутри FFI-вызова скрипта — ГИПОТЕЗА (механизм по коду)

Скрипт зовёт C через FFI (`C_get`, `C_set`, `C_set_vector`, `C_array_*`). Если внутри этого вызова
движок дойдёт до `CallChannel` канала, на котором висит `tw.on_call`, то шим →
`dispatch_channel_call` → `sched::dispatch_call` → `lua_pcall` **на том же `lua_State`**, пока
внешний FFI-вызов не вернулся. Защиты нет: `dispatch_call` не смотрит, исполняется ли сейчас Lua.
LuaJIT не поддерживает вход в состояние через классический C API из C-функции, вызванной через FFI
(VM не синхронизирует `L->base/top`, у JIT-трассы — тем более); итог — порча Lua-стека или PANIC.
Глушилки (`mute`) сюда не попадают — они в VM не входят.

Когда это достижимо: чтение канала, у которого `GetFloat` что-то вызывает (`Lua Script` →
`RunScriptValue` → игровой Lua, способный звать каналы; возможно `Trigger` и т.п.), запись в канал с
действием в сеттере, `vector_ch:set` (пробрасывает `SetFloat` детям-`Value`). PuzzlePRO таких чтений
не делает (`NumTilesInGrid = Puzzle::Fetch_NumberBlocksInPlay`, оба — простые `Value`,
`cgrpy query value`), то есть к багу отношения не имеет.

**Направление:** флаг «VM занята» (ставится в `run_export`/`dispatch_call`/`run_script`); при нём
`dispatch_call` не входит в VM, отвечает «proceed» и пишет один раз в диагностику владельца.

### С1. Глушилки PuzzlePRO в `XX_gui` портят игровую логику — СНЯТО

Все семь заглушённых узлов (`Do_LadderMedalRequirements`, `Do_ShowSongName`, `#3938`, `#3970`,
`#2351`, `#5412`, `#3707`) пишут только своё GUI-состояние (`EarnedBronze?`, `PointFloaters.*`,
`PointsByColor.LastKnownPoints`, `SongnameX`…); `cgrpy query users` — ни одного импорта этих каналов
из других групп. Единственная внешняя запись — `StartGroup::inFetchColorByID_NoParameter` в `#2351`
(параметр вызова, не состояние). Пропуск этих узлов на очки и трассу не влияет.

### С2. Каналы удаляются мимо детура `Release` — СНЯТО (почти)

Снято IDA в `HighPoly.dll`: `~A3d_ChannelGroup` (RVA `0x9b40`) зовёт `A3d_ChannelGroup::Release`
(RVA `0x8b00`) **прямым** вызовом, а Detours патчит саму функцию — перехват срабатывает и на этом
пути. `Release` — не счётчик ссылок, а «удалить всё содержимое»: в цикле `RemoveChannel(i)`
(слот 14, RVA `0x8c90`), который удаляет канал его же slot-0 деструктором. Остаётся одна дверь:
`RemoveChannel` — публичный виртуальный метод группы, его могут звать канальные DLL поштучно (xref
внутри `HighPoly.dll` — только vtable). Если так удалить подписанный канал, binding останется с
висячим `channel` (vptr не трогается — проверка в `restore()` это ловит), а новый канал по тому же
адресу будет «найден» `find_binding` и подписка на него молча не встанет. Открытый вопрос — кто зовёт
слот `+0x38` группы.

---

## Ход работы

- 24.10: прочитаны `channel_shim`, `api_hooks`, `lua_host`, `fam_table`, `channel_ref`; PuzzlePRO
  (`channels.lua`, `mutes.lua`, `scan.lua`, `tally.lua`). Найдены A1, A2; оба подтверждены
  `harness/audit/shim_audit.exe`.
- 24.10: IDA — `HighPoly.dll` (Release/деструктор группы, tree count), `Array Value`
  (`SetFloat`/`GetFloat`), `Array Unique` (`ArrayConnectItem::GetData/SetData/GetRow`). Записаны A3–A5,
  С1, С2. CHIC колонок PuzzlePRO снят по `.cgr` (`examples/qchic.py`, `chic_map`). `A3d_List::GetItem`
  (RVA `0x6b70`) проверяет границы — чтение запредельной строки, видимо, безопасно, но сама колонка
  (её `GetRow` по `+0x2c` connect-объекта) не найдена.

- 24.10: сквозное репро A1 (`toggle_audit.exe`) — подавление со второго включения. Прочитаны
  `channels.lua`, `api_channels.cxx`, `callbacks.lua`, `lua_sched` (unload, enter/leave,
  dispatch_call), `lua_registry` (set_enabled, find/place_of — границы проверяются),
  `engine_control`, `channel_kind`, `channel_vtable`, все `fam_*`, `sandbox.lua`. Записаны A6.

**Проверено и в порядке** (чтобы не перепроверять): протухшие хендлы — `Channel:_resolve` сверяет
`tw_graph_revision()` на каждом чтении, а ревизия бьётся синхронно в детуре выгрузки, то есть
указатель в уничтоженную группу Lua не использует; регистрация `tw.on_call`/`tw.mute` из колбэков
запрещена (`window_open`), поэтому подписка из хука невозможна, а `on_ready` идёт из `EngineLoop` до
графа, где кадров шима на стеке нет; `dispatch_call` для снятого обработчика отвечает «proceed»;
`hk_engine_loop` защищён от вложенности; `runnable`/`find`/`place_of` проверяют границы.

## Открытые вопросы

- Кто зовёт `A3d_ChannelGroup::RemoveChannel` (слот `+0x38`) поштучно (С2).
- Сравнивает ли где-нибудь движок/канальные DLL vptr канала с константой `??_7…` (копия vtable
  такую проверку провалит). Поиск по шаблону `cmp [reg], offset ??_7…` в 242 DLL не делался.
- `GetChannel(group, имя)` на канале-импорте (`--stubs` в cgrpy): возвращает заглушку или цель? Если
  цель — binding запишет чужую группу, и `remove_all_in` при выгрузке настоящей группы его не снимет
  (vptr тогда уйдёт вместе с объектом, блок утечёт; падения нет, но см. С2 про адрес).
- Колонка таблицы за `connect+0x2c`: проверяет ли её `GetRow`/`CreateRow` индекс (отрицательный,
  огромный, NaN → `ftol` = `0x80000000`). От ответа зависит, чем кончается A4 и `array:get(-1)`.
- Типы числового семейства с собственным слотом 19 / 17, у которых `set`/`get` — действие (A4, A6).

## Не просмотрено

`lua_tabs`, `api_hud`, `lua_hud_space`, `lua_ui`, `lua_storage`, `lua_log`, `lua_diag`, `lua_linemap`,
`lua_remote` (кроме того, что `pump()` исполняет команды из `Present`) — это ImGui/файлы/IPC, игровое
состояние они не трогают; `engine_state`, `engine_frame`, `engine_symbols` — только чтение;
`framework/` кроме `channel_shim` (D3D9/dinput8/texture хуки) — вне графа каналов.

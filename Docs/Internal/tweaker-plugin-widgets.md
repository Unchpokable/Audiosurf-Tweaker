# Custom widgets API (`TweakerPlugin`)

## Что это и зачем

В `TweakerPlugin` UI оверлея строится на **ImGui**, но стандартные контролы ImGui по виду и поведению не совпадают с визуальным языком TweakerUI (акценты, soft surfaces, tween-анимации). Поэтому поверх ImGui лежит свой слой виджетов в пространстве имён `tw::ui::widgets`.

Они:

- рисуются вручную через `ImDrawList` + `ImGui::ButtonBehavior` (hit-test и ввод ImGui, внешний вид — наш);
- читают цвета из `tw::ui::theme` **каждый кадр** (тема может меняться в рантайме);
- анимируют hover / press / selection через `tweeny` (`src/libtweeny`);
- живут как **stateful-объекты** (не как одноразовые `ImGui::Button(...)` в стиле immediate-only без собственного state).

Код:

| Путь | Назначение |
|------|------------|
| `TweakerPlugin/src/ui/widgets/*.hxx\|.cxx` | Виджеты |
| `TweakerPlugin/src/ui/widgets/detail/draw.hxx` | Общие хелперы отрисовки / lerp / alpha |
| `TweakerPlugin/src/ui/theme.hxx\|.cxx` | Палитра |
| `TweakerPlugin/src/ui/CMakeLists.txt` | Статическая библиотека `tweaker_ui` |
| `TweakerPlugin/smoke/main.cxx` | Визуальный smoke-harness |

Сборка smoke (не входит в `ALL`):

```bash
cmake --build --preset x86-release --target smoke_test
# exe: TweakerPlugin/build/x86-release/bin/tweaker_ui_smoke.exe
```

---

## Базовые правила

### 1. Виджет — объект с временем жизни

Виджеты **нельзя** создавать на стеке каждый кадр: внутри tween-состояние, hover/press flags, (у списков) кэш строк.

Типичный паттерн в draw-коллбеке / smoke:

```cpp
using namespace tw::ui::widgets;

static button apply_btn{"settings_apply", {140.f, 32.f}};
static toggle ghost_lane{"settings_ghost", {44.f, 24.f}};

apply_btn.update("Apply");
if (apply_btn.clicked()) {
    // ...
}
ghost_lane.update();
```

### 2. `id` должен быть стабильным и уникальным в области ImGui ID stack

Конструктор принимает `const char* id`. Он уходит в `ImGui::PushID` и влияет на hit-testing.

- Не переиспользуйте один и тот же `id` у двух живых виджетов в одном окне/ветке ID.
- Не меняйте `id` между кадрами у одного экземпляра.
- Вложенность (`BeginChild`, `tab_view::begin_view` с `PushID(tab)`) создаёт отдельные ветки — одинаковые локальные id в разных табах обычно ок.

### 3. Размер: `0` = «заполни доступное»

Почти везде `ImVec2 size`, где ось `<= 0` резолвится через `detail::resolve_size`:

- `size.x <= 0` → `ImGui::GetContentRegionAvail().x` (с fallback на дефолт виджета);
- `size.y <= 0` → либо avail (если так задумано у конкретного виджета), либо фиксированный default height.

Практика:

```cpp
list.set_size({0.f, 180.f});          // ширина на всю колонку, высота 180
tabs.set_size({0.f, avail_h});        // и ширина, и высота из родителя
button{"ok", {120.f, 32.f}};          // фиксированная кнопка
```

Менять размер можно каждый кадр через `set_size` (удобно, когда высота = `GetContentRegionAvail().y` после заголовка).

### 4. Цвета — только из `theme`, каждый кадр

Не кэшируйте `ImU32` цвета между кадрами. Виджеты сами читают `theme::…` при отрисовке.

```cpp
tw::ui::theme::apply_dark();                 // дефолты
tw::ui::theme::from_config(config_text);     // key=hex поверх dark
```

Формат `from_config`: строки `key=#AARRGGBB` или `#RRGGBB`, по одной на строку; неизвестные ключи игнорируются. Список ключей — имена полей в `theme.hxx` (`accent_primary`, `surface`, `text_primary`, …).

### 5. Кадр = `update()` / `begin()…end()`

| Виджет | Как «тикать» за кадр |
|--------|----------------------|
| `button`, `toggle`, `list_item`, `list_view`, `color_picker` | один вызов `update(...)` |
| `item_group` | `begin()` → контент → `end()` |
| `popup_menu` | `if(opened()) { begin()` → контент → `end(); }` |
| `tab_view` | `begin()` → ноль+ `begin_view`/`end_view` → `end()` |

Флаги вроде `clicked()`, `changed()`, `selection_changed()` валидны **до следующего** `update`/`begin` того же экземпляра (сбрасываются в начале тика).

### 6. Alpha и кастомная отрисовка

`detail::to_u32` умножает alpha на `ImGui::GetStyle().Alpha`. Это обязательно для корректного fade внутри `tab_view` (и любого `PushStyleVar(ImGuiStyleVar_Alpha, …)`).

Если пишете новый виджет или ad-hoc `AddText`/`AddRect` в том же стиле — **не** зовите `ColorConvertFloat4ToU32` напрямую; используйте `detail::to_u32` / `detail::apply_style_alpha`.

---

## Тема (`tw::ui::theme`)

Группы:

- **Accent** — `accent_primary`, `accent_text`, `accent_soft`, `accent_selected`, `accent_pressed`, …
- **Surfaces** — `app_background`, `surface`, `surface_muted`, `surface_row`, …
- **Borders** — `border`, `border_subtle`, …
- **Text** — `text_primary`, `text_secondary`, `text_muted`, …

Виджеты сознательно завязаны на эти токены (не на `ImGuiCol_*`), чтобы оверлей совпадал с хостом TweakerUI.

---

## `button`

Простая кнопка с hover/press tween и лёгким «вдавливанием» при press.

```cpp
#include "ui/widgets/button.hxx"

static button save{"menu_save", {160.f, 36.f}};
save.set_label("Save");          // опционально, можно хранить метку
save.update();                   // рисует stored label
// или:
save.update("Save now");         // метка только на этот кадр (как ImGui::Button)

if (save.clicked()) { /* ... */ }
if (save.hovered()) { /* ... */ }
```

**Замечания:**

- `clicked()` — true только в кадре отпускания/срабатывания `ButtonBehavior` (как у ImGui).
- Анимации: hover ~150 ms cubicOut, press ~90 ms quadraticOut.

---

## `toggle`

Бинарный переключатель (track + thumb).

```cpp
#include "ui/widgets/toggle.hxx"

static toggle enabled{"feat_enabled", {44.f, 24.f}};

enabled.update();
if (enabled.changed()) {
    persist(enabled.checked());
}

// Программная установка (тоже запускает tween):
enabled.set_checked(false);
```

**Замечания:**

- Клик внутри `update()` сам тоглит `checked` и ставит `changed() == true` на этот кадр.
- `set_checked` без изменения значения — no-op (tween не рестартует).

---

## `list_item`

Одна строка списка: опциональная иконка + текст, состояния hover/selected.

```cpp
#include "ui/widgets/list_item.hxx"

static list_item row{"skin_row_0", {0.f, 36.f}};
static bool selected = false;

row.set_content({
    .text = "Neon Pulse",
    .icon = my_tex,              // или ImTextureID_Invalid
    .icon_size = {64.f, 64.f},   // 0,0 = квадратный fit в слот
});
row.set_selected(selected);
row.update();
if (row.clicked()) {
    selected = !selected;
}
```

Перегрузка `update(const list_item_content&)` рисует переданный контент без записи в `m_content` навсегда — удобно, если контент считается на лету.

**Замечания:**

- Selection визуально управляется снаружи через `set_selected` (виджет сам по клику selected не переключает — это делает `list_view` или ваш код).
- Иконка вписывается с сохранением aspect ratio (`detail::add_image_keep_aspect`).

---

## `list_view`

Скроллируемый single-select список на базе `list_item`.

```cpp
#include "ui/widgets/list_view.hxx"

static list_view skins{"skins_list", {0.f, 200.f}};
static bool seeded = false;

if (!seeded) {
    const std::vector<list_item_content> items = {
        {.text = "Neon Pulse"},
        {.text = "Mono Track"},
        // ...
    };
    skins.set_items(items);   // копирует данные, пересоздаёт строки
    seeded = true;
}

skins.set_size({0.f, ImGui::GetContentRegionAvail().y});
skins.update();

if (skins.selection_changed()) {
    apply_skin(skins.selected_index()); // -1 если ничего не выбрано
}
```

**Замечания:**

- `set_items` — относительно тяжёлый (пересборка строк); не вызывайте каждый кадр без нужды.
- `selected_index()`: `-1` = нет выбора; после урезания списка индекс может сброситься.
- Внутри — `BeginChild` со скроллом; всегда парный `EndChild` (уже внутри `update`).
- **Не вызывайте `update()` дважды за кадр** на одном экземпляре (например, в двух табах `tab_view` во время crossfade) — заведите отдельные `list_view` на разные панели.

---

## `item_group`

Группирующая рамка (GroupBox): shrink-to-content контейнер с обводкой и заголовком на верхней кромке. Контент между `begin()` / `end()` — любые ImGui-вызовы и кастомные виджеты.

```
  ┌─ Actions ─────────────┐
  │  [widget A]           │
  │  [widget B]           │
  └───────────────────────┘
```

### Макет и размеры

- **Shrink-to-content** по обеим осям: рамка = bounding box детей + `inner_padding` (и расширение, если `group_title` шире контента). Не вызывает `resolve_size` / не тянется на `GetContentRegionAvail`.
- Верхняя кромка рамки проходит через середину текста заголовка; линия обводки в зоне title **прерывается** (gap).
- Скругление по умолчанию **5px** (`set_rounding`).
- `outer_padding` (default `{0, 4}`) — отступ самой группы от окружающего layout.
- `inner_padding` (default `{8, 8}`) — отступ между рамкой и внутренними элементами.
- Inset заголовка от левого угла ≈ `max(rounding, 8)`; горизонтальный pad разрыва обводки ≈ 4px вокруг текста.

### Цвета и отрисовка

- Обводка: `theme::border` через `detail::to_u32` каждый кадр (без кэша `ImU32`).
- Заголовок: `theme::text_secondary`.
- Без заливки фона группы и без tween-анимаций.
- Рамка рисуется через `AddRect` (как у `button` / `tab_view`); gap под title — закраска `ChildBg` / `WindowBg`. Координаты снапятся на пиксельную сетку — самодельный `PathStroke` здесь не использовать (даёт неровную толщину/блюр).

### Lifecycle

```cpp
#include "ui/widgets/item_group.hxx"

static item_group actions{"settings_actions", "Actions"};
actions.set_group_title("Actions");   // опционально, можно задать в ctor
actions.set_rounding(5.f);
actions.set_outer_padding({0.f, 4.f});
actions.set_inner_padding({8.f, 8.f});

actions.begin();
save_btn.update("Save");
ghost_lane.update();
actions.end();                        // обязателен
```

Инварианты (в debug — `assert`):

- Не вкладывать `begin()` в `begin()` на одном экземпляре без `end()`.
- Каждому `begin()` — ровно один `end()` (иначе разъедется ImGui ID / `BeginGroup` stack).
- Как и у остальных виджетов: экземпляр **stateful** и долгоживущий (`static` / member), не на стеке каждый кадр.

**Замечания:**

- Внутри — `ImGui::BeginGroup` / `EndGroup`; после измерения — `Dummy` на занятый rect (с учётом title overhang и `outer_padding`), чтобы родительский layout / `SameLine` не ломались.
- Пустой title допустим: рамка без gap, top inset = только `inner_padding.y`.
- Smoke: обёртка кнопок в окне `Smoke controls` (`"Actions"`) в [`smoke/main.cxx`](../../TweakerPlugin/smoke/main.cxx).

---

## `popup_menu`

Всплывающее окно со shrink-to-content, скруглённым chrome и opacity crossfade. Caller сам открывает (`open()`); позиция = курсор в момент открытия (верхний левый угол). Основной кейс — ПКМ над якорем; ЛКМ-open — явная кнопка/зона, спроектированная под это.

```
  (cursor)
     ┌─ Context ────────┐
     │  [widget A]      │
     │  [widget B]      │
     └──────────────────┘
```

### Lifecycle

```cpp
#include "ui/widgets/popup_menu.hxx"

static button more{"menu_more", {120.f, 32.f}};
static popup_menu ctx{"ctx_menu", "Context"}; // title optional (default empty)
static list_item copy_row{"ctx_copy", {160.f, 28.f}};

more.update("More…");
if(more.clicked()) {                          // явный LMB-open
    ctx.open();
}
// типичный ПКМ:
// if(ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
//     ctx.open();
// }

if(ctx.opened()) {
    ctx.begin();
    copy_row.set_content({.text = "Copy"});
    copy_row.update();
    if(copy_row.clicked()) {
        ctx.close();
    }
    ctx.end();
}
```

`opened()` остаётся `true` на время fade-out (чтобы `if(opened())` рисовал crossfade). При dismiss сразу выставляется `NoInputs` — полупрозрачный остаток не перехватывает клики.

### Макет и внешний вид

- **Shrink-to-content** по обеим осям (как `item_group`): рамка = bounding box детей + `inner_padding` (+ title band, если задан).
- Скругление по умолчанию **5px** (`set_rounding`).
- `inner_padding` (default `{8, 8}`).
- Fill: `theme::surface` (темнее elevated — чтобы press/hover кнопок и list_item не сливались с задником).
- Обводка: `theme::accent_border` + лёгкий `add_rect_glow` от `accent_primary`.
- Опциональный заголовок (`ctor` / `set_title`): полоска `accent_soft` сверху, текст `accent_text`. Пустой title — без полоски.
- Отдельное ImGui-окно (`NoDecoration` / `NoBackground`) — поверх родителя, с собственным hit-test.
- Origin clamp’ится к viewport (после измерения размера — сдвиг на следующий кадр при необходимости).

### Open / close / dismiss

| Событие | Поведение |
|---------|-----------|
| `open()` | TL = `GetMousePos()`; fade-in opacity 0→1 (~200 ms cubicOut); interactive |
| `close()` / клик вне / потеря фокуса окна | сразу `NoInputs`; fade-out →0; `opened()` false только когда opacity ≈ 0 |
| `open()` во время Closing | новый TL; fade-in от текущего opacity; снова interactive |

Dismiss по клику (ЛКМ/ПКМ) вне frame rect или когда окно popup теряет фокус (после того как хотя бы раз его получило). Кадр `open()` и следующий — dismiss пропускается, чтобы открывающий клик не закрыл меню сразу.

### Инварианты (debug — `assert`)

- Не вкладывать `begin()` в `begin()` без `end()`.
- Каждому `begin()` — ровно один `end()`.
- Caller оборачивает пару в `if(opened())`; при вызове `begin` на закрытом меню — no-op (без ImGui stack).

### Подводные камни

1. **Не шарьте один stateful-виджет** между popup и другим местом, которое может рисоваться в том же кадре.
2. Контент должен уважать `StyleVar_Alpha` (`detail::to_u32`) — иначе fade «мигает».
3. Виджет **не** привязывается к якорю сам: открытие и выбор кнопки мыши — на стороне caller.

Smoke: LMB «Open popup» + RMB в `Smoke controls` — [`smoke/main.cxx`](../../TweakerPlugin/smoke/main.cxx).

---

## `color_picker`

HSV+Alpha пикер: 2D SV-квадрат, слайдеры Hue и Alpha на **всю ширину виджета**, справа от квадрата — hex и RGBA (оба сразу, двусторонняя синхронизация). Весь контент рисуется внутри одного `update()`; наружу — `ImVec4` RGBA.

```
┌──────────────┐  #AARRGGBB
│   SV (S×V)   │  R / G / B / A
└──────────────┘
[════════════ Hue (full width) ════════════]
[═══════════ Alpha (full width) ═══════════]
```

### Lifecycle

```cpp
#include "ui/widgets/color_picker.hxx"

static color_picker accent{"theme_accent", {280.f, 0.f}};
static ImVec4 accent_color{0.2f, 0.83f, 0.75f, 1.f};

// Внутренний цвет:
accent.update();
if(accent.changed()) {
    use(accent.color());
}

// Или bind к внешнему ImVec4 (pull при внешней записи, write-back каждый кадр):
accent.update(accent_color);
```

`set_color(ImVec4)` — программная установка (HSV пересчитывается; hue сохраняется при S≈0).

### Замечания

- Внутренняя модель — HSV (`m_h/m_s/m_v/m_a`); наружу только RGBA float 0–1.
- Hex: `#RRGGBB` или `#AARRGGBB` (ARGB, как в `theme::from_config`). Apply на Enter / deactivate поля.
- RGBA-поля — int 0–255. Пока поле в фокусе, интерактивный drag не перезаписывает буфер ввода.
- Скругление SV/баров ~5px; цвета обводок из `theme::border`; отрисовка через `detail::to_u32` (fade в `tab_view` / `popup_menu`).
- `size.x <= 0` → ширина из `GetContentRegionAvail`; `size.y <= 0` → auto-высота от SV + 2 бара.
- Как и остальные виджеты: stateful, не создавать на стеке каждый кадр; не шарить один экземпляр между двумя `tab_view` panels во время crossfade.

Smoke: группа `"Color"` в [`smoke/main.cxx`](../../TweakerPlugin/smoke/main.cxx).

---

## `tab_view`

Вертикальная навигация слева + контентная область справа с crossfade при смене таба.

### Макет

```
+------------------+----------------------------+
| Tab A            |                            |
| Tab B  [active]  |   content (ImGui + widgets)|
| Tab C            |                            |
+------------------+----------------------------+
```

- Ширина nav-панели = ширина самой широкой текстовой метки + padding; все таб-кнопки одной ширины.
- Скругление по умолчанию **5px** (`set_rounding`).
- Active tab: более светлый текст + soft glow (`accent_text` / `accent_primary`) + фон `accent_soft`.

### Обязательный lifecycle

```cpp
#include "ui/widgets/tab_view.hxx"

static tab_view tabs{"settings_tabs", {0.f, 280.f}};
static bool tabs_ready = false;

if (!tabs_ready) {
    const std::string_view labels[] = {"Options", "Skins", "About"};
    tabs.set_tabs(labels);     // до begin(), не внутри begin_view
    tabs_ready = true;
}

tabs.set_size({0.f, ImGui::GetContentRegionAvail().y});
tabs.begin();

if (tabs.begin_view(0)) {
    // ImGui + custom widgets таба 0
    tabs.end_view();
}
if (tabs.begin_view(1)) {
    // ...
    tabs.end_view();
}
if (tabs.begin_view(2)) {
    // ...
    tabs.end_view();
}

tabs.end();

int i = tabs.selected_tab();
if (tabs.selection_changed()) { /* ... */ }
```

Инварианты (в debug — `assert`):

- Не вкладывать `begin()` в `begin()` без `end()`.
- Каждому успешному `begin_view` — ровно один `end_view`.
- `end()` только после закрытия всех view.

### Семантика `begin_view(index)`

| Условие | Возвращает | Opacity контента |
|---------|------------|------------------|
| `index == selected_tab()`, переход завершён | `true` | `1.0` |
| `index == selected_tab()`, идёт fade-in | `true` | `content_t` (0→1) |
| `index == previous_tab`, идёт fade-out | `true` | `1 - content_t` |
| иначе / child clipped / bad index | `false` | — |

Пока идёт transition, **могут рисоваться оба** таба (outgoing + incoming) в перекрывающихся page-child фиксированного размера. Outgoing получает `ImGuiWindowFlags_NoInputs`, чтобы скролл/клики не отбирались у incoming.

`selected_tab()` — **целевой** индекс после клика (не previous).

### Подводные камни `tab_view`

1. **`set_tabs` только снаружи `begin`/`begin_view`.** Внутри — нельзя.

2. **Всегда вызывайте `end()`**, даже если все `begin_view` вернули `false` (например, content child clipped). Иначе разъедется стек ImGui (`EndChild` / `PopID`).

3. **Клиппинг / скролл родителя.** `BeginChild` content вызывается всегда; `EndChild` обязателен даже при `BeginChild == false`. Это уже внутри `tab_view` — вам достаточно держать пару `begin`/`end`.

4. **Не шарьте один stateful-виджет между двумя табами**, которые могут оба вернуть `true` во время crossfade. Отдельные экземпляры на таб (или контент только в одном табе).

5. **Заполнение высоты.** Считайте `avail` у родителя и `set_size({0.f, avail})` *до* `begin()`. Внутри page: после заголовка снова `GetContentRegionAvail().y` для вложенного `list_view`.

6. **Родительское окно и «лишний» scroll.** Если снаружи после блока, который съел весь `GetContentRegionAvail()`, ещё рисуется футер — зарезервируйте под него высоту заранее (см. nested-секцию в `smoke/main.cxx`).

7. **Stock ImGui vs custom.** Оба должны уважать `StyleVar_Alpha`. Кастомные виджеты делают это через `detail::to_u32`. Сырой `AddText` без alpha-множителя во время fade будет «мигать».

---

## Вложенность и layout

Рекомендуемый паттерн «панель → табы → список»:

```cpp
const float footer_h = ImGui::GetTextLineHeightWithSpacing() + 4.f;
const float shell_h = std::max(200.f, ImGui::GetContentRegionAvail().y - footer_h);

if (ImGui::BeginChild("##panel", ImVec2{0.f, shell_h}, ImGuiChildFlags_Borders)) {
    // header ...
    tabs.set_size({0.f, ImGui::GetContentRegionAvail().y});
    tabs.begin();
    if (tabs.begin_view(0)) {
        ImGui::TextUnformatted("Library");
        const float list_h = std::max(80.f, ImGui::GetContentRegionAvail().y);
        library.set_size({0.f, list_h});
        library.update();
        tabs.end_view();
    }
    // другие табы ...
    tabs.end();
}
ImGui::EndChild();
ImGui::Text("status..."); // футер, под который reserved footer_h
```

Не делайте:

- `Dummy(full)` + потом ещё `BeginChild` той же высоты без `SameLine` / без аккуратного claim — легко получить удвоенную высоту и вечный scrollbar;
- `SetCursorScreenPos` / `SetCursorPos` «за границы» без последующего `Dummy`, закрывающего расширенный rect (ImGui выдаст warning).

`tab_view` сам использует схему `Dummy(nav_width, height)` + `SameLine` + content `BeginChild`, чтобы не двойнить высоту в родителе.

---

## Анимации (ориентиры)

| Событие | Длительность | Easing (как в коде) |
|---------|--------------|---------------------|
| Hover (button / list_item / tab) | ~150 ms | cubicOut |
| Press (button / tab) | ~90 ms | quadraticOut |
| Select (list_item / tab label) | ~150 ms | cubicOut |
| Toggle thumb | ~200 ms | cubicOut |
| Tab content crossfade | ~200 ms | cubicOut |
| Popup open/close fade | ~200 ms | cubicOut |

Source of truth для текущего значения — **собственные `float` поля** виджета; `tween.step(dt)` пишется в них. Не опирайтесь на `peek()` у только что созданного tween (см. комментарии в `button.cxx` / `list_item.cxx`).

---

## CMake / линковка

- Библиотека: **`tweaker_ui`** (`TweakerPlugin/src/ui`).
- Публичный include root: `TweakerPlugin/src` → `#include "ui/widgets/button.hxx"`.
- `TweakerPlugin` DLL и `smoke_test` оба линкуют `tweaker_ui`.
- Smoke **не** использует PCH плагина; includes прописываются явно (как в `smoke/main.cxx`).

Новый виджет:

1. `widgets/foo.hxx` + `foo.cxx` в стиле существующих (snake_case, `m_`, namespace `tw::ui::widgets`).
2. Добавить `widgets/foo.cxx` в `tweaker_ui` в `src/ui/CMakeLists.txt`.
3. По возможности — секция в `smoke/main.cxx`.
4. Цвета только из `theme`; отрисовка через `detail::` хелперы; tween через `libtweeny`.

---

## Чеклист «виджет ведёт себя странно»

| Симптом | Что проверить |
|---------|----------------|
| ImGui: `Missing EndChild` / `PopID` | Пара `tab_view::begin`/`end`, `item_group::begin`/`end` или `popup_menu::begin`/`end`; не рано выходить из draw, минуя `end()` |
| ImGui: `SetCursorPos extend boundaries` | После абсолютного курсора нужен `Dummy` на занятый rect |
| Вечный scrollbar у окна | Не задвоен ли layout claim; не съел ли child весь avail без места под футер |
| Custom controls резко пропадают на смене таба | Рисуете ли через `detail::to_u32`; не обходите ли Style Alpha |
| `list_view` дёргается / ломает scroll в табах | Один экземпляр на два `begin_view` за кадр? |
| Кнопки «мертвые» / чужой hit-test | Коллизия ImGui `id`; забытый `PushID`/`PopID` снаружи |
| Нет анимации после `set_*` | Значение не изменилось (no-op) или `update` не вызывается |
| Тема «не применяется» | Читаете закэшированный `ImU32` вместо `theme::` каждый кадр |
| `item_group` обводка «мыльная» / разной толщины | Не PathStroke вручную — `AddRect` + pixel snap; gap title закраской WindowBg/ChildBg |
| `popup_menu` сразу закрывается после open | Открывающий клик попал в dismiss? Нужен skip кадра (встроено); не вызывайте `close()` сами в том же кадре без нужды |
| `popup_menu` ловит клики во время fade-out | Должен быть `NoInputs` после dismiss; проверьте, что `close()` / auto-dismiss выставили interactive=false |

---

## Где смотреть эталон

Актуальные примеры:

- Overlay modules + `item_group` / `popup_menu` в Smoke controls: [`TweakerPlugin/smoke/main.cxx`](../../TweakerPlugin/smoke/main.cxx) (`draw_smoke_controls`).
- Nested layout (`BeginChild` → `tab_view` → lists / toggles): interactive menu в том же smoke / `plugins/interactive/menu.cxx`.

Исходники виджетов: [`TweakerPlugin/src/ui/widgets/`](../../TweakerPlugin/src/ui/widgets/).
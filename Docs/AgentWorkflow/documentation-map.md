# Карта руководств и исследований

Читайте по теме задачи. Эти ссылки не требуют загружать все документы при
старте. Нормативные правила — в [каталоге workflow](README.md); документы
`Internal` содержат подробности реализации, доказательства и историю.

## Текущие работы

| Работа | Канонический источник |
|---|---|
| **Skyboxes Tab** — текущая незавершённая работа | [sky-manager-roadmap](../Internal/sky-manager-roadmap.md): продолжай от его шапки и фактического дерева. Уже начата в Revive; не объявляй следующие фазы выполненными по наличию класса. |
| **Lua audit** — исправления уже выполнялись | [lua-audit-journal](../Internal/lua-audit-journal.md): статус, решения владельца, репро, открытые находки. Старая память «research-only» не задаёт состояние. |
| **Developer Bundle** — постоянная сайд-задача | [dev-bundle-roadmap](../Internal/dev-bundle-roadmap.md) как история решений и конкретных обновлений AgentKit/CgrPy; старые Ф5/Ф6 не глобальная приёмка проекта. |
| **Миграция агентов** | [план и результаты](../Internal/agents-migration.md): решения владельца Q1–Q12, перенос источников, приёмка окружения. |
| **Раскладка исходников и каналы организации** | [repo-layout-plan](../Internal/repo-layout-plan.md): согласованный план выноса небес, проектов скриптов и harness в организацию, канал RC/alpha и перестройка дерева; реализация по фазам §6, итоги — §10. |

KnownBugs — личный блокнот владельца и в эту карту работ не входит.
Завершённые Script tabs описаны руководством ниже; старый roadmap архивный.

## Модули и проверки

| Тема | Вход |
|---|---|
| Общая архитектура | [architecture](architecture.md); [принципы](project-principles.md) |
| Стиль и организация кода | [code-style](code-style.md): обязательное чтение до правок; TU как модуль состояния, необходимость классов, запрет private static, читаемость/имена, naming/layout, PCH/include, ошибки/hooks и организация C#/AXAML |
| Защита от повторного запуска и присутствие модулей | Механизмы single-instance/mutex/pipe разобраны в [архиве обзора](../Internal/Archive/agent-migration/overview.pre-migration.md); текущую архитектурную вводную бери из architecture, не из исторического обзора. |
| Загрузка/оффлайн/установка | [plugin-offline-mode](../Internal/plugin-offline-mode.md): этапы загрузки и раскладка; старую политику совместимости и syscalls заменяют действующие workflow-правила |
| Протокол хост/bridge/плагин | [overlay-protocol](../Internal/overlay-protocol.md), [Quick Player](../Internal/overlay-quickplayer.md), [Scripts](../Internal/overlay-scripts.md) |
| Lua runtime/engine/ABI | [журнал аудита](../Internal/lua-audit-journal.md), [проект Lua](../Internal/lua-scripting.md), [закрытый план движка](../Internal/lua-engine-fix-roadmap.md): механика, семьи, жизненный цикл и доказательства. Начинай с аудита, не с выполнения старых фаз. |
| Хостовый модуль скриптов и twllink | [script-manager](../Internal/script-manager.md) |
| Порядок скриптов и место HUD | [script-coexistence](../Internal/script-coexistence.md) |
| Вкладки Lua/storage/prelude | [script-tabs](../Internal/script-tabs.md), сохраняющие номера §2/§3 для ссылок кода |
| PuzzlePRO HUD | [руководство](../Internal/puzzlepro-hud.md), затем его `docs/DESIGN.md` при работе непосредственно с проектом; исходный проект, не bundle |
| Виджеты оверлея | [tweaker-plugin-widgets](../Internal/tweaker-plugin-widgets.md), [история R/U и решений](../Internal/overlay-widgets-history.md) |
| Avalonia UI и память | [avalonia-pitfalls](../Internal/avalonia-pitfalls.md), [UiStand README](../../Tools/UiStand/README.md) |
| Skybox Replacer | [карта](../Internal/skybox-replacer-roadmap.md), [руководство](../Internal/skybox-replacer.md), далее shader/geometry/baked-noise документы по ссылкам карты |
| Публичный Lua API | [scripting](../scripting.md), [authoring](../scripting/authoring.md), [API reference](../scripting/api-reference.md), [обновление генерации](development.md#изменение-публичного-lua-api-и-бандлов) |
| Публичный формат неба | [skyboxes](../skyboxes.md) и его тематические manuals; `sky-package.md` устарел и не спецификация текущего `.sky` |
| Форматы скинов | [совместимость](project-principles.md#совместимость-разделена-по-данным), модель SkinPackager и LegacyDataConverter; не теряй перенос из legacy-формата |

## Реверс игры

Инструменты и базы — [reverse-engineering-tools](reverse-engineering-tools.md).
Первичные журналы:

- [boot](../Internal/reversing-journal-boot.md): жизненный цикл, готовность,
  группы, порядок кадра и семейства каналов;
- [engine](../Internal/reversing-journal-engine.md): HighPoly, ABI/vtable,
  ICF, экспорты и перехваты;
- [gameplay](../Internal/reversing-journal-gameplay.md): графы, статистика,
  режимы, очки, VisMusic; его старый §7.2 отменён engine §7;
- [lua](../Internal/reversing-journal-lua.md): формат CGR, Aco_Lua и ранний ресёрч;
- [geometry](../Internal/reversing-journal-geometry.md): ObjectData/меши/OBJ;
- [render](../Internal/reversing-journal-render.md): сцена, окружение трассы,
  частицы, порядок отрисовки, ограничения скриптового доступа.

Производные английские справочники — [internals](../internals.md). Не переноси
число из памяти без проверки указанной CgrPy-командой или бинарём. Новую
находку записывай в соответствующий журнал, не в клиентский кеш как единственный
источник. Исторический корневой CLAUDE сохранён в архиве миграции и не является
параллельной действующей инструкцией.

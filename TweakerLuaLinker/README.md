# TweakerLuaLinker — `twllink.exe`

Компоновщик Lua-проектов для скриптов `TweakerPlugin`: модули и `require` на входе, один `.lua` для
`engine\TweakerStuff\Scripts\` и `.lua.map` на выходе. Плюс проверки по фактическому API плагина,
установка в игру, шаблоны проектов.

Этот файл — для того, кто правит сам `twllink`. **Как им пользоваться** (проект, манифест, команды,
таблица кодов диагностик) — `Docs/scripting/authoring.md`, и только там: это публичный контракт, и
его копия здесь разошлась бы. Для хоста (`TweakerUI`) — краткое руководство по CLI с контрактом
`--json` и кодов возврата: `Docs/Internal/script-manager-roadmap.md`, §3. Почему всё устроено именно
так — `Docs/Internal/lua-engine-fix-roadmap.md` §9 и §12, Ф5.

## Раскладка

```
Program.cs              только Ctrl+C → отмена и сбой → код 3
Cli.cs                  разбор аргументов, сборка, --install, --new, вывод текстом и --json
Project/                Manifest + ManifestReader (twproject.json, JsonDocument без рефлексии),
                        Scaffold (--new, папка .twllink/ проекта), GameInstall (--install, AUDIOSURF_DIR)
Linking/                ModuleGraph (разбор Loretta, require, регистр, циклы, недостижимые модули),
                        SourceLines (строки модуля + их исходные номера; strip комментариев),
                        BundleWriter (вывод построчно, карта пишется ВО ВРЕМЯ вывода, хэш бандла),
                        Linker (порядок работы), ApiChecker (проверки TW03xx по api.json)
Api/                    ApiDescription (api.json), ApiReference (разбор api-reference.md),
                        DefinitionWriter (tw.def.lua для Lua Language Server)
Diagnostics/            Diagnostic, DiagnosticBag, DiagnosticCodes (коды не переиспользуются)
```

Вшиты в exe (`EmbeddedResource`): `Docs/scripting/api.json`, `Docs/scripting/tw.def.lua`,
`Docs/scripting/twproject.schema.json`.

## Сборка и публикация

```
dotnet build TweakerLuaLinker/TweakerLuaLinker.csproj
dotnet publish TweakerLuaLinker/TweakerLuaLinker.csproj -c Release -p:TwPublish=aot -o <dir>      # основной путь
dotnet publish TweakerLuaLinker/TweakerLuaLinker.csproj -c Release -p:TwPublish=single -o <dir>   # запасной
```

AOT-публикация должна проходить **без единого предупреждения** тримминга и AOT — это проверялось при
каждом изменении; новое предупреждение — повод разобраться, а не подавить. Для AOT нужен MSVC (C++-нагрузка
Visual Studio). Из окружения, где выставлена `NoDefaultCurrentDirectoryInExePath=1` (так работает агент),
линковка падает на `vswhere.exe` — снимать переменную на время публикации. Скрипты деплоя публикуют
`twllink.exe` в `/distr/TweakerUI/DevTools/Binaries/`.

`dotnet build` самого `TweakerUI` собирает компоновщик обычной (не AOT) сборкой по `ProjectReference` и
копирует её в `DevTools\Binaries\` своего выхода — так в дев-сборке Tweaker'а `twllink` лежит там же, где
в поставке (target `CopyTwllink` в `TweakerUI.csproj`, там же — почему из ссылки убран `_IsPublishing`).

## Тесты

```
dotnet test TweakerLuaLinker.Tests/TweakerLuaLinker.Tests.csproj
```

- Бандлы запускаются под `minilua.exe` из сборки LuaJIT плагина (`TweakerPlugin/build/*/luajit-gen/`); нет
  его — такие тесты пропускаются с причиной. В `minilua` нет `print`, `tostring`, `math`,
  `string.reverse`, строковой метатаблицы — фикстуры пишут через `io.write`.
- `ShippedProjectsTests` — поставляемые собранные скрипты (`TweakerPlugin/assets/scripts/`) совпадают с
  тем, что собирается из их проектов (`TweakerPlugin/assets/script-projects/`).
- `ApiDocsTests` — `api.json` против `api-reference.md` в обе стороны, актуальность `tw.def.lua`, схема
  манифеста против `ManifestReader`, таблица кодов в `authoring.md` против `DiagnosticCodes`.

## Когда меняется API `tw.*` в плагине

По порядку (он же в CLAUDE.md):

1. в прологе (`TweakerPlugin/src/lua/lua_prelude.cxx`) — если имя **добавлено**, поднять `tw.api_version`;
2. `harness/lua/ljtest` с `TW_API_UPDATE=1` — переписывает `Docs/scripting/api.json`;
3. дописать `Docs/scripting/api-reference.md`;
4. `dotnet test TweakerLuaLinker.Tests` с `TWLLINK_UPDATE_DEFS=1` — переписывает `Docs/scripting/tw.def.lua`;
5. пересобрать `twllink` (оба файла вшиты) и пересобрать поставляемые проекты, если сменилась версия
   `twllink` (`Linker.ToolVersion` печатается в строку `@bundle`).

## Новый код диагностики

Константа в `DiagnosticCodes` (и в `_all`, для предупреждения — в `_warnings`), строка в таблице
`Docs/scripting/authoring.md` — тест упадёт, пока их нет в обоих местах.

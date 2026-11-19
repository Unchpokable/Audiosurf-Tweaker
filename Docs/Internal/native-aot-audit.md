# Аудит Native AOT для .NET 10

Дата: 2026-11-16. Ветка: `native-aot`, исходный HEAD:
`f3243c63120f0a24ebf9a5a08da3f3ac98af1e43`.
На старте рабочее дерево чистое. Ветки, индекс и коммиты не менялись.

**Миграция реализована и принята владельцем 2026-11-20.** UI и twllink публикуются
Native AOT без IL-предупреждений. Deploy собирает поставку в `distr/TweakerUI`.
Актуальные результаты — в разделе «Реализация и итоговая проверка» в конце документа.
Первоначальный аудит ниже описывает состояние до изменений. UiStand и NUnit
остаются managed. Installer удалён.

## Вывод

**Переход возможен, но полноценную рабочую поставку нельзя получить только
переключением конфигураций при текущем исходном коде и зависимостях.**

Avalonia 12.0.5 не является препятствием: пробная публикация TweakerUI с Native AOT
успешно создала настоящий x64 native exe. В него вошли используемые части
TweakerCore, AudiosurfInterface и QuickPlayerCore. Однако анализатор обнаружил
несовместимые пути и риски потери метаданных. В частности, существующая библиотека
toast-уведомлений генерирует IL во время исполнения и использует встроенный COM.
Сохранение типов при trimming этого не исправляет.

`twllink` уже настроен на Native AOT; отдельная публикация и проверка трёх
Lua-шаблонов прошли. Дополнительные .NET 10 инструменты тоже требуют внимания:
AOT UiStand собирается, но падает при загрузке XAML; текущий NUnit/VSTest стек
не превращается в работающий native runner добавлением `PublishAot`.

Исходники приложения, проекты и deploy-скрипты оставлены без изменений.
Не включён по умолчанию режим поставки с известными несовместимостями.
Параметры AOT задавались только командам проверки. Изменение репозитория — этот
отчёт. Локальные логи, Lua fixtures и бинарники находятся в `Temp/native-aot/`.

## Проверенные конфигурации

| Проект / группа | Фактическая конфигурация | Значение для перехода |
|---|---|---|
| `Directory.Build.props` | Все проекты, кроме LegacyDataConverter: `net10.0-windows`, x64, latest C#, Nullable/ImplicitUsings выключены | Нельзя без разбора включать `PublishAot` глобально: здесь также Installer и тесты |
| `TweakerUI` | WinExe, `net10.0-windows10.0.19041.0`, Avalonia 12.0.5, compiled bindings | Главный кандидат на AOT; Windows TFM менять не требуется |
| `TweakerCore` | Библиотека, общие настройки; SkiaSharp 4.150.0 | Компилируется внутрь native exe потребителя; самостоятельная native DLL для неё не нужна |
| `AudiosurfInterface` | Библиотека, общие настройки; без PackageReference | Включена в пробный AOT TweakerUI; собственных IL-предупреждений в этом прогоне не обнаружено |
| `QuickPlayerCore` | Библиотека, общие настройки; TagLibSharp 2.3.0, ссылки на две предыдущие библиотеки | JSON плейлистов и reflection внутри TagLib требуют исправления/проверки |
| `TweakerLuaLinker` | Exe `twllink`, `net10.0`, win-x64, InvariantGlobalization; `PublishAot=true`, если `TwPublish != single` | Уже AOT; `TwPublish=single` оставляет управляемый fallback |
| `Installer` | Exe, наследует .NET 10; COMReference IWshRuntimeLibrary, встроенный COM в исходниках | Устаревший проект, не входит в Deploy; его нельзя считать готовым к AOT |
| `Tools/UiStand` | Exe, Windows SDK TFM, x64; Avalonia.Headless/Markup.Xaml.Loader; ссылки через HintPath на Debug DLL UI | Отдельный dev-инструмент вне solution; динамическая загрузка ресурсов и reflection мешают AOT |
| Четыре `*.Tests` | Test SDK 18.7.0, NUnit 4.6.1, NUnit3TestAdapter 6.2.0; linker-тесты переопределяют TFM на `net10.0` и RID на win-x64 | Текущий запуск через VSTest использует managed test assemblies; это отдельная задача миграции runner |
| `LegacyDataConverter` | Старый csproj, **.NET Framework 4.8.1**, ILRepack 2.0.46, System.Text.Json 10.0.9 | Сохранить без изменений, вне AOT-перехода; обязательный конвертер legacy-форматов |

Проверены все 12 csproj в отслеживаемом дереве, solution, общие props,
`App.config`, четыре Deploy-скрипта и CMake-точки входа. Отслеживаемых
`global.json`, publish profiles и общих `.targets` нет. Solution содержит также
устаревший Installer и LegacyDataConverter; это не единая dotnet-only цель.
В solution присутствуют Any CPU/x64/x86 и дополнительная конфигурация с именем
`x64`; эти mappings сами по себе не задают Native AOT.

Нативные части уже собираются CMake/MSVC: ASBridge — C++23, InjectHelper —
C++17 с обязательной проверкой x86, TweakerPlugin — C/C++23 с обязательной
проверкой x86, Ninja-пресетами x86-debug/x86-release и отдельным MSBuild build tree.
Они не становятся частью .NET AOT-компиляции. Сохранение x64 внешнего UI и x86
игровых процессов через существующий pipe/IPC не требует смены архитектуры.

`Deploy.ps1` / `.sh` сейчас публикуют TweakerUI как self-contained single-file
с self-extraction, отдельно собирают net481-конвертер полным MSBuild и отдельно
публикуют AOT twllink. `DeployDebug` публикует UI как self-contained multi-file
с символами, конвертер оставляет Release. Состав PluginPayload/DevToolsPayload
задают существующие targets `CopyTweakerPlugin` / `CopyDevTools`.

## Подтверждённые препятствия и исправления

### 1. System.Text.Json без сгенерированных контекстов

Вызовы `JsonSerializer.Serialize/Deserialize` используют reflection-overloads,
без `JsonTypeInfo` / `JsonSerializerContext`. Выданы `IL2026` и `IL3050`.
При AOT trimming включён обязательно; он по умолчанию выключает reflection-based
JSON serialization. Поэтому успешная компиляция таких вызовов не означает,
что чтение/запись данных будут работать.

Затронутые места:

- `TweakerCore/Engine/SkinPackager.cs`: чтение/запись SkinManifest;
- `TweakerCore/FolderChecker/FolderHashInfo.cs`: кеш `.hinf`;
- `TweakerCore/PluginInstall/PluginDeployManifest.cs`: состояние установки;
- `TweakerCore/Scripting/AgentSetup.cs`: LocalFile и AgentManifest;
- `QuickPlayerCore/Playlist.cs`: плейлисты;
- `TweakerUI/Models/ColorPalette.cs`: ColorPalettePrint;
- `TweakerUI/Models/PaletteDynamicLoadContainer.cs`: контейнер палитр.

Исправление: добавить source-generated JSON-контексты в соответствующих слоях,
перевести вызовы на overloads с метаданными, включить вложенные DTO/коллекции.
В Playlist заменить негeneric `JsonStringEnumConverter` на
`JsonStringEnumConverter<PlaybackMode>` и `<AdvanceTrigger>` либо эквивалентную
генерируемую конфигурацию. Сохранить имена, enum strings, defaults и совместимость
файлов; отдельно проверить выход LegacyDataConverter → новые readers.

Один флаг `JsonSerializerIsReflectionEnabledByDefault=true` не является
достаточным исправлением: он не сохраняет весь нужный граф типов и не решает
требования runtime code generation. В этой работе код сериализации не менялся.

### 2. Toast-уведомления Microsoft.Toolkit.Uwp.Notifications 7.1.3

В `TweakerUI/Core/ApplicationNotificationManager.cs` вызывается
`ToastContentBuilder.Show()`. На достижимом пути библиотека использует:

- `ToastNotificationManagerCompat.CreateActivatorType()` →
  `AssemblyBuilder.DefineDynamicAssembly`: **IL3050**, генерация кода во время работы;
- `CAppResolver` и COM marshalling: **IL3052 / IL2050**.

Это реальные ограничения Native AOT. Trimmer descriptors и сохранение сборки
не добавят JIT/Reflection.Emit или встроенный COM. WinRT в целом не запрещён:
современная C#/WinRT projection может работать с AOT; проблема — конкретная
реализация старой библиотеки уведомлений.

Исправление: заменить этот путь AOT-совместимой реализацией уведомлений со
статическим COM/WinRT interop, либо вынести его в небольшой native helper.
Вариант отказа от системных toast в пользу уже имеющихся Avalonia notifications
требует продуктового решения: это изменение поведения. Нынешний catch может
скрывать сбой и показывать in-app fallback; это не сохранение toast-функции.

### 3. Steam libraryfolders.vdf → Gameloop → Newtonsoft.Json

`TweakerUI/Core/ConfigurationManager.cs:231` превращает VDF в JSON, затем
десериализует `Dictionary<string, LibraryFoldersRecord>` через Json.NET.
Выданы `IL2026 / IL3050`; внутри Gameloop/Json.NET достижимы dynamic proxy и
reflection-пути, для Gameloop есть также `IL2070`.

Исправление: читать нужные `path`/`apps` непосредственно из статически доступного
VDF-дерева, без промежуточного JSON/dynamic; если текущий VDF parser сам требует
динамического механизма на используемом пути — заменить его trim/AOT-совместимым
parser. Альтернатива промежуточной сериализации должна использовать статические
DTO и сгенерированные JSON-метаданные. Простое обновление версии Json.NET или
подавление предупреждений не доказывает работоспособность.

### 4. ViewLocator и compiled bindings

В `TweakerUI/ViewLocator.cs` View выбирается строкой из имени ViewModel через
`Type.GetType` и `Activator.CreateInstance`. Класс уже помечен
`RequiresUnreferencedCode`; AOT-анализ выдаёт `IL2026` из `App.axaml`.
Типы и конструкторы могут быть удалены trimming.

Исправление кодом: статическое сопоставление ViewModel → View или явные
DataTemplates. Возможный вариант только конфигурацией — точечно сохранить типы,
конструкторы и необходимые метаданные Views через trimmer descriptor; его нужно
поддерживать при добавлении вкладок и проверить все переходы.

`AvaloniaUseCompiledBindingsByDefault=true` уже установлен. Явных
`ReflectionBinding` и отключений `CompileBindings` в отслеживаемом UI не найдено.
Это закрывает важную часть привязок, но не строковый ViewLocator, JSON или
сторонние библиотеки. XAML-ресурсы UI компилируются; их наличие не отменяет
отдельные риски поиска типов по строке.

### 5. System.Configuration.ConfigurationManager 10.0.9

`OpenExeConfiguration`, AppSettings и Save используются при запуске и изменении
настроек. В достижимом `System.Configuration.TypeUtil` обнаружены
`IL2067 / IL2070 / IL2026 / IL2057`: создание типов и конструкторов из строк.

Вариант только конфигурацией: сохранить фактически используемые configuration
sections, коллекции и конструкторы явным descriptor; проверить создание, чтение,
дополнение и запись `.exe.config`. Если требуется общий механизм произвольных
секций/типов, его динамическая загрузка несовместима с AOT. Более устойчивый
вариант кодом — прямое чтение/запись существующего XML appSettings либо переход
к типизированной конфигурации с миграцией настроек. Факт поломки всех AppSettings
не утверждается: здесь подтверждён анализаторный риск, runtime-проверка не проведена.

### 6. TagLibSharp 2.3.0

В `TagLib.File.Create` компилятор обнаружил `IL2072`:
`Activator.CreateInstance` выбирает конструктор формата по таблице типов.
В `PrepareExceptionForRethrow` — `IL2026` для `ObjectManager`.

Возможная правка конфигурации — сохранить конструкторы используемых форматов и
нужные метаданные TagLib. Затем проверить импорт/чтение/запись MP3, FLAC и остальных
поддерживаемых форматов, обложки и повреждённые файлы. При необходимости заменить
reflection-factory и старый путь обработки исключений в отдельной версии/fork
зависимости либо выбрать совместимую библиотеку. Само предупреждение не доказывает
поломку каждого MP3; корректность текущей библиотеки при trimming не доказана.

### 7. Build-only ProjectReference на twllink

Первая AOT-проба остановилась на **NETSDK1150**: self-contained TweakerUI видит
framework-dependent executable reference на twllink. `ReferenceOutputAssembly=false`
не исключил эту ссылку из проверки SDK 10.0.400. Существующий
`GlobalPropertiesToRemove` намеренно убирает `_IsPublishing` и `SelfContained`,
поскольку инструмент собирается отдельно.

Для исследования использован только CLI-параметр
`ValidateExecutableReferencesMatchSelfContained=false`; после него публикация дошла
до AOT и завершилась. Для постоянного решения сохранить независимую публикацию
инструмента, явно настроить проверку build-only executable reference и изоляцию
передаваемых publish-свойств, включая `PublishAot`/publish directory.
Другой вариант — отдельный MSBuild tool-build target вместо executable reference.
Самостоятельный publish twllink уже существует, объединять его с UI не требуется.

В пробном UI publish дополнительно оказались framework-dependent `twllink.exe`
и `twllink.runtimeconfig.json` в корне. Они не заменяют native инструмент в
`DevTools/Binaries`: при подготовке AOT-поставки нужно проверить также перенос
content/runtime assets build-only ссылки и исключить лишние файлы.

### 8. UiStand

Существующий стенд опубликован AOT с `UiBin`, указывающим на managed build output
Release TweakerUI. Получен native exe, но `shoot` завершился исключением:

```text
Avalonia.Markup.Xaml.XamlLoadException:
No precompiled XAML found for avares://TweakerUI/Themes/NeutralColors.axaml
```

Стенд программно создаёт ResourceInclude/StyleInclude по URI. В отличие от
include в компилируемом XAML этот путь требует сохранения assembly/resources и
метаданных; текущей конфигурации недостаточно. Дополнительно в стенде много
reflection-доступа к приватным типам, свойствам, полям и методам UI/Avalonia.
ILC сообщил также об отсутствующей System.Drawing.Common на достижимом toast-пути:
HintPath-ссылки на DLL не дают полноценный транзитивный NuGet-граф UI.

Исправление: настроить полное замыкание зависимостей и preservation для внешней
UI-сборки, либо перенести набор ресурсов/фабрики в компилируемый XAML и статические
ссылки, сократив reflection в fixtures. Сначала устранить XAML-сбой, затем проверять
каждый используемый режим. Успешной проверки изображений в этой работе нет.

### 9. NUnit / VSTest

У всех четырёх test-проектов один стек. В существующем Test SDK 18.7.0
`Microsoft.NET.Test.Sdk.Program.cs` содержит пустую точку входа:

```csharp
class AutoGeneratedProgram { static void Main(string[] args) { } }
```

Проверена AOT-публикация `AudiosurfInterface.Tests`: успешно создан native exe.
Запуск с `--list-tests` вернул 0 без вывода, **тесты не исполнялись**. VSTest
обычно загружает managed test assembly; native exe с пустым Main его не заменяет,
а trimming может удалить недостижимые тесты.

Для буквально всего .NET 10 кода нужен отдельный AOT-совместимый test runner
со статическим discovery/registration; выбранный framework/adapter надо проверить
на этом режиме. Возможная миграция на runner с генерацией discovery требует
отдельной работы и, вероятно, правок тестов. Сохранение managed NUnit для dev-тестов
плюс native smoke-тесты опубликованных exe практически допустимо, но не выполняет
буквальное требование AOT для всех test-проектов. Эта работа не меняла runner.

### 10. Installer

`dotnet build Installer ... -p:PublishAot=true` остановился на **MSB4803**:
`ResolveComReference` не поддерживается .NET-версией MSBuild. Это ограничение
текущей сборки, которое возникает до AOT-компиляции.

Даже сборка полным MSBuild не исправит `new ShellLink()` с ComImport и
`IPersistFile`: встроенный COM Native AOT не поддерживает. Также программа
использует `Assembly.Location` для путей, что несовместимо с таким размещением.
Исправление — удалить устаревший Installer в рамках предусмотренной отдельной
работы, либо модернизировать пути и interop: статические COM wrappers/генерируемый
COM interop, без COMReference/старой активации. В поставку Deploy он не входит.

## Изменения конфигураций после устранения блокеров

1. Включать `PublishAot` у конечных приложений, `IsAotCompatible` — у совместимых
   библиотек; не выпускать независимые native DLL для обычных ProjectReference.
   Пока библиотека не исправлена, использовать анализаторы как диагностику,
   не объявлять её совместимой формально.
2. В паре `Deploy.ps1/.sh` заменить single-file CoreCLR publish на AOT publish
   win-x64. Убрать не относящиеся к нему self-extraction настройки и обновить
   описание поставки; сохранить native DLL Avalonia/Skia/HarfBuzz рядом с exe.
3. Разделять обычный `dotnet build` для разработки и native publish для поставки:
   `PublishAot` включает анализатор, но native executable создаёт publish target.
   AOT не требует отказа от JIT-сборок IDE. Если нужен AOT DeployDebug, отдельно
   определить native symbols/debugger и проверить DEBUG developer tools;
   нынешний multi-file managed debugger режим имеет другое назначение.
4. Сохранить независимый AOT publish twllink, legacy-converter и существующие
   CopyTweakerPlugin/CopyDevTools. Не включать Native AOT глобально для net481,
   Installer и тестового стека.
5. Не переносить `InvariantGlobalization=true` с CLI на UI без проверки:
   локали и пользовательские данные UI имеют другие требования.
6. Не подавлять IL-предупреждения глобально и не пытаться выключить trimming:
   `PublishTrimmed=false` запрещён AOT targets. Descriptors исправляют сохранение
   метаданных, но не runtime генерацию кода и не встроенный COM.

Native AOT не означает единственный физический файл всего UI: в пробе рядом с
exe остались `av_libglesv2.dll`, `libSkiaSharp.dll`, `libHarfBuzzSharp.dll`.
Их суммарный размер с UI exe — около 53.6 МиБ, без символов и остальных инструментов.
Их упаковка/статическая линковка — отдельная задача, не условие перевода C# в AOT.

## Выполненные проверки и воспроизведение

Среда: Windows x64, .NET SDK **10.0.400**, MSBuild **18.9.6**,
ILCompiler/runtime **10.0.11**, установленный Visual Studio 18 2026 / MSVC.
Проверки публикации выполнены через `dotnet build -t:Publish`, с
`_IsPublishing=true`, то есть с реальным native publish pipeline.

Для всех путей ниже cwd — корень репозитория; сначала создать `Temp/native-aot`.
Пример основной диагностической команды PowerShell:

```powershell
$auditRoot = (Resolve-Path Temp/native-aot).Path
dotnet build TweakerUI/TweakerUI.csproj -t:Publish -c Release -r win-x64 `
    -p:_IsPublishing=true -p:PublishAot=true `
    -p:ValidateExecutableReferencesMatchSelfContained=false `
    -p:UsedAvaloniaProducts= -p:TrimmerSingleWarn=false `
    "-p:PublishDir=$auditRoot/ui/"
```

`UsedAvaloniaProducts=` — локальный обход telemetry build task: в sandbox задача
AvaloniaStats падала при записи лога вне репозитория. Сборки с native hooks/MSVC
потребовали выполнения вне sandbox; это ограничение исследовательской среды,
а не запрет AOT. Постоянные системные настройки не менялись.

| Проверка | Результат / локальный лог |
|---|---|
| UI AOT без обхода executable validation | NETSDK1150; `ui-publish.log` |
| UI AOT с обходом проверки | Exit 0; 67 предупреждений всего, включая NuGet; `ui-publish-analysis.log` |
| Проверка формата UI exe | PE machine `0x8664`, CLR header RVA = 0, размер 36 755 456 байт (~35.1 МиБ) |
| Отдельная AOT-публикация twllink | Exit 0; `twllink-publish.log`; exe 3 964 416 байт, machine `0x8664`, CLR header RVA = 0 |
| Native twllink `--version`, `--new hud/hook/empty`, сборка каждого `--project ... --json` | Все exit 0, success=true, 0 errors / 0 warnings; Lua bundle и `.lua.map` созданы только в Temp |
| AOT UiStand, UiBin → Release UI build output | Exit 0, 146 предупреждений; `uistand-publish.log` |
| Native UiStand `shoot` | Провал: XamlLoadException; `uistand-shoot.log` |
| Installer build с PublishAot | Провал: MSB4803; `installer-build.log` |
| AOT AudiosurfInterface.Tests publish | Exit 0; `nunit-publish.log`; native exe с пустым Main |
| Native tests exe `--list-tests` | Exit 0, вывода нет, тесты не запускались; `nunit-native-run.log` |
| Контрольный обычный `dotnet build TweakerUI/TweakerUI.csproj -p:UsedAvaloniaProducts=` | Exit 0, 3 предупреждения; `ui-default-build.log` |

57 различных строк IL-предупреждений финальной UI-публикации сохранены в
`ui-warnings.txt`; это 26 IL3050, 22 IL2026, 3 IL2070, 2 IL2057 и по одному
IL2050/IL2072/IL3052/IL2067. Подсчёт не включает повторение тех же строк в итогах
MSBuild и не является числом независимых багов.

NuGet выдал NU1903 для ILCompiler 10.0.11 и в части прогонов NU1900 при обращении
к vulnerability feed. Это отдельные сообщения среды/пакетов, они не скрыты и
не использованы как доказательство несовместимости приложения с AOT.

`distr/TweakerUI` перед проверками был пуст. Полный Deploy не запускался:
рабочая AOT-поставка не готова, поэтому выпускать частично проверенный bundle
не имеет смысла. Исследовательские публикации сохранены в Temp, `distr` не менялся.
UI с настоящей конфигурацией и игра не запускались, DLL в игру не устанавливались.
Снимки до/после не сравнивались: исходники/разметка не менялись, native UiStand
не прошёл старт. Прогон тестов под JIT не объявляется проверкой AOT.

После исправлений владельцу нужно проверить native UI: первый запуск и сохранение
настроек; поиск Steam/Audiosurf; все вкладки и диалоги; скины и legacy-конвертацию;
палитры; плейлисты, теги и повреждённые медиа; системные уведомления; установку
плагина, скриптовые/skybox-проекты и live IPC с игрой. Эта приёмка ещё не проведена.

## Первичные внешние источники

- [Avalonia Native AOT](https://docs.avaloniaui.net/docs/deployment/native-aot/):
  настройки приложения/библиотек, compiled XAML, сохранение reflection metadata.
- [Avalonia XAML compilation](https://docs.avaloniaui.net/docs/xaml/compilation):
  границы compiled bindings и reflection bindings.
- [Microsoft Native AOT](https://learn.microsoft.com/en-us/dotnet/core/deploying/native-aot/):
  ограничения динамической загрузки/генерации, встроенного COM и обязательный trimming.
- [System.Text.Json source generation](https://learn.microsoft.com/en-us/dotnet/standard/serialization/system-text-json/source-generation):
  reflection defaults при trimming и перегрузки с генерируемыми метаданными.
- [IL3052](https://learn.microsoft.com/en-us/dotnet/core/deploying/native-aot/warnings/il3052):
  встроенный COM и необходимость wrappers.
- [ToastNotificationManagerCompat source](https://github.com/CommunityToolkit/WindowsCommunityToolkit/blob/main/Microsoft.Toolkit.Uwp.Notifications/Toasts/Compat/ToastNotificationManagerCompat.cs):
  реализация runtime-generated COM activator; конкретные предупреждения версии 7.1.3
  подтверждены локальным ILC log.

Статус: аудит завершён; переход рабочей поставки не завершён по перечисленным
локальным причинам. Правки исходников и следующая фаза миграции не начинались.

## Решения после аудита и план миграции

Уточнение от владельца и дополнительное исследование: 2026-11-16.
Этот раздел задаёт текущую область работы. Первоначальные пункты 8–10 выше
сохраняют диагностическую ценность, но **не блокируют AOT публичной поставки**.

### Принятые решения

| Пункт аудита | Решение |
|---|---|
| 1. System.Text.Json | Согласован переход на source-generated контексты с сохранением форматов |
| 2. Toast | Полностью удалить системные toast, их настройки и Microsoft.Toolkit.Uwp.Notifications; оставить уведомления Avalonia |
| 3. VDF | Реализовать небольшой собственный reader для нужного Steam KeyValues-текста |
| 4. Views | Перейти на явные фабрики/шаблоны без reflection; рекомендуемый вариант ниже |
| 5. Настройки | Владелец явно выбрал сохранение XML и небольшой собственный сервис |
| 6. TagLib | Перейти с TagLibSharp на native TagLib через P/Invoke; helper DLL нужна только для аудиометаданных |
| 7. Сборка | Исправить конфигурацию независимых публикаций UI/tool и сборки helper |
| 8. UiStand | Остаётся managed agent harness; его AOT-публикация не требуется |
| 9. Unit-тесты | Сохранить NUnit/VSTest без AOT; дополнить проверкой реально опубликованного native приложения |
| 10. Installer | Удалить устаревший проект; не учитывать как цель AOT |

Итоговая цель: Native AOT `TweakerUI.exe` и `twllink.exe`, native x64 helper рядом
с UI, используемые managed-библиотеки компилируются внутрь native потребителей.
LegacyDataConverter остаётся обязательным net481-инструментом. Debug/JIT-сборки
для разработки, UiStand и NUnit разрешены и не являются неудавшейся миграцией.

### Предлагаемая helper DLL

Одна **x64 `TweakerNative.dll`**, C++23/CMake, с узким C ABI нужна только для
native TagLib. Имя предварительное. Это внешний компонент хоста, не часть x86
TweakerPlugin/asbridge. WinRT, toast и их регистрация в helper не входят.

P/Invoke-wrapper для TagLib располагается в QuickPlayerCore.
Модельный слой не начинает зависеть от UI. Для native ABI нужны
`extern "C"`, фиксированные размеры чисел, UTF-16 Windows paths/text, явное
владение возвращёнными данными и коды ошибок. C++ объекты, STL и исключения
через границу не передаются. Managed declarations можно выполнить через
source-generated `LibraryImport`. DLL не выполняет I/O под loader lock.

**Toast удаляются.** После опроса пользователей владелец решил отказаться от
системных уведомлений: используются штатные всплывающие уведомления Avalonia.
Ранее рассмотренный C++/WinRT helper и исследование AUMID/activation больше
не нужны. Это согласованное изменение поведения, не временный AOT fallback.

При реализации удалить `ShowUWPNotification`, выбор канала в
`ApplicationNotificationManager`, свойства и обработчики двух UWP-переключателей
в SettingViewModel/SettingsProvider, соответствующие строки SettingView и bindings
конфигурации. Ключи `UWPNotificationsAllowed` / `UWPNotificationSilent` убрать
из поставляемого App.config и считать устаревшими при сохранении существующего
пользовательского XML. Старое значение true не должно мешать показу Avalonia
notifications. Общие ShowError/ShowSuccess/ShowWarning/ShowInformation и диалоги
остаются; проверить уведомления и снимки Settings через managed UiStand.

**TagLib.** Фактическая поверхность текущего приложения небольшая:
`MetadataReader` читает title, first performer, album, duration, sample rate,
bitrate и bit depth; `TempFileTagger` меняет только title временной копии.
Сейчас поддерживаются MP3/M4A/FLAC/WAV. Обложки берутся отдельным
`LocalFileCoverArtProvider` из cover.jpg/jpeg/png: извлечение встроенных картинок
не нужно включать в эту миграцию.

Достаточно двух операций: чтение метаданных и запись title. Native
`FileRef` поддерживает чтение/изменение тегов и save. При чтении возвращается
снимок данных с одной явной операцией освобождения, которую оборачивает SafeHandle;
файловый объект не живёт до конца жизни карточки UI. Перед возвратом из записи
дескриптор файла закрывается, чтобы существующий atomic File.Move staging-копии
работал. Оригинальные аудиофайлы по-прежнему не переписываются.

Общий `AudioProperties` даёт длительность в миллисекундах, bitrate и sample rate,
но не единый bit-depth API. Внутри helper нужен форматный доступ к
FLAC/WAV/MP4 properties; это обоснованная разница API контейнеров, скрытая от C#.
Проверить first-performer semantics, пустые/Unicode-теги, VBR duration и запись
title во всех четырёх контейнерах. Существующие TempFileTaggerTests читают результат
через TagLibSharp: при миграции обновить и эти обращения, не оставлять незаметную
транзитивную managed TagLib-зависимость. Корректность сериализации контейнера
дополнительно проверять fixtures/декодером и игрой, не только новым wrapper.

Native TagLib собирается CMake, может статически линковаться в helper; её версию
и зависимости следует зафиксировать. Сборка `taglib_c` и экспорт всего C API
не нужны для двух операций. Параметры форматов/компрессии выбирать по проверенным
fixtures, не отключать поддержку, которую раньше давал TagLibSharp.
Источники: [FileRef](https://taglib.org/api/classTagLib_1_1FileRef.html),
[AudioProperties](https://taglib.org/api/classTagLib_1_1AudioProperties.html),
[MP4 properties](https://taglib.org/api/classTagLib_1_1MP4_1_1Properties.html),
[WAV properties](https://taglib.org/api/classTagLib_1_1RIFF_1_1WAV_1_1Properties.html),
[CMake TagLib](https://github.com/taglib/taglib/blob/master/INSTALL.md).
Компиляция native TagLib/helper в этой работе ещё не выполнялась.

### Reader VDF без Gameloop и Newtonsoft

Здесь нужен текстовый **KeyValues1**, используемый libraryfolders.vdf, а не все
разновидности VDF/бинарные KeyValues. Подход — небольшой tokenizer и reader
`key → string | object`, с порядком записей и повторяющимися ключами, после него
отдельный Steam-specific поиск библиотек/Audiosurf. Это заменяет текущую цепочку
VDF → JSON → Dictionary, не добавляя ещё одного сериализатора.

Reader должен понимать quoted/unquoted tokens, вложенные `{ }`, whitespace/BOM,
`//` comments и согласованные escape sequences (`\\`, `\"` и т. п.). Незнакомые
поля пропускаются корректно на любой глубине, ошибочный/оборванный ввод даёт
диагностику с позицией. Важно не превращать обычный backslash Windows path
в произвольный escape. `#include`, `#base` и conditional directives не входят
в минимальный reader; при их обнаружении нужна явная диагностика, а не
интерпретация как обычных ключей. Полноту общего Valve KeyValues не заявляем.

Современный layout `libraryfolders → numeric library → path/apps/12900`
покрывает нынешнюю задачу. Старый layout со строковыми numeric entries можно
читать тем же parser, а наличие игры проверять по кандидатному каталогу.
Это Steam-specific selection поверх общего механизма разбора, без новых
веток в tokenizer. Никакие файлы Steam не изменяются. Нужны fixtures нескольких
библиотек, escaped/Unicode paths, комментариев, отсутствующего apps, пустого и
оборванного файла. Реальный libraryfolders.vdf в этой сессии не проверен:
Steam registry path недоступен процессу исследования.

Первичный образец лексики:
[Valve KeyValues::ReadToken](https://github.com/ValveSoftware/source-sdk-2013/blob/master/src/tier1/KeyValues.cpp).
Source SDK не копируется в проект; предложена самостоятельная небольшая реализация.

### Views без изменения времени жизни

`MainWindowViewModel` уже создаёт и хранит восемь ViewModel, а команды только
выбирают `CurrentView`; `ContentControl` использует ViewLocator. Поэтому
рекомендован минимальный вариант: оставить `IDataTemplate` ViewLocator и заменить
строковый поиск на статический switch по восьми типам с `new ...View()`.
Match должен соответствовать поддерживаемым типам. Существующий DataContext
продолжает назначать Avalonia.

Это сохраняет создание нового View при переключении и существующие attach/detach
события. Кешировать все View или одновременно держать их в visual tree ради AOT
не требуется: это уже отдельное изменение жизненного цикла/памяти.
Альтернатива — восемь явных XAML DataTemplate; оба решения статические.
Новая навигационная система и перенос выбора View в ViewModel не нужны.

### XML-настройки: небольшой собственный сервис

Владелец выбрал этот вариант после сравнения. Сохраняются путь
`Environment.ProcessPath + ".config"`, существующие `<configuration><appSettings>`
и имена ключей, `SettingBinding[]`, defaults и доступ через SettingsProvider.
Заменяется только чтение/дополнение/сохранение: обычный XmlReader/XDocument,
без динамической загрузки configuration sections и без reflection-binder.
При загрузке сохраняется XML-документ, изменение касается нужных `<add>`;
посторонние секции/ключи не удаляются. Нужны сохранение через временный файл и
замену, обработка ошибок записи, повторное открытие, неполный и повреждённый XML.

Не вводятся DI container, live reload и новый формат ради небольшого набора
string/bool settings. Это выбор по простоте сопровождения и удобству сохранения,
не оптимизация частоты I/O.

Исследован и Microsoft.Extensions.Configuration: source-generated binding даёт
AOT-путь, но стандартные file providers не сохраняют изменения автоматически.
У `XmlConfigurationProvider` **даже в .NET 10.0.11** есть
`RequiresDynamicCode` / `RequiresUnreferencedCode` из-за XmlDocumentDecryptor.
Поэтому перенос на `.Xml` не был бы автоматическим устранением предупреждений.
JSON-provider читает JsonDocument, но потребовал бы переноса формата и отдельной
записи. Для текущего согласованного варианта эти зависимости не нужны.
Источники: [configuration generator](https://learn.microsoft.com/en-us/dotnet/core/extensions/configuration-generator),
[XML provider 10.0.11](https://github.com/dotnet/runtime/blob/v10.0.11/src/libraries/Microsoft.Extensions.Configuration.Xml/src/XmlConfigurationProvider.cs),
[Set изменяет Data](https://github.com/dotnet/runtime/blob/main/src/libraries/Microsoft.Extensions.Configuration/src/ConfigurationProvider.cs),
[JSON parser](https://github.com/dotnet/runtime/blob/v10.0.0/src/libraries/Microsoft.Extensions.Configuration.Json/src/JsonConfigurationFileParser.cs).

### Порядок реализации и критерии проверки

План согласован владельцем, включая отказ от toast. Реализация начата по его
прямой команде; результаты всех этапов приведены в следующем разделе.

1. **JSON и managed AOT-пути.** Ввести контексты, включая приватные DTO AgentSetup
   (доступный вложенный контекст или аккуратная организация типов, без превращения
   DTO в публичный API). Заменить VDF-chain, XML backend и ViewLocator;
   удалить системные toast и связанные настройки, оставить уведомления Avalonia.
   Проверить прежние файлы/legacy-конверсию и UI-снимки через обычный UiStand.
2. **Native helper.** Собрать x64 DLL, внедрить чтение/запись тегов.
   Tagging остаётся в существующих background tasks. Проверить метаданные
   и изменение title временной копии в поддерживаемых форматах.
3. **Зависимости и состав дерева.** После замены потребителей удалить Gameloop.Vdf,
   Gameloop.Vdf.JsonConverter, Newtonsoft.Json, Microsoft.Toolkit.Uwp.Notifications,
   System.Configuration.ConfigurationManager и больше не нужный System.Drawing.Common
   из UI, TagLibSharp — из QuickPlayerCore. Проверить тестовые/harness references.
   Удалить Installer и его solution mappings, обновить активные инструкции/README;
   исторические журналы массово не переписывать.
4. **Конфигурации.** UI/twllink публикуются независимо в AOT; исправить build-only
   reference и перенос его assets, helper — обязательный x64 native payload UI.
   Подключить его сборку/копирование к MSBuild и парным Deploy/DeployDebug так,
   чтобы обычные build/run и NUnit находили ту же DLL. Это отдельный host target,
   не CopyTweakerPlugin; существующую раскладку PluginPayload/DevTools не дублировать.
   У тестов/UiStand `PublishAot=false`, без распространения UI publish properties;
   тестовая ссылка на twllink должна использовать обычный managed build.
5. **Финальная проверка.** Реальный AOT publish и разбор всех оставшихся IL warnings,
   native smoke публичных exe и интеграции DLL, затем полный managed NUnit/VSTest
   с учётом skipped, обычный UiStand, сборка полного distr через Deploy. Managed
   тесты подтверждают логику, но не заменяют проверку trimming/PInvoke/resources
   именно опубликованного native бинаря. Работу с игрой проверяет владелец;
   уведомления Avalonia проверяются вместе с остальным UI.

Native P/Invoke требуется проверять и в JIT тестах, и в AOT-публикации: отключение
AOT для NUnit не означает отключение native helper. Не включать AOT глобально
в Directory.Build.props с последующим подавлением ошибок тестов.

Статус уточнения: план реализован. XML-настройки сохранены, системные toast
удалены; native helper содержит только работу с TagLib.

## Реализация и итоговая проверка

2026-11-16–17. Все согласованные этапы выполнены в ветке `native-aot`.
На этапе реализации индекс и игровая установка не менялись.

### Что изменено

| Область | Реализация |
|---|---|
| JSON | CoreJsonContext, вложенный private AgentJsonContext, PlayerJsonContext и PaletteJsonContext; все прежние reflection-overloads заменены явными JsonTypeInfo. Enum converters стали generic. В UI reflection JSON отключён и при обычной сборке |
| Настройки | XmlSettingsStore в TweakerCore.Configuration и прежний SettingBinding[] в UI. Путь `Environment.ProcessPath + ".config"`, старые ключи и defaults сохранены; неизвестные ключи, секции и комментарии остаются. Сохранение через уникальный temporary file, flush и замену |
| VDF | Собственный reader Steam KeyValues1: вложенные объекты, quoted/unquoted tokens, BOM, комментарии, escaped/Unicode paths, дубли ключей. Современный apps index и старые строковые библиотеки. Ошибки с позицией, лимит глубины; directives/conditions явно отклоняются |
| ViewLocator | Статический switch и согласованный Match по восьми ViewModel. Новый View при переключении, DataContext по-прежнему назначает Avalonia |
| Уведомления | Удалены Windows toast, два переключателя, обработчики и Toolkit package. Старые UWP-ключи удаляются при дополнении/сохранении config; Avalonia host получает все Show-вызовы |
| Аудиометаданные | Native TagLib через x64 TweakerNative.dll, LibraryImport и SafeHandle в QuickPlayerCore. Чтение snapshot и запись title staging-копии; существующие background tasks и atomic publish копий сохранены |
| Зависимости | Удалены Gameloop.Vdf, его JsonConverter, Newtonsoft.Json, UWP Toolkit, System.Configuration.ConfigurationManager, System.Drawing.Common из UI и TagLibSharp из QuickPlayerCore. Тесты тегирования больше не используют TagLibSharp |
| Публикация | UI и twllink имеют PublishAot=true; IlcTreatWarningsAsErrors и ILLinkTreatWarningsAsErrors включены. NUnit и UiStand явно managed. У build-only ссылки UI → twllink изолированы publish properties, Private=false и ShouldBeValidatedAsExecutableReference=false; глобального обхода NETSDK1150 нет |
| Payload | TweakerNative.targets добавляет DLL и лицензии как transitive content assets QuickPlayerCore; это работает в build, тестах и publish. App.config копируется под именем TweakerUI.exe.config. Лишнего twllink.exe в корне UI-публикации нет |
| Deploy | Обе пары ps1/sh синхронизированы: release UI — AOT, Debug UI — managed self-contained с PDB, twllink в обоих случаях AOT. Bash-файлы закреплены как LF в .gitattributes и проверены через bash -n |
| Installer | Удалены исходники проекта и solution mappings, обновлены активные инструкции/README. Исторические журналы сохранены |

Helper собирается CMake/C++23 с MSVC static runtime. Статически включены
[TagLib 2.3.2](https://taglib.org/) с bundled utfcpp 4.2.0 и
[zlib 1.4](https://zlib.net/); официальные архивы закреплены SHA-256.
Поддержка compressed ID3 через zlib сохранена, внешние исходники не изменялись.
Лицензии копируются в `Licenses/`. Отдельные TagLib/zlib/VC runtime DLL не нужны.
Контракт и сборка описаны в `TweakerNative/README.md`.

Форматы скинов, палитр, playlist и служебных JSON не менялись. Private DTO
AgentSetup не стали публичным API. XML с повреждённой структурой или external
appSettings (`file`/`configSource`) даёт ошибку через прежний callback и не
перезаписывается; динамическую загрузку XML section handlers сервис не выполняет.

### Проверки

| Проверка | Результат |
|---|---|
| Обычный dotnet build TweakerUI | Успех, managed development build, обязательные native hooks выполнены |
| TweakerCore.Tests | 300 passed, 0 failed, 0 skipped |
| QuickPlayerCore.Tests | 148 passed, 0 failed, 0 skipped |
| AudiosurfInterface.Tests | 26 passed, 0 failed, 0 skipped |
| TweakerLuaLinker.Tests | 166 passed, 0 failed, 0 skipped; Lua/minilua проверки доступны |
| Native AOT UI publish | Успех, 0 ILLink/ILCompiler warnings, без suppressions |
| Native AOT twllink publish | Успех, 0 ILLink/ILCompiler warnings; опубликованный exe создал/собрал hud, hook, empty, 0 diagnostics |
| DeployDebug.ps1 | Полный managed debug bundle, 353 файла, около 295,8 МиБ, helper/лицензии/конвертер присутствуют |
| Deploy.ps1 | Полный release bundle в distr/TweakerUI, 119 файлов, около 74,5 МиБ |
| Bash counterparts | Оба прошли bash -n; полный bundle выполнялся PowerShell-скриптами |
| UiStand shoot/compare | 88 снимков, 86 полностью совпадают. В двух Settings изменена только полоса прокрутки после удаления карточки; дополнительно просмотрены полные Settings до/после в обеих темах |
| Native smoke harness | Реально опубликован AOT, RuntimeFeature.IsDynamicCodeSupported=false, JSON reflection disabled. Успешны все новые JSON contexts, XML/VDF, native metadata/tagging и загрузка/отрисовка восьми Views с настоящим App.axaml |
| Legacy compatibility | Собственные BinaryFormatter fixtures .tasp/.palette/.pltc преобразованы неизменённым LegacyDataConverter и прочитаны новым AOT-кодом |
| Независимая проверка аудио | FFprobe подтвердил title/artist/album после записи title в MP3/M4A/FLAC/WAV; FFmpeg дал одинаковые SHA-256 декодированного PCM до/после |

Всего **640 NUnit-тестов**, пропусков нет. Новые проверки включают XML reopen,
неполный XML, сохранение посторонних секций/комментариев, отказ замены readonly-файла
без потери исходника и temporary-мусора; современный/старый VDF, ошибки и unsupported
directives; Unicode audio paths/tags, VBR MP3, сохранность остальных метаданных,
отказ на неаудиофайле и закрытие native file handles перед File.Move.

Отдельно сравнены first-performer результаты с прежним TagLibSharp 2.3.0 во всех
четырёх форматах. Выявлено и исправлено отличие M4A: прежний
[AppleTag.GetText](https://github.com/mono/taglib-sharp/blob/main/src/TaglibSharp/Mpeg4/AppleTag.cs)
разделял исполнителей внутри значения по `;`, native TagLib сохраняет эту строку.
Адаптер MP4 воспроизводит выбор первого исполнителя; для других контейнеров
literal semicolons не разделяются. Добавлен синтетический M4A fixture и regression
test; сравнение старой/новой реализации и AOT smoke также проходят.
TagLibSharp использовался только во временном сравнительном tool под Temp,
зависимость не возвращалась в приложение или его NUnit-проекты.

Для NUnit этой среды потребовались только process-local параметры:

```powershell
$env:TEMP = (Resolve-Path Temp/native-aot/test-temp).Path
$env:TMP = $env:TEMP
dotnet test <project.csproj> -p:_MSTestEnableParentProcessQuery=false
```

Без них testhost не мог наблюдать родительский процесс (Win32 access denied),
а стандартный Temp был недоступен. Свойства проекта ради среды не менялись.
PowerShell deploy запускался с `-ExecutionPolicy Bypass` только у дочернего процесса;
`NoDefaultCurrentDirectoryInExePath` временно снимался с восстановлением в finally.
Обычные .NET/CMake команды запускались вне sandbox там, где он мешал MSVC/testhost.

Логи, native smoke project и снимки оставлены в игнорируемом `Temp/native-aot/`:
`deploy-release.log`, `deploy-debug.log`, `final-*.log`, `runtime-smoke-build.log`,
`runtime-smoke-run.log`, `runtime-smoke-final-bundle.log`, `tag-comparison-run.log`, `independent-audio.json`, `bundle-pe.json`, `shots-before/`,
`shots-final/`, `Settings-{before,after}-{light,dark}.png`.
Smoke использует headless platform, не создаёт MainWindowViewModel и не запускает
игровые службы; это проверка AOT ресурсов/логики, не полноценный запуск игрового клиента.

### Итоговые бинарники

| Файл в release bundle | PE / CLR | Размер, байт |
|---|---|---:|
| TweakerUI.exe | x64, CLR header RVA=0 | 30 917 632 |
| DevTools/Binaries/twllink.exe | x64, CLR header RVA=0 | 3 964 416 |
| TweakerNative.dll | x64, CLR header RVA=0 | 916 480 |
| LegacyDataConverter.exe | managed .NET Framework, CLR header присутствует | 1 164 800 |

CoreCLR/runtimeconfig/managed UI assemblies в release нет. Native Avalonia,
SkiaSharp и HarfBuzz DLL остаются рядом с exe. Первоначальная AOT-проба UI
до удаления зависимостей была 36 755 456 байт; итоговый exe — около 29,5 МиБ.

### Оставшиеся замечания и приёмка владельца

- SDK 10.0.400 выбирает Microsoft.DotNet.ILCompiler 10.0.11: NuGet по-прежнему
  сообщает NU1903 ([advisory из вывода NuGet](https://github.com/advisories/GHSA-p3jp-9v83-76w9)).
  Предупреждение не подавлялось; обновление установленного SDK/runtime patch
  остаётся отдельным действием окружения. В отдельных restore был NU1900 о
  недоступном vulnerability feed. Это не IL-предупреждения совместимости AOT.
- Остался прежний CS4014 в App.axaml.cs и прежние ILRepack duplicate-resource
  сообщения LegacyDataConverter. Их код в этой работе не менялся.
- Обнаружено отдельное расхождение с архитектурным описанием: существующий
  BuildAsBridge конфигурирует CMake без -A, поэтому в этой среде получен x64
  asbridge.exe. InjectHelper и TweakerPlugin — x86. Этот target и его исходники
  миграция не меняла; это не следствие Native AOT и требует отдельного решения
  по требуемой разрядности IPC-моста.
- Офлайн-проверки не охватывали реальный Steam libraryfolders.vdf, работу с игрой
  и полноценный запуск опубликованного TweakerUI с игровыми службами.

Чек-лист для ручной проверки release: запуск и восемь вкладок,
перезапуск с сохранёнными XML-настройками/темой, Avalonia notifications,
импорт своих legacy скинов и палитр, Quick Player с MP3/M4A/FLAC/WAV и title tags,
связь с игрой и работу установленного плагина. Развёртывание DLL в игру — владельцем.

2026-11-20. Владелец проверил Native AOT сборку и просмотрел код. Регрессий и
ошибок в проверенных сценариях он не обнаружил и разрешил перенос в `Revive`.
Отдельных результатов по пунктам чек-листа нет.

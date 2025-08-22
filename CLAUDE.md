# Claude Code Instructions - WPF .NET 9 Desktop Utility

## Project Overview
This is a .NET 9 WPF desktop utility for managing game resource packages. The application focuses on file operations, archive handling, image processing, and basic Windows process interaction using the MVVM pattern with CommunityToolkit.Mvvm.

## Architecture & Patterns

### MVVM Implementation
- Use classic MVVM pattern for all UI-related logic
- ViewModels handle all business logic and UI state management
- Models represent data structures and business entities
- Views contain only UI-specific code and data binding

### Project Structure
```
/Views/          - XAML views and code-behind
/ViewModels/     - MVVM ViewModels with CommunityToolkit.Mvvm
/Models/         - Data models and business entities
/Core/           - Core business logic and utilities
/Themes/         - Custom ResourceDictionaries and styles
/Controls/       - Custom UserControls
/Forms/          - Dialog windows and forms
```

## Technology Stack

### Core Framework
- **Target**: .NET 9, Windows Desktop
- **UI Framework**: WPF with CommunityToolkit.Mvvm
- **Icons**: MaterialDesignInXAML (icons ONLY, no controls/styles)
- **Configuration**: appsettings.json

### Key Dependencies
```xml
<PackageReference Include="CommunityToolkit.Mvvm" />
<PackageReference Include="MaterialDesignThemes" />
<PackageReference Include="Microsoft.Extensions.Configuration.Json" />
```

## Coding Standards

### Naming Conventions
- **Private fields**: `_underscoredCamelCase`
- **ViewModels**: `...ViewModel` suffix
- **Models**: `...Model` suffix
- **Public members**: `PascalCase`
- **Commands/DependencyProperties**: Use CommunityToolkit.Mvvm source generators (no special naming required)

### Class Structure Example
```csharp
public partial class MainViewModel : ObservableObject
{
    private string _currentFilePath;
    private bool _isProcessing;
    
    [ObservableProperty]
    private string displayText;
    
    [RelayCommand]
    private async Task ProcessFileAsync()
    {
        // Implementation
    }
}
```

## Error Handling Strategy

### Exception Philosophy
- **User code**: Avoid throwing exceptions except in truly exceptional cases
- **Library exceptions**: Use try-catch blocks to convert to Result<T>
- **Error propagation**: Use Result<T> pattern instead of exceptions
- **UI feedback**: ViewModels handle Result<T> and provide user feedback

### Result<T> Pattern
The project uses a custom Result<T> pattern for error handling:

```csharp
namespace Tweaker.Core;

public record Error(string Message, string? Code = null, Exception? InnerException = null);

public abstract record Result<T>;
public sealed record Success<T>(T Value) : Result<T>;
public sealed record Failure<T>(Error Value) : Result<T>;

public static class Impl
{
    public static Result<T> ToFailure<T>(this string message) => new Failure<T>(new Error(message));
    public static Result<T> ToFailure<T>(this string message, string code) => new Failure<T>(new Error(message, code));
    public static Result<T> ToFailure<T>(this string message, string code, Exception exception)
        => new Failure<T>(new Error(message, code, exception));

    public static Result<T> ToSuccess<T>(this T value) => new Success<T>(value);

    public static Result<TOut> Map<TIn, TOut>(
        this Result<TIn> result, Func<TIn, TOut> mapper)
    {
        return result switch
        {
            Success<TIn>(var value) => new Success<TOut>(mapper(value)),
            Failure<TIn>(var error) => new Failure<TOut>(error),
            _ => throw new InvalidOperationException()
        };
    }

    public static Result<TOut> Bind<TIn, TOut>(
        this Result<TIn> result, Func<TIn, Result<TOut>> binder)
    {
        return result switch
        {
            Success<TIn>(var value) => binder(value),
            Failure<TIn>(var error) => new Failure<TOut>(error),
            _ => throw new InvalidOperationException()
        };
    }

    public static TResult Match<T, TResult>(
        this Result<T> result, Func<T, TResult> onSuccess, Func<Error, TResult> onError)
    {
        return result switch
        {
            Success<T>(var value) => onSuccess(value),
            Failure<T>(var error) => onError(error),
            _ => throw new InvalidOperationException(),
        };
    }
}
```

### Error Handling Pattern
**Always use Result<T> for operations that can fail:**

```csharp
public async Task<Result<string>> ProcessFileAsync(string filePath)
{
    try
    {
        // Library calls that might throw
        var content = await File.ReadAllTextAsync(filePath);
        var processedContent = ProcessContent(content);
        
        return new Success<string>(processedContent);
    }
    catch (FileNotFoundException ex)
    {
        return "File not found".ToFailure<string>("FILE_NOT_FOUND", ex);
    }
    catch (UnauthorizedAccessException ex)
    {
        return "Access denied".ToFailure<string>("ACCESS_DENIED", ex);
    }
    catch (Exception ex)
    {
        return "Unexpected error occurred".ToFailure<string>("UNEXPECTED_ERROR", ex);
    }
}

// Usage in ViewModels
[RelayCommand]
private async Task LoadFileAsync()
{
    var result = await ProcessFileAsync(_selectedFilePath);
    
    result.Match(
        onSuccess: content => 
        {
            FileContent = content;
            StatusMessage = "File loaded successfully";
        },
        onError: error =>
        {
            FileContent = string.Empty;
            StatusMessage = $"Error: {error.Message}";
            // Log error details if needed
        }
    );
}
```

## Asynchronous Programming

### Async/Await Guidelines
- Use `async`/`await` for any long-running operations
- Use `Task` mechanism for background operations
- Return results to Dependency Properties for UI binding
- Always use `ConfigureAwait(false)` in non-UI code

### Background Task Pattern
```csharp
[RelayCommand]
private async Task LoadDataAsync()
{
    IsLoading = true;
    
    var result = await Task.Run(async () => 
    {
        // Background work
        return await ProcessLargeFileAsync();
    }).ConfigureAwait(false);
    
    // Update UI on UI thread
    await Application.Current.Dispatcher.InvokeAsync(() =>
    {
        ProcessedData = result;
        IsLoading = false;
    });
}
```

## UI Guidelines

### Styling
- **NEVER** use MaterialDesignInXAML controls or styles
- **ALWAYS** use custom styles from `Themes/` ResourceDictionaries
- **ONLY** use MaterialDesignInXAML for icons: `<materialDesign:PackIcon Kind="..." />`

### ResourceDictionary Structure
```xml
<ResourceDictionary xmlns="http://schemas.microsoft.com/winfx/2006/xaml/presentation">
    <!-- Custom button styles -->
    <Style x:Key="PrimaryButton" TargetType="Button">
        <!-- Custom implementation -->
    </Style>
</ResourceDictionary>
```

### Data Binding
- Use `{Binding}` with proper PropertyChanged notifications
- Leverage CommunityToolkit.Mvvm's `[ObservableProperty]` attribute
- Use `RelayCommand` for command binding

## File Operations

### Core Functionality Areas
- File system operations (copy, move, delete, rename)
- Archive handling (zip, rar, custom formats)
- Image processing and manipulation
- Windows process interaction

### File Operation Pattern
```csharp
public async Task<OperationResult> ExtractArchiveAsync(string archivePath, string destinationPath)
{
    try
    {
        IsProcessing = true;
        
        var result = await Task.Run(() =>
        {
            // Archive extraction logic
            return ExtractArchiveCore(archivePath, destinationPath);
        });
        
        return result;
    }
    catch (Exception ex)
    {
        // Log error, return appropriate error code
        return OperationResult.ExtractionFailed;
    }
    finally
    {
        IsProcessing = false;
    }
}
```

## Configuration Management

### appsettings.json Structure
```json
{
  "AppSettings": {
    "DefaultGamePath": "",
    "MaxConcurrentOperations": 4,
    "TempDirectory": "%TEMP%\\GameResourceManager"
  },
  "FileFormats": {
    "SupportedArchives": [".zip", ".rar", ".7z"],
    "SupportedImages": [".png", ".jpg", ".bmp", ".dds"]
  }
}
```

### Configuration Loading
```csharp
public class ConfigurationService
{
    private readonly IConfiguration _configuration;
    
    public ConfigurationService()
    {
        _configuration = new ConfigurationBuilder()
            .AddJsonFile("appsettings.json", optional: false)
            .Build();
    }
    
    public T GetSection<T>(string sectionName) where T : new()
    {
        var section = new T();
        _configuration.GetSection(sectionName).Bind(section);
        return section;
    }
}
```

## Best Practices

### ViewModel Responsibilities
- Manage UI state and user interactions
- Coordinate between Models and Views
- Handle error codes and provide user feedback
- Execute commands and background operations

### Model Responsibilities
- Represent business data and entities
- Contain validation logic
- Implement INotifyPropertyChanged when needed

### View Responsibilities
- Display data through data binding
- Handle user input via commands
- Contain minimal code-behind (ideally none)

### Performance Considerations
- Use `ObservableCollection<T>` for dynamic lists
- Implement virtualization for large datasets
- Use background threads for I/O operations
- Cache frequently accessed data

## Common Patterns

### Progress Reporting
```csharp
[ObservableProperty]
private double progressValue;

[ObservableProperty]
private string progressText;

private async Task ProcessWithProgressAsync()
{
    var progress = new Progress<(double value, string text)>(p =>
    {
        ProgressValue = p.value;
        ProgressText = p.text;
    });
    
    await ProcessFileWithProgressAsync(progress);
}
```

### Dialog Interaction
```csharp
[RelayCommand]
private async Task OpenFileAsync()
{
    var dialog = new OpenFileDialog
    {
        Filter = "Archive files (*.zip;*.rar)|*.zip;*.rar"
    };
    
    if (dialog.ShowDialog() == true)
    {
        var result = await LoadFileAsync(dialog.FileName);
        HandleLoadResult(result);
    }
}
```

## Dependencies and NuGet Packages

Always include these core packages:
- `CommunityToolkit.Mvvm` - MVVM helpers and source generators
- `MaterialDesignThemes` - For icons only
- `Microsoft.Extensions.Configuration.Json` - Configuration management

When working with files and archives, suggest appropriate packages:
- `System.IO.Compression` - Built-in ZIP support
- `SharpCompress` - Multi-format archive support
- `ImageSharp` - Image processing

Remember: No dependency injection container is used - instantiate services directly or use static classes where appropriate.
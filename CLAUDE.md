# Claude Code Instructions - Qt6/QML Desktop Utility

## Project Overview
This is a Qt6/QML desktop utility for managing game resource packages. The application focuses on file operations, archive handling, image processing, and basic process interaction using a hybrid architecture with QML frontend and C++ backend.

## Architecture & Organization

### Project Structure
The project is organized as multiple CMake subprojects:

```
QTweaker/                 - Main project root
├── Backend/              - Qt bindings for QML-Core integration
├── Core/                 - Internal application logic (Qt-independent)
├── NativeImage/          - STB wrapper C library
└── [other modules]/      - Additional CMake subprojects as needed
```

### Architectural Philosophy

#### Core Module ("C with Classes")
- **Classes**: Only for state encapsulation, NO inheritance trees or virtual functions
- **Polymorphism**: Static polymorphism via templates and free template functions
- **Structures**: Preferred for simple data without complex state management
- **Qt Usage**: Can use Qt Core classes (QString, QDataStream, etc.) when they simplify logic and provide better API than STL equivalents

#### Backend Module (Qt-Idiomatic)
- **QML Integration**: Each QML component should have a corresponding backend object
- **Qt Framework Integration**: Use Qt patterns (QObject, signals/slots, Q_PROPERTY) for optimal QML integration
- **Architecture Pattern**: QML-focused variant of MVVM where backend objects bridge QML and Core

### Technology Stack

#### Core Framework
- **Target**: Qt6 (QtCore, QtGui), C++20
- **UI**: Pure QML (no Qt Widgets, no Qt Designer)
- **Build System**: CMake (primary)
- **External Libraries**: Currently none (STB via NativeImage wrapper)
- **Core Dependencies**: Qt Core classes allowed when they provide superior API (QDataStream, QString, etc.)

## Coding Standards

### Naming Conventions
- **enum/enum class/struct/class**: `PascalCase`
- **Functions/methods/variables/parameters**: `snake_case`
- **Private class members**: `m_snake_case`

### Code Organization Patterns

#### Core Module Example
```cpp
// Prefer structures and template functions
struct FileProcessor 
{
    QString input_path;  // Qt classes allowed for better API
    QByteArray buffer;   // More convenient than std::vector<uint8_t>
};

template<typename Processor>
Error process_file(Processor& processor, const QString& path)
{
    // Template-based static polymorphism
    return processor.process_data(path);
}

// Classes only for state encapsulation, no inheritance
class BinaryDataSerializer 
{
public:
    Error save_data(const QString& file_path, const CustomDataFormat& data);
    Error load_data(const QString& file_path, CustomDataFormat& data);

private:
    QDataStream m_stream;  // Qt classes for simpler API
    QByteArray m_buffer;
    // No virtual functions, no inheritance
};

// Example using QDataStream for binary serialization
Error BinaryDataSerializer::save_data(const QString& file_path, const CustomDataFormat& data) 
{
    QFile file(file_path);
    if (!file.open(QIODevice::WriteOnly)) 
    {
        return make_error(Rank::Severe, "Cannot open file", 
                         "Failed to open '{}' for writing", file_path.toStdString());
    }
    
    QDataStream stream(&file);
    stream.setByteOrder(QDataStream::LittleEndian);
    
    // Much cleaner than std::ofstream for binary data
    stream << data.version << data.header << data.payload;
    
    return Error{}; // Success
}
```

#### Backend Module Example
```cpp
// Qt-idiomatic approach for QML integration
class FileProcessorBackend : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString currentFile READ currentFile NOTIFY currentFileChanged)
    Q_PROPERTY(bool isProcessing READ isProcessing NOTIFY isProcessingChanged)

public:
    explicit FileProcessorBackend(QObject* parent = nullptr);
    
    QString currentFile() const { return m_current_file; }
    bool isProcessing() const { return m_is_processing; }

public slots:
    void processFile(const QString& filePath);

signals:
    void currentFileChanged();
    void isProcessingChanged();
    void processingCompleted(bool success, const QString& errorMessage);

private:
    QString m_current_file;
    bool m_is_processing = false;
    
    // Bridge to Core module
    std::unique_ptr<core::FileProcessor> m_core_processor;
};
```

## Error Handling Strategy

### Error Philosophy
- **Non-critical errors**: Return `core::Error` struct with error information
- **Critical errors**: Use `panic()` function to throw exceptions
- **External API**: Use try-catch blocks only for external libraries that throw exceptions
- **User code**: Never throws exceptions (except via `panic()` for unrecoverable errors)

### Error Structure Usage
```cpp
namespace core 
{
    // Use the provided Error struct and utility functions
    Error process_archive(const std::string& path) 
    {
        if (!std::filesystem::exists(path)) 
        {
            return make_error(Rank::Severe, "File not found", 
                             "Archive file '{}' does not exist", path);
        }
        
        try 
        {
            // External library call that might throw
            auto result = external_lib::extract(path);
            return Error{}; // Success (empty Error indicates no error)
        }
        catch (const external_lib::exception& e) 
        {
            return make_error(Rank::Moderate, "Extraction failed", 
                             "External library error: {}", e.what());
        }
    }
    
    // For unrecoverable errors
    void critical_operation() 
    {
        auto error = validate_system_state();
        if (error.error_rank == Rank::Fatal) 
        {
            panic<std::runtime_error>(error);
        }
    }
}
```

### Backend Error Handling
```cpp
// Convert Core errors to QML-friendly signals
void FileProcessorBackend::processFile(const QString& filePath) 
{
    m_is_processing = true;
    emit isProcessingChanged();
    
    auto std_path = filePath.toStdString();
    auto result = m_core_processor->process(std_path);
    
    m_is_processing = false;
    emit isProcessingChanged();
    
    if (result.error_rank != Rank::Trivial) // Non-empty error
    {
        emit processingCompleted(false, result.short_description);
    } 
    else 
    {
        emit processingCompleted(true, QString());
    }
}
```

## Asynchronous Programming

### Async Strategy
- **Simple operations**: Use `std::async` for straightforward cases
- **Qt integration**: Use `QFuture`/`QtConcurrent` when better Qt integration needed
- **QML updates**: Always emit signals on main thread for QML property updates

### Async Patterns
```cpp
// Backend async operation with QFuture
void FileProcessorBackend::processLargeFileAsync(const QString& filePath) 
{
    auto future = QtConcurrent::run([this, filePath]() -> core::Error {
        auto std_path = filePath.toStdString();
        return m_core_processor->process_large_file(std_path);
    });

    auto watcher = new QFutureWatcher<core::Error>(this);
    connect(watcher, &QFutureWatcher<core::Error>::finished, [this, watcher]() {
        auto result = watcher->result();
        emit processingCompleted(result.error_rank == Rank::Trivial, 
                               result.short_description);
        watcher->deleteLater();
    });
    
    watcher->setFuture(future);
}

// Core module with std::async
namespace core 
{
    std::future<Error> process_file_async(const std::string& path) 
    {
        return std::async(std::launch::async, [path]() -> Error {
            return process_file_sync(path);
        });
    }
}
```

## QML Integration Guidelines

### Backend Registration
```cpp
// main.cpp
#include <QQmlApplicationEngine>
#include <QGuiApplication>

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    
    // Register backend types for QML
    qmlRegisterType<FileProcessorBackend>("Tweaker.Backend", 1, 0, "FileProcessor");
    qmlRegisterType<ArchiveManagerBackend>("Tweaker.Backend", 1, 0, "ArchiveManager");
    
    QQmlApplicationEngine engine;
    engine.load(QUrl(QStringLiteral("qrc:/main.qml")));
    
    return app.exec();
}
```

### QML Usage Pattern
```qml
// FileProcessorView.qml
import QtQuick 2.15
import Tweaker.Backend 1.0

Item {
    FileProcessor {
        id: fileProcessor
        
        onProcessingCompleted: function(success, errorMessage) {
            if (success) {
                statusText.text = "Processing completed successfully"
            } else {
                statusText.text = "Error: " + errorMessage
            }
        }
    }
    
    Button {
        text: "Process File"
        enabled: !fileProcessor.isProcessing
        onClicked: fileProcessor.processFile(selectedFilePath)
    }
    
    Text {
        id: statusText
        text: fileProcessor.isProcessing ? "Processing..." : "Ready"
    }
}
```

## Best Practices

### Core Module Guidelines
- Use Qt Core classes when they provide superior API (QDataStream vs std::ofstream for binary data)
- Architectural constraint: NO class inheritance hierarchies or virtual functions
- Use templates liberally for static polymorphism
- Prefer composition over inheritance
- Use free functions when possible
- Return `core::Error` for all operations that can fail

### Backend Module Guidelines
- One backend class per major QML component
- Use Q_PROPERTY for all data that QML needs to access
- Emit signals for all state changes QML should know about
- Keep backend objects lightweight - delegate heavy work to Core
- Use Qt's threading facilities when integrating with QML

### Performance Considerations
- Core operations should be efficient and minimal
- Use move semantics extensively in Core
- Consider object pooling for frequently created/destroyed objects
- Profile before optimizing, especially Core/Backend boundary

### Error Handling Best Practices
- Always check `core::Error` return values in Backend
- Convert Core errors to user-friendly messages for QML
- Use appropriate `Rank` levels for different error severities
- Log detailed error information while showing simplified messages to users

## File Operations Patterns

### Core File Processing
```cpp
namespace core 
{
    struct FileMetadata 
    {
        QString path;     // Qt types for better string handling
        qint64 size;      // Qt integer types
        QDateTime modified_time;  // More convenient than std::chrono
    };
    
    template<typename Handler>
    Error process_files_batch(const QStringList& paths, Handler&& handler) 
    {
        for (const auto& path : paths) 
        {
            auto result = handler(path);
            if (result.error_rank > Rank::Minor) 
            {
                return result;
            }
        }
        return Error{}; // Success
    }
    
    // Example of using Qt classes for cleaner API
    class ConfigurationManager 
    {
    public:
        Error save_config(const QString& file_path, const QJsonObject& config);
        Error load_config(const QString& file_path, QJsonObject& config);
        
    private:
        QSettings m_settings;  // Qt provides better config handling than raw file I/O
        // No inheritance, no virtual functions
    };
}
```

### Backend File Operations
```cpp
class FileManagerBackend : public QObject 
{
    Q_OBJECT
    Q_PROPERTY(QStringList recentFiles READ recentFiles NOTIFY recentFilesChanged)
    
public slots:
    void processFiles(const QStringList& files);
    
signals:
    void fileProcessed(const QString& fileName, bool success);
    void allFilesProcessed();

private:
    QStringList m_recent_files;
};
```

Remember: The goal is clean separation between Qt-agnostic Core logic and Qt-specific Backend integration, with QML as the presentation layer.
using Microsoft.Extensions.Configuration;
using Tweaker.Core.Errors;
using System.IO;

namespace Tweaker.Settings;

public enum ConfigurationState
{
    Valid,
    Invalid
}

public interface IConfigurationService
{
    ConfigurationState State { get; }
    Result<T> GetSection<T>(string sectionName) where T : class, new();
    Result<string> GetValue(string key);
    Result<T> GetValue<T>(string key);
}

public class ConfigurationService : IConfigurationService
{
    private IConfiguration? _configuration;
    private Error? _initializationError;

    public ConfigurationState State { get; private set; }

    public ConfigurationService()
    {
        var configurationResult = LoadConfiguration();

        configurationResult.Match(
            onSuccess: config =>
            {
                _configuration = config;
                State = ConfigurationState.Valid;
                return true;
            },
            onError: error =>
            {
                _initializationError = error;
                State = ConfigurationState.Invalid;
                return false;
            }
        );
    }

    private Result<IConfiguration> LoadConfiguration()
    {
        try
        {
            var basePath = AppDomain.CurrentDomain.BaseDirectory;
            var configPath = Path.Combine(basePath, "appsettings.json");

            if (!File.Exists(configPath))
            {
                return "Configuration file 'appsettings.json' not found".ToFailure<IConfiguration>("CONFIG_FILE_NOT_FOUND");
            }

            var configuration = new ConfigurationBuilder()
                .SetBasePath(basePath)
                .AddJsonFile("appsettings.json", optional: false, reloadOnChange: true)
                .Build();

            return ((IConfiguration)configuration).ToSuccess();
        }
        catch (Exception ex)
        {
            return "Failed to load configuration file".ToFailure<IConfiguration>("CONFIG_LOAD_ERROR", ex);
        }
    }

    public Result<T> GetSection<T>(string sectionName) where T : class, new()
    {
        if (State == ConfigurationState.Invalid)
        {
            return _initializationError != null
                ? new Failure<T>(_initializationError)
                : "Configuration is in invalid state".ToFailure<T>("INVALID_STATE");
        }

        try
        {
            var section = new T();
            var configSection = _configuration!.GetSection(sectionName);

            if (!configSection.Exists())
            {
                return $"Configuration section '{sectionName}' not found".ToFailure<T>("SECTION_NOT_FOUND");
            }

            configSection.Bind(section);
            return section.ToSuccess();
        }
        catch (Exception ex)
        {
            return $"Failed to bind configuration section '{sectionName}'".ToFailure<T>("BINDING_ERROR", ex);
        }
    }

    public Result<string> GetValue(string key)
    {
        if (State == ConfigurationState.Invalid)
        {
            return _initializationError != null
                ? new Failure<string>(_initializationError)
                : "Configuration is in invalid state".ToFailure<string>("INVALID_STATE");
        }

        try
        {
            var value = _configuration![key];
            if (value == null)
            {
                return $"Configuration key '{key}' not found".ToFailure<string>("KEY_NOT_FOUND");
            }

            return value.ToSuccess();
        }
        catch (Exception ex)
        {
            return $"Failed to get configuration value for key '{key}'".ToFailure<string>("GET_VALUE_ERROR", ex);
        }
    }

    public Result<T> GetValue<T>(string key)
    {
        if (State == ConfigurationState.Invalid)
        {
            return _initializationError != null
                ? new Failure<T>(_initializationError)
                : "Configuration is in invalid state".ToFailure<T>("INVALID_STATE");
        }

        try
        {
            var stringValue = _configuration![key];
            if (stringValue == null)
            {
                return $"Configuration key '{key}' not found".ToFailure<T>("KEY_NOT_FOUND");
            }

            if (typeof(T) == typeof(string))
            {
                return ((T)(object)stringValue).ToSuccess();
            }

            var converter = System.ComponentModel.TypeDescriptor.GetConverter(typeof(T));
            if (converter.CanConvertFrom(typeof(string)))
            {
                var convertedValue = (T)converter.ConvertFromString(stringValue)!;
                return convertedValue.ToSuccess();
            }

            return $"Cannot convert '{stringValue}' to {typeof(T).Name}".ToFailure<T>("CONVERSION_ERROR");
        }
        catch (Exception ex)
        {
            return $"Failed to get and convert configuration value for key '{key}' to {typeof(T).Name}".ToFailure<T>("CONVERSION_ERROR", ex);
        }
    }
}
using System;
using System.IO;
using System.Linq;
using System.Xml;
using System.Xml.Linq;

namespace TweakerCore.Configuration
{
    /// <summary>Edits appSettings without loading section types or discarding unrelated XML.</summary>
    public sealed class XmlSettingsStore
    {
        private readonly string _path;
        private readonly XDocument _document;
        private readonly XElement _settings;

        public XmlSettingsStore(string path)
        {
            _path = path;
            if (File.Exists(path))
            {
                using var reader = XmlReader.Create(path, new XmlReaderSettings { DtdProcessing = DtdProcessing.Prohibit });
                _document = XDocument.Load(reader, LoadOptions.PreserveWhitespace);
                if (_document.Root?.Name != "configuration")
                    throw new XmlException("Expected a configuration root element.");
            }
            else
            {
                _document = new XDocument(new XElement("configuration"));
            }

            _settings = _document.Root.Element("appSettings");
            if (_settings == null)
            {
                _settings = new XElement("appSettings");
                _document.Root.Add(_settings);
            }

            // External/merged sections need semantics this small local store does not implement.
            if (_settings.Attribute("file") != null || _settings.Attribute("configSource") != null)
                throw new XmlException("External appSettings sections are not supported.");
        }

        public string Get(string key) => Find(key)?.Attribute("value")?.Value;

        public void Set(string key, string value)
        {
            var element = Find(key);
            if (element == null)
            {
                element = new XElement("add", new XAttribute("key", key));
                _settings.Add(element);
            }

            element.SetAttributeValue("value", value ?? "");
        }

        public bool Ensure(string key, string value)
        {
            if (Get(key) != null)
                return false;

            Set(key, value);
            return true;
        }

        public bool Remove(string key)
        {
            var elements = _settings.Elements("add").Where(e => (string)e.Attribute("key") == key).ToArray();
            foreach (var element in elements)
                element.Remove();

            return elements.Length != 0;
        }

        public void Save()
        {
            var temporaryPath = _path + "." + Guid.NewGuid().ToString("N") + ".tmp";
            try
            {
                using (var stream = new FileStream(temporaryPath, FileMode.CreateNew, FileAccess.Write, FileShare.None))
                {
                    _document.Save(stream, SaveOptions.DisableFormatting);
                    stream.Flush(true);
                }

                File.Move(temporaryPath, _path, overwrite: true);
            }
            finally
            {
                if (File.Exists(temporaryPath))
                    File.Delete(temporaryPath);
            }
        }

        private XElement Find(string key) => _settings.Elements("add")
            .LastOrDefault(e => (string)e.Attribute("key") == key);
    }
}

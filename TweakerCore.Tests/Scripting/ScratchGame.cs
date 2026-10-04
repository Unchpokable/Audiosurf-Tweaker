namespace TweakerCore.Tests.Scripting
{
    using System;
    using System.IO;
    using TweakerCore.PluginInstall;

    /// <summary>
    /// A temporary folder with a fake game's engine\ (QuestViewer.exe + channels\, which is all
    /// PluginInstallation checks) and a fake plugin bundle beside it.
    /// </summary>
    internal sealed class ScratchGame : IDisposable
    {
        public ScratchGame()
        {
            Root = Path.Combine(Path.GetTempPath(), "TweakerCoreTests", "Scripting", Guid.NewGuid().ToString("N"));
            Engine = Path.Combine(Root, "Audiosurf", "engine");
            Payload = Path.Combine(Root, "PluginPayload");

            Directory.CreateDirectory(Path.Combine(Engine, PluginInstallation.ChannelsFolderName));
            File.WriteAllText(Path.Combine(Engine, PluginInstallation.GameExecutableName), "not really the game");
            Directory.CreateDirectory(Scripts);
            WritePayload(PluginInstallation.PluginRelativePath, "plugin v1");
        }

        public string Root { get; }
        public string Engine { get; }
        public string Payload { get; }

        public string Scripts => PluginInstallation.ScriptsPath(Engine);

        public string ScriptPath(string file) => Path.Combine(Scripts, file);

        public ScratchGame Script(string file, string text)
        {
            File.WriteAllText(ScriptPath(file), text);
            return this;
        }

        public ScratchGame ShippedScript(string file, string text)
        {
            return WritePayload(PluginInstallation.ScriptsRelativeFolder + file, text);
        }

        public ScratchGame WritePayload(string relativePath, string text)
        {
            var path = Path.Combine(Payload, relativePath.Replace('/', Path.DirectorySeparatorChar));
            Directory.CreateDirectory(Path.GetDirectoryName(path));
            File.WriteAllText(path, text);
            return this;
        }

        public string ReadScript(string file) => File.ReadAllText(ScriptPath(file));

        public void Dispose()
        {
            try
            {
                if (Directory.Exists(Root))
                    Directory.Delete(Root, recursive: true);
            }
            catch (IOException)
            {
            }
        }
    }
}

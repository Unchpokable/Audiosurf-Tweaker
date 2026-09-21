using System.Collections.Generic;

namespace TweakerCore.PluginInstall
{
    /// <summary>A shipped file that was updated over a copy the user had changed, and where their copy went.</summary>
    public sealed class PluginFileBackup
    {
        public PluginFileBackup(string relativePath, string backupFileName)
        {
            RelativePath = relativePath;
            BackupFileName = backupFileName;
        }

        public string RelativePath { get; }

        /// <summary>File name only ("puzzlepro_hud.lua.old") - it sits next to the file it was made from.</summary>
        public string BackupFileName { get; }
    }

    public sealed class PluginInstallError
    {
        public PluginInstallError(string relativePath, string message)
        {
            RelativePath = relativePath;
            Message = message;
        }

        /// <summary>Null when the failure is about the operation as a whole rather than one file.</summary>
        public string RelativePath { get; }

        public string Message { get; }
    }

    /// <summary>
    /// What an install or uninstall actually did. The host turns the interesting half of it - <see cref="UpdatedWithBackup"/>
    /// above all - into a notification (Docs/Internal/plugin-offline-mode.md §6.3, Р-2).
    ///
    /// Every list holds paths relative to the engine folder, '/'-separated.
    /// </summary>
    public sealed class PluginInstallReport
    {
        /// <summary>Files that were not there at all and are now.</summary>
        public List<string> Added { get; } = new List<string>();

        /// <summary>Shipped files overwritten silently: the copy on disk was still the one Tweaker wrote.</summary>
        public List<string> Updated { get; } = new List<string>();

        /// <summary>Updated over the user's own version, which was renamed out of the way first.</summary>
        public List<PluginFileBackup> UpdatedWithBackup { get; } = new List<PluginFileBackup>();

        /// <summary>Shipped files the user deleted. Deliberately not restored - deleting one is a decision.</summary>
        public List<string> SkippedDeleted { get; } = new List<string>();

        /// <summary>No longer part of the bundle and still untouched by the user, so removed.</summary>
        public List<string> Removed { get; } = new List<string>();

        /// <summary>No longer part of the bundle but modified, so left alone and dropped from the manifest - the user owns it now.</summary>
        public List<string> Released { get; } = new List<string>();

        public List<PluginInstallError> Errors { get; } = new List<PluginInstallError>();

        public bool Success
        {
            get { return Errors.Count == 0; }
        }

        /// <summary>Whether anything at all happened on disk - lets the host stay quiet about a no-op install.</summary>
        public bool ChangedAnything
        {
            get
            {
                return Added.Count > 0 || Updated.Count > 0 || UpdatedWithBackup.Count > 0 || Removed.Count > 0 || Released.Count > 0;
            }
        }
    }
}

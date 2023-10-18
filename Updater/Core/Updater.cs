using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.IO.Compression;
using System.Linq;
using Updater.Core.Extensions;

namespace Updater.Core
{
    internal class Updater : IDisposable
    {
        private string _targetApplicationPath;
        private ZipArchive _updateArchiveSource;
        private int _targetAppProcId;

        public static Updater SetUp(string targetAppPath, string updateZip, int targetAppProcId)
        {
            if (!Path.Exists(targetAppPath))
                throw new ArgumentException("Target application path does not exists");

            if (!Path.Exists(updateZip)) throw new ArgumentException("Update source zip path does not exists");

            var archive = new ZipArchive(File.OpenRead(updateZip), ZipArchiveMode.Read);

            var targetAppProc = Process.GetProcessById(targetAppProcId);
            if (targetAppProc == null)
                targetAppProcId = -1;

            return new Updater(targetAppPath, archive, targetAppProcId);
        }

        protected Updater(string targetApp, ZipArchive updateSource, int targetAppProcId)
        {
            _targetApplicationPath = targetApp;
            _updateArchiveSource = updateSource;
            _targetAppProcId = targetAppProcId;
        }

        public void UpdateBinaries()
        {
            ExtractArchiveParts(".exe", ".dll", ".xml");
        }

        public void UpdateConfig()
        {
            ExtractArchiveParts(".cfg", "config");
        }

        public void Dispose()
        {
            _updateArchiveSource.Dispose();
        }

        private void ExtractArchiveParts(params string[] exts)
        {
            var filesToExtract =
                _updateArchiveSource.Entries.Where(entry => Path.GetExtension(entry.Name).SameWith(exts));
            ExtractArchiveForced(filesToExtract, _targetApplicationPath);
        }

        private void ShutdownTargetApplication()
        {
            if (_targetAppProcId == -1)
                return;

            try
            {
                Process.Start(new ProcessStartInfo
                {
                    FileName = "cmd.exe",
                    Arguments = $"/c taskkill /f /pid {_targetAppProcId}",
                    WindowStyle = ProcessWindowStyle.Hidden,
                });
            }
            catch
            {
                // ignored
            }
        }

        private void ExtractArchiveForced(IEnumerable<ZipArchiveEntry> entries, string destination)
        {
            if (!Directory.Exists(destination))
                Directory.CreateDirectory(destination);

            foreach (var entry in entries)
            {
                var destinationPath = Path.GetFullPath(Path.Combine(destination, entry.FullName));

                if (entry.Length == 0)
                {
                    Directory.CreateDirectory(Path.GetDirectoryName(destinationPath));
                    continue;
                }

                entry.ExtractToFile(destinationPath, true);
            }
            
        }
    }
}

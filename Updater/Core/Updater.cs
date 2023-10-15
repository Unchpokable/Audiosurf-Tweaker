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
        private string _targetAppProcId;


        public void UpdateBinaries()
        {
            var files = _updateArchiveSource.Entries.Where(entry => Path.GetExtension(entry.Name).SameWith(".exe", ".dll", ".xml"));


        }

        public void UpdateOthers()
        {

        }

        public void Dispose()
        {
            _updateArchiveSource.Dispose();
        }


        private void ShutdownTargetApplication()
        {
            if (string.IsNullOrEmpty(_targetAppProcId))
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

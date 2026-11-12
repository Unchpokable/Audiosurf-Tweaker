using System;
using System.Diagnostics;
using System.IO;
using System.Threading.Tasks;

namespace TweakerUI.Core
{
    /// <summary>
    /// What the developer tabs ask of Explorer: show a folder, send one to the Recycle Bin. One place, so both tabs do
    /// it the same way and report failures the same way.
    /// </summary>
    internal static class ShellActions
    {
        /// <summary>Opens <paramref name="folder"/> in Explorer. A failure is logged under <paramref name="owner"/>: there is nothing the user could do about it.</summary>
        public static void OpenFolder(string owner, string folder)
        {
            try
            {
                using (Process.Start(new ProcessStartInfo(folder) { UseShellExecute = true }))
                {
                }
            }
            catch (Exception exception) when (exception is System.ComponentModel.Win32Exception || exception is InvalidOperationException)
            {
                Logger.Log(owner, "cannot open " + folder + ": " + exception.Message);
            }
        }

        /// <summary>
        /// Sends <paramref name="folder"/> and everything in it to the Recycle Bin - a project is the user's work, and
        /// the button is one misclick from "Build" (script-manager.md §8.16). Null on success, the reason otherwise.
        /// </summary>
        public static async Task<string> RecycleFolderAsync(string folder)
        {
            try
            {
                await Task.Run(() => Microsoft.VisualBasic.FileIO.FileSystem.DeleteDirectory(
                    folder, Microsoft.VisualBasic.FileIO.UIOption.OnlyErrorDialogs, Microsoft.VisualBasic.FileIO.RecycleOption.SendToRecycleBin));
                return null;
            }
            catch (Exception exception) when (exception is IOException || exception is UnauthorizedAccessException || exception is OperationCanceledException)
            {
                return exception.Message;
            }
        }

        public static async Task<string> RecycleFileAsync(string file)
        {
            try
            {
                await Task.Run(() => Microsoft.VisualBasic.FileIO.FileSystem.DeleteFile(
                    file, Microsoft.VisualBasic.FileIO.UIOption.OnlyErrorDialogs, Microsoft.VisualBasic.FileIO.RecycleOption.SendToRecycleBin));
                return null;
            }
            catch (Exception exception) when (exception is IOException || exception is UnauthorizedAccessException || exception is OperationCanceledException)
            {
                return exception.Message;
            }
        }
    }
}

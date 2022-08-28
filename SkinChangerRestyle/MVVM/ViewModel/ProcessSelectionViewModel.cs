using SkinChangerRestyle.Core;
using SkinChangerRestyle.Core.Extensions;
using SkinChangerRestyle.MVVM.Model;
using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.ComponentModel;
using System.Diagnostics;
using System.Linq;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using System.Windows;
using System.Windows.Data;

namespace SkinChangerRestyle.MVVM.ViewModel
{
    internal class ProcessSelectionViewModel : ObservableObject, IDisposable
    {
        public ProcessSelectionViewModel()
        {
            Processes = new ObservableCollection<ShortProcessDescriptor>();
            BindingOperations.EnableCollectionSynchronization(Processes, _locker);
            BrowseProcesses();
            _asHandle = AudiosurfHandle.Instance;
            SetHandleToSelectedProcess = new RelayCommand(SetHandleToSelectedProcessInternal);
        }

        ~ProcessSelectionViewModel()
        {
            Extensions.DisposeAndClear(this);
        }

        private ObservableCollection<ShortProcessDescriptor> _processes;
        private object _locker = new object();
        private bool _disposedValue;
        private Thread _searchTask;
        private string _searchMask;
        private AudiosurfHandle _asHandle;

        public string SearchMask
        {
            get { return _searchMask; }
            set 
            { 
                _searchMask = value; 
                OnPropertyChanged();
                _searchTask.Abort();
                BrowseProcesses(mask: $"{value}");
            }
        }

        public RelayCommand SetHandleToSelectedProcess { get; set; }
        public RelayCommand CloseWindow { get; set; }

        public ObservableCollection<ShortProcessDescriptor> Processes
        {
            get => _processes;
            set 
            { 
                _processes = value;
                BindingOperations.EnableCollectionSynchronization(_processes, _locker);
                OnPropertyChanged();
            }
        }

        protected virtual void Dispose(bool disposing)
        {
            if (!_disposedValue)
            {
                if (disposing)
                {
                    _processes.Clear();
                }
                _disposedValue = true;
            }
        }

        public void Dispose()
        {
            Dispose(disposing: true);
            GC.SuppressFinalize(this);
        }

        private void BrowseProcesses(string mask = null)
        {
            if (mask != null) mask = mask.ToLower();

            _searchTask = new Thread(() =>
            {
                Processes.Clear();
                foreach (var process in Process.GetProcesses())
                {
                    try
                    {
                        var p = process.Handle;
                        try
                        {
                            var shortproc = new ShortProcessDescriptor(process);
                            if (mask != null)
                            {
                                if (process.ProcessName.ToLower().Contains(mask))
                                    Processes.Add(shortproc);
                            }
                            else
                                Processes.Add(shortproc);
                        }
                        catch { }

                    }
                    catch (Win32Exception e) { }
                    catch { }
                }
            });
            _searchTask.Start();
        }

        private void SetHandleToSelectedProcessInternal(object param)
        {
            var process = (ShortProcessDescriptor)param;
            _asHandle.StopAutoHandling();
            _asHandle.SetHandle(Process.GetProcessById(process.GetProcessID()));
            GC.Collect();
            GC.WaitForPendingFinalizers();
            GC.Collect();
            CloseWindow.Execute(new object());
        }
    }

    internal class ShortProcessDescriptor
    {
        public ShortProcessDescriptor(Process origin)
        {
            Name = origin.ProcessName;
            ExecutablePath = origin.MainModule.FileName;
            _handle = origin.Id;
            _mwHandle = origin.MainWindowHandle;
        }

        public string Name { get; private set; }
        public string ExecutablePath { get; private set; }
        private int _handle;
        private IntPtr _mwHandle;

        public IntPtr GetMainWindow() => _mwHandle;
        public int GetProcessID() => _handle;
    }
}

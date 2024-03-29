﻿using SkinChangerRestyle.Core;
using SkinChangerRestyle.Core.Extensions;
using System;
using System.Collections.ObjectModel;
using System.ComponentModel;
using System.Threading;
using System.Windows.Data;
using ASCommander;

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

        private ObservableCollection<ShortProcessDescriptor> _processes;
        private object _locker = new object();
        private bool _disposedValue;
        private Thread _searchTask;
        private string _searchMask;
        private AudiosurfHandle _asHandle;

        public string SearchMask
        {
            get => _searchMask;
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
                foreach (var process in System.Diagnostics.Process.GetProcesses())
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
                    catch (Win32Exception) { }
                    catch { }
                }
            });
            _searchTask.Start();
        }

        private void SetHandleToSelectedProcessInternal(object param)
        {
            var process = (ShortProcessDescriptor)param;
            _asHandle.ReinitializeWndProcMessageService();
            _asHandle.StopAutoHandling();
            _asHandle.SetHandle(System.Diagnostics.Process.GetProcessById(process.GetProcessID()));
            GC.Collect();
            GC.WaitForPendingFinalizers();
            GC.Collect();
            CloseWindow.Execute(new object());
        }
    }

    internal class ShortProcessDescriptor
    {
        public ShortProcessDescriptor(System.Diagnostics.Process origin)
        {
            Name = origin.ProcessName;
            if (origin.MainModule != null) ExecutablePath = origin.MainModule.FileName;
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

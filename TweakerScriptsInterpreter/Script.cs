using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace TweakerScripts
{
    public class Script
    {
        public Script()
        {
            Operations = new List<Action>();
        }

        public Script(IList<Action> operations)
        {
            Operations = operations;
        }

        public event EventHandler<Exception> ScriptExecutionFault;

        public IList<Action> Operations { get; private set; }

        public void Execute()
        {
            foreach (var action in Operations)
            {
                try
                {
                    action.Invoke();
                }
                catch (Exception ex)
                {
                    ScriptExecutionFault?.Invoke(this, ex);
                    return;
                }
            }
        }

        public async void ExecuteAsync()
        {
            await Task.Run(() => { Execute(); });
        }
    }
}

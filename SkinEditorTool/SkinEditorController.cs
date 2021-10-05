namespace SkinEditorTool
{
    using System;
    using ChangerAPI.Skin_Creator;

    public class SkinEditorController
    {
        public event EventHandler EditorOpened;
        public event EventHandler EditorClosed;
        public event EventHandler Editor_SkinExported;
        public event EventHandler Editor_SkinRewrited;
        public event EventHandler Editor_SkinOpened;
        public event EventHandler Editor_SkinClosed;

        private SkinCreatorForm creatorForm;

        public SkinEditorController()
        {
            creatorForm = new SkinCreatorForm();
        }

        public void Show()
        {
            creatorForm.Show();
            EditorOpened?.Invoke(this, EventArgs.Empty);
        }

        public void Close()
        {
            creatorForm.Close();
            EditorClosed?.Invoke(this, EventArgs.Empty);
        }
    }
}

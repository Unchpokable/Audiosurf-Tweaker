namespace TweakerUI.Models
{
    /// <summary>One of twllink's --new templates, as the new-project form offers it.</summary>
    public sealed class ProjectTemplate
    {
        public ProjectTemplate(string key, string title, string hint)
        {
            Key = key;
            Title = title;
            Hint = hint;
        }

        /// <summary>What twllink --new takes: "hud", "hook", "empty".</summary>
        public string Key { get; }

        public string Title { get; }

        public string Hint { get; }
    }
}

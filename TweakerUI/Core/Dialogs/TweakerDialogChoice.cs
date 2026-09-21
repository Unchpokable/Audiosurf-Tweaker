using System;
using System.Collections.Generic;

namespace TweakerUI.Core.Dialogs
{
    /// <summary>
    /// One button of a free-form dialog. The two-outcome <see cref="TweakerDialogButtons"/> pair stays as
    /// it is - it covers nearly every call site - while the plugin flows (Docs/Internal/plugin-offline-mode.md
    /// §6.4, §6.5) need three: "close it myself" / "close it for me" / "cancel", and "load now" /
    /// "restart the game" / "later".
    /// </summary>
    public sealed class TweakerDialogChoice
    {
        public TweakerDialogChoice(string text, bool isCancel = false)
        {
            Text = text ?? throw new ArgumentNullException(nameof(text));
            IsCancel = isCancel;
        }

        public string Text { get; }

        /// <summary>
        /// The outcome a closed window means. Exactly one choice should carry it: closing the dialog through
        /// the window chrome has to land somewhere, and "the user walked away" is never the destructive option.
        /// </summary>
        public bool IsCancel { get; }

        public static IReadOnlyList<TweakerDialogChoice> Of(params string[] texts)
        {
            var choices = new List<TweakerDialogChoice>(texts.Length);
            for (var i = 0; i < texts.Length; i++)
                choices.Add(new TweakerDialogChoice(texts[i], isCancel: i == texts.Length - 1));

            return choices;
        }
    }
}

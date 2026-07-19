using System.Collections.Generic;
using AudiosurfInterface;

namespace TweakerUI.Core
{
    /// <summary>
    /// The 14 real, playable Audiosurf characters, in the transport bar's 2x7 grid order.
    /// GameCharacter also has CurrentCharacter ("don't force anything") and Freeride - neither is a
    /// selectable character button, so they're deliberately excluded from this roster.
    /// </summary>
    internal static class CharacterRoster
    {
        public static readonly IReadOnlyList<(GameCharacter Value, string DisplayName)> RealCharacters = new[]
        {
            (GameCharacter.Mono, "Mono"),
            (GameCharacter.Pointman, "Pointman"),
            (GameCharacter.DoubleVision, "Double Vision"),
            (GameCharacter.MonoPro, "Mono Pro"),
            (GameCharacter.Vegas, "Vegas"),
            (GameCharacter.Eraser, "Eraser"),
            (GameCharacter.PointmanPro, "Pointman Pro"),
            (GameCharacter.Pusher, "Pusher"),
            (GameCharacter.DoubleVisionPro, "Double Vision Pro"),
            (GameCharacter.NinjaMono, "Ninja Mono"),
            (GameCharacter.EraserElite, "Eraser Elite"),
            (GameCharacter.PointmanElite, "Pointman Elite"),
            (GameCharacter.PusherElite, "Pusher Elite"),
            (GameCharacter.DoubleVisionElite, "Double Vision Elite"),
        };
    }
}

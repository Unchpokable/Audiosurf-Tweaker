namespace AudiosurfInterface
{
    /// <summary>
    /// The character/mode a track is launched with (the "playsong&lt;character&gt;" family of
    /// ascommand). CurrentCharacter forces nothing - the game plays with whatever character is
    /// already selected at the character screen. Freeride is its own mode, not a scored character,
    /// but shares the same "playsong..." dispatch slot.
    /// </summary>
    public enum GameCharacter
    {
        CurrentCharacter,
        Mono,
        Pointman,
        DoubleVision,
        MonoPro,
        Vegas,
        Eraser,
        PointmanPro,
        Pusher,
        DoubleVisionPro,
        NinjaMono,
        EraserElite,
        PointmanElite,
        PusherElite,
        DoubleVisionElite,
        Freeride
    }
}

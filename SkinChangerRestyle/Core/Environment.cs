namespace SkinChangerRestyle.Core
{
    using ChangerAPI.Engine;
    using System.Collections.Generic;

    internal static class Environment
    {
        internal static string skinsFolderPath { get; set; }
        internal static string gamePath { get; set; }
        internal static DCSBehaviour ControlSystemBehaviour { get; set; }
        internal static bool DCSWarningsAllowed { get; set; }

        internal static List<SkinLink> LoadedSkins { get; set; }
        internal static AudiosurfSkin[] ActivePageSkins { get; set; }

        internal static IEnumerable<SkinLink[]> LoadedSkinsByChanks
        {
            get
            {
                var chunk = new List<SkinLink>(chunkSize);
                using (var sourceEnumerator = LoadedSkins.GetEnumerator())
                {
                    while (sourceEnumerator.MoveNext())
                    {
                        chunk.Add(sourceEnumerator.Current);
                        if (chunk.Count == chunk.Capacity)
                        {
                            yield return chunk.ToArray();
                            chunk.Clear();
                        }
                    }
                    yield return chunk.ToArray();
                }
            }
        }

        private static int chunkSize = 6;
    }
}

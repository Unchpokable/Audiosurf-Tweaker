namespace SkinChangerRestyle.Core
{
    using ChangerAPI.Engine;
    using System.Collections.Generic;
    using System.IO;

    internal static class EnvironmentContainer
    {
        internal static string skinsFolderPath { get; set; }
        internal static string gamePath { get; set; }
        internal static DCSBehaviour ControlSystemBehaviour { get; set; }
        internal static bool DCSWarningsAllowed { get; set; }

        internal static List<SkinLink> LoadedSkins { get; set; }
        internal static AudiosurfSkinExtended[] ActivePageSkins { get; set; }

        internal static string DefaultSkinsPath
        {
            get
            {
                return Path.GetDirectoryName(System.Reflection.Assembly.GetExecutingAssembly().Location) + @"\\Skins";
            }
        }

        internal static IEnumerable<SkinLink[]> LoadedSkinsByChunks
        {
            get
            {
                if (LoadedSkins == null) yield break; 

                var chunk = new List<SkinLink>(ChunkSize);
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

        public static readonly int ChunkSize = 9;
    }
}

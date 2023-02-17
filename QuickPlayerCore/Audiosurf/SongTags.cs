using System;

namespace QuickPlayerCore.Audiosurf
{
    public class SongTags
    {
        public readonly string FourLanes = "[as-4lane]";
        public readonly string ForceMono = "[as-monoonly]";
        public readonly string EverybodyMono = "[as-everybodymono]";
        public readonly string MonoLessGrey = "[as-lessgrey]";
        public readonly string MonoAllGrey = "[as-allgrey]";
        public readonly string MonoNogrey = "[as-nogrey]";
        public readonly string MonoNoStealth = "[as-npstlth]";
        public readonly string FirstPerson = "[as-first]";
        public readonly string SidewinderCamera = "[as-swind]";
        public readonly string BankingCamera = "[as-bankcam]";
        public readonly string BlocksCaterpillar = "[as-caterp]";
        public readonly string HideBlocksGrid = "[as-hidepuz]";
        public readonly string Steep = "[as-steep]";

        public string MinimumMatchSize(int value)
        {
            if (value < 0 || value > 24)
                throw new ArgumentOutOfRangeException("value", value, "Minimal match size can't be bigger than maximum grid capaticy of 1-hand elite characters");
            return $"[as-msz{value}]";
        }

        public string WhitesPercentage(int value)
        {
            if (value < 0 || value > 100)
                throw new ArgumentOutOfRangeException("value", value, "Percent value can't exceed range of 0-100");
            return $"[as-wb{value}]";
        }

        public string MatchCollectionTicks(int value)
        {
            if (value < 0)
                throw new ArgumentOutOfRangeException("value", value, "Can't set match collection ticks count to negative");
            return $"[as-mt{value}]";
        }

        public string GridRowsCount(int value)
        {
            if (value < 0 || value > 8)
                throw new ArgumentOutOfRangeException("value", value, "Acceptable value must be in range 0-8");
            return $"[as-prows{value}]";
        }
    }
}

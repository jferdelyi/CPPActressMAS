// Seed shim of the C# oracles (CPPActressMAS): every `new Random()` of an example or of the framework becomes a
// System.Random seeded from its call site, so that an oracle run is reproducible and a C++ port can draw the same
// numbers. GlobalUsings.cs maps the name Random to this class; the original sources are compiled unchanged.
//
// Seed of the k-th instance created at a call site (file basename B, line L), k = 0, 1, ...:
//   h    = FNV-1a 64 of the UTF-8 text "B:L"
//   x    = SplitMix64(SplitMix64(CAM_SEED ^ h) + k)
//   seed = (int)(x & 0x7FFFFFFF)
// CAM_SEED is read from the environment variable of the same name (0 when absent). Each seed is logged on stderr as
// "#camrng B:L k seed". Sample and Next are not overridden: the draws are those of the seeded System.Random.
using System;
using System.Collections.Generic;
using System.IO;
using System.Runtime.CompilerServices;
using System.Text;

namespace CamOracle
{
    public class SeededRandom : System.Random
    {
        public SeededRandom([CallerFilePath] string p_file = "", [CallerLineNumber] int p_line = 0) : base(SiteSeeds.Next(p_file, p_line)) { }

        public SeededRandom(int p_seed) : base(p_seed) { }
    }

    internal static class SiteSeeds
    {
        private static readonly object s_lock = new object();
        private static readonly Dictionary<string, ulong> s_counts = new Dictionary<string, ulong>();
        private static readonly ulong s_base = ReadBase();

        public static int Next(string p_file, int p_line)
        {
            string l_site = Path.GetFileName(p_file.Replace('\\', '/')) + ":" + p_line;
            ulong l_k;
            lock (s_lock)
            {
                s_counts.TryGetValue(l_site, out l_k);
                s_counts[l_site] = l_k + 1;
            }
            int l_seed = Seed(s_base, l_site, l_k);
            Console.Error.WriteLine("#camrng " + l_site + " " + l_k + " " + l_seed);
            return l_seed;
        }

        public static int Seed(ulong p_base, string p_site, ulong p_k)
        {
            ulong l_x = SplitMix64(SplitMix64(p_base ^ Fnv1a(p_site)) + p_k);
            return (int)(l_x & 0x7FFFFFFFUL);
        }

        private static ulong ReadBase()
        {
            string l_text = System.Environment.GetEnvironmentVariable("CAM_SEED");
            return ulong.TryParse(l_text, out ulong l_value) ? l_value : 0UL;
        }

        private static ulong Fnv1a(string p_text)
        {
            ulong l_hash = 14695981039346656037UL;
            foreach (byte l_byte in Encoding.UTF8.GetBytes(p_text))
            {
                l_hash ^= l_byte;
                l_hash *= 1099511628211UL;
            }
            return l_hash;
        }

        private static ulong SplitMix64(ulong p_x)
        {
            ulong l_z = p_x + 0x9E3779B97F4A7C15UL;
            l_z = (l_z ^ (l_z >> 30)) * 0xBF58476D1CE4E5B9UL;
            l_z = (l_z ^ (l_z >> 27)) * 0x94D049BB133111EBUL;
            return l_z ^ (l_z >> 31);
        }
    }
}

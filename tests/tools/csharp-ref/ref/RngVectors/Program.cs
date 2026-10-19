// Test vectors for a C++ re-implementation of .NET's SEEDED System.Random
// (legacy Knuth subtractive generator; .NET 6+ keeps it as Net5CompatSeedImpl when a seed is given).
// Every series starts from a FRESH generator.
using System;
using System.Globalization;
using System.Linq;
using System.Reflection;
using System.Runtime.InteropServices;

internal static class Program
{
    private static string Ints(Func<int> f, int n) =>
        string.Join(" ", Enumerable.Range(0, n).Select(_ => f().ToString(CultureInfo.InvariantCulture)));

    private static void Main()
    {
        Console.WriteLine("# runtime: " + RuntimeBanner.Text);

        // ---- vectors requested by the kit ----
        Random r;
        r = new Random(42); Console.WriteLine("Random(42).Next()        : " + Ints(() => r.Next(), 10));
        r = new Random(42); Console.WriteLine("Random(42).Next(100)     : " + Ints(() => r.Next(100), 10));
        r = new Random(42);
        Console.WriteLine("Random(42).NextDouble()  : (G17 | IEEE-754 bits)");
        for (int i = 0; i < 10; i++)
        {
            double d = r.NextDouble();
            Console.WriteLine($"  [{i}] {d.ToString("G17", CultureInfo.InvariantCulture)} | 0x{BitConverter.DoubleToInt64Bits(d):X16}");
        }
        r = new Random(1); Console.WriteLine("Random(1).Next(5)        : " + Ints(() => r.Next(5), 10));

        // ---- extra vectors (edge cases of the seed and of the range overloads) ----
        foreach (int seed in new[] { 0, 1, -1, 42, int.MaxValue, int.MinValue })
        {
            r = new Random(seed);
            Console.WriteLine($"Random({seed}).Next() x5 : " + Ints(() => r.Next(), 5));
        }
        r = new Random(42); Console.WriteLine("Random(42).Next(-10,10)  : " + Ints(() => r.Next(-10, 10), 10));
        r = new Random(42); Console.WriteLine("Random(42).Next(int.MinValue,int.MaxValue) [large-range path]: " +
                                              Ints(() => r.Next(int.MinValue, int.MaxValue), 5));
        r = new Random(1); Console.WriteLine("Random(1).Next(n) n=5,4,3,2 (one RandomPermutation(5)): " +
                                             string.Join(" ", new[] { 5, 4, 3, 2 }.Select(n => r.Next(n))));
    }
}

// Portable runtime banner: .NET (SDK build, NETCOREAPP defined) or Mono (mcs build against the shipped net472 dll).
internal static class RuntimeBanner
{
    public static string Text
    {
        get
        {
#if NETCOREAPP
            return RuntimeInformation.FrameworkDescription + " (Environment.Version " + Environment.Version + "), " +
                   RuntimeInformation.OSDescription + ", " + RuntimeInformation.ProcessArchitecture;
#else
            var mono = Type.GetType("Mono.Runtime");
            var name = mono == null ? null : mono.GetMethod("GetDisplayName", BindingFlags.NonPublic | BindingFlags.Static);
            return (name == null ? ".NET Framework" : "Mono " + name.Invoke(null, null)) +
                   " (Environment.Version " + Environment.Version + "), " + Environment.OSVersion + ", 64-bit process=" + Environment.Is64BitProcess;
#endif
        }
    }
}

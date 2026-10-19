// Execution-order traces of the ORIGINAL ActressMAS v3.1 EnvironmentMas (framework used as is:
// only the public extension points Agent.Setup/ActDefault and EnvironmentMas.TurnFinished are overridden).
// Log entry "X(setup)" = Setup() ran (first turn of the agent), "X" = ActDefault() ran (no message).
using System;
using System.Collections.Generic;
using System.Linq;
using System.Reflection;
using System.Runtime.InteropServices;
using ActressMas;

internal sealed class TraceEnv : EnvironmentMas
{
    public static readonly List<string> Log = new List<string>();

    public TraceEnv(int noTurns, bool randomOrder, Random rand)
        : base(noTurns: noTurns, randomOrder: randomOrder, rand: rand, parallel: false) { }

    public override void TurnFinished(int turn)
    {
        Console.WriteLine($"  turn {turn}: {string.Join(" ", Log)}   | keys after turn: {string.Join(" ", AllAgents())}");
        Log.Clear();
    }
}

internal class NamedAgent : Agent
{
    public override void Setup() => TraceEnv.Log.Add(Name + "(setup)");
    public override void ActDefault() => TraceEnv.Log.Add(Name);
}

// Scenario 3: B stops itself in its first ActDefault (turn 1); E then adds F in the same turn.
internal sealed class StoppingAgent : NamedAgent
{
    public override void ActDefault() { base.ActDefault(); if (Name == "B") Stop(); }
}

internal sealed class SpawningAgent : NamedAgent
{
    private bool _spawned;
    public override void ActDefault()
    {
        base.ActDefault();
        if (!_spawned) { _spawned = true; Environment.Add(new NamedAgent(), "F"); }
    }
}

internal static class Program
{
    private static readonly string[] Names = { "A", "B", "C", "D", "E" };

    private static void Run(string title, TraceEnv env, Func<string, Agent> make)
    {
        Console.WriteLine(title);
        foreach (var n in Names) env.Add(make(n), n);
        env.Start();
    }

    // Independent re-implementation of EnvironmentMas.RandomPermutation (EnvironmentMas.cs:322-338)
    // to show the exact RNG consumption: Next(n) for n = count..2, i.e. count-1 draws per turn.
    private static void Predict(int seed, int count, int turns)
    {
        var rand = new Random(seed);
        for (int t = 0; t < turns; t++)
        {
            int[] numbers = Enumerable.Range(0, count).ToArray();
            int n = count;
            var draws = new List<string>();
            while (n > 1)
            {
                int bound = n;
                int k = rand.Next(n--);
                draws.Add($"Next({bound})={k}");
                int tmp = numbers[n]; numbers[n] = numbers[k]; numbers[k] = tmp;
            }
            Console.WriteLine($"  turn {t}: {string.Join(" ", numbers.Select(i => Names[i]))}   | draws: {string.Join(" ", draws)}");
        }
    }

    private static void Main()
    {
        Console.WriteLine("# runtime: " + RuntimeBanner.Text + "; " + Info.Version);

        Run("S1 sequential, randomOrder=true, rand=new Random(1), noTurns=3, agents added A,B,C,D,E:",
            new TraceEnv(3, true, new Random(1)), _ => new NamedAgent());
        Console.WriteLine("S1-predicted (independent Fisher-Yates replay with new Random(1), key order A B C D E):");
        Predict(1, 5, 3);

        Run("S2 sequential, randomOrder=false, noTurns=3, agents added A,B,C,D,E:",
            new TraceEnv(3, false, null), _ => new NamedAgent());

        Run("S3 sequential, randomOrder=false, noTurns=4; turn 1: B.Stop() then E adds F (Dictionary slot reuse):",
            new TraceEnv(4, false, null),
            n => n == "B" ? new StoppingAgent() : n == "E" ? new SpawningAgent() : new NamedAgent());

        Run("S4 sequential, randomOrder=true, rand=new Random(1), noTurns=4; same B.Stop()/F spawn as S3:",
            new TraceEnv(4, true, new Random(1)),
            n => n == "B" ? new StoppingAgent() : n == "E" ? new SpawningAgent() : new NamedAgent());
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

// Driver of the Wolf, Sheep and Grass model of Antelmi et al. (ABM_Comparison, ActressMAS/PredatorPreyConsole), in
// place of its Program.cs, on ActressMAS v3.1: the same setup and the same measured scope, but the 200 turns run
// through Start() since RunTurn is private in v3.1, and the grass grows in TurnFinished, after the populations are
// written, as between two RunTurn calls of the original. Start() also ends when no agent is left, where the original
// would run its remaining turns empty.
// Arguments: --agents N (1 000 by default): 60 % sheep and 40 % wolves, as Settings.cs (4 800 and 3 200 on 282 x 282),
// on a field of side floor(sqrt(10 N)), Table 8 of the article. Prints the elapsed time in ms after the original line.
// The C++ counterpart is tests/bench/Wsg.cpp.
using System;
using System.Diagnostics;
using System.Globalization;

namespace PredatorPrey
{
    public class BenchEnvironment : WorldEnvironment
    {
        public BenchEnvironment(int noTurns) : base(noTurns)
        {
        }

        public override void TurnFinished(int turn)
        {
            base.TurnFinished(turn);
            World w = Memory["World"];
            w.UpdateGrass();
        }
    }

    public class Bench
    {
        private static Random _rand = new Random();
        static readonly Stopwatch timer = new Stopwatch();

        private static void Main(string[] args)
        {
            int noAgents = 1000;
            if (args.Length == 2 && args[0] == "--agents")
                noAgents = int.Parse(args[1], CultureInfo.InvariantCulture);
            else if (args.Length != 0)
            {
                Console.Error.WriteLine("usage: Wsg [--agents N]");
                System.Environment.Exit(2);
            }
            Settings.GridSize = (int)Math.Floor(Math.Sqrt(10.0 * noAgents));
            Settings.NoDoodlebugs = noAgents * 2 / 5;
            Settings.NoAnts = noAgents - Settings.NoDoodlebugs;

            // From here, the Main of the original Program.cs, but Start()
            var worldEnv = new BenchEnvironment(Settings.NoTurns); // derived from ActressMas.EnvironmentMas
            var world = worldEnv.Memory["World"];

            int noCells = Settings.GridSize * Settings.GridSize;

            int[] randVect = RandomPermutation(noCells);
            timer.Start();

            for (int i = 0; i < Settings.NoDoodlebugs; i++)
            {
                int e = _rand.Next(20);
                var a = new DoodlebugAgent(e);

                worldEnv.Add(a, world.CreateName(a)); // unique name
                world.AddAgentToMap(a, randVect[i]);
            }

            for (int i = Settings.NoDoodlebugs; i < Settings.NoDoodlebugs + Settings.NoAnts; i++)
            {
                int e = _rand.Next(8);
                var a = new Sheep(e);
                worldEnv.Add(a, world.CreateName(a));
                world.AddAgentToMap(a, randVect[i]);
            }

            worldEnv.Start();

            Console.WriteLine("time elapsed - " + timer.Elapsed.ToString());
            timer.Stop();
            Console.WriteLine(timer.Elapsed.TotalMilliseconds.ToString("F3", CultureInfo.InvariantCulture) + " ms");
        }

        private static int[] RandomPermutation(int n)
        {
            // Fisher-Yates shuffle

            int[] numbers = new int[n];
            for (int i = 0; i < n; i++)
                numbers[i] = i;

            while (n > 1)
            {
                int k = _rand.Next(n--);
                int temp = numbers[n]; numbers[n] = numbers[k]; numbers[k] = temp;
            }

            return numbers;
        }
    }
}

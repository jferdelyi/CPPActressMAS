// Driver of the Flockers model of Antelmi et al. (ABM_Comparison, ActressMAS/flockers), in place of its Program.cs,
// on ActressMAS v3.1: the same setup and the same measured scope. The published Program.cs creates an AntAgent, the
// class that World.cs recognizes by its name, while the agent class of FlockAgent.cs is FlockAgent: AntAgent below is
// that agent under the expected name.
// Arguments: --agents N (1 000 by default; Settings.cs was left at 10 million agents on 10 000 x 10 000), on a field of
// side floor(sqrt(10 N)), Table 8 of the article. Prints the elapsed time in ms after the original line.
// The C++ counterpart is tests/bench/Flockers.cpp.
using System;
using System.Diagnostics;
using System.Globalization;

namespace PredatorPrey
{
    public class AntAgent : FlockAgent
    {
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
                Console.Error.WriteLine("usage: Flockers [--agents N]");
                System.Environment.Exit(2);
            }
            Settings.GridSize = (int)Math.Floor(Math.Sqrt(10.0 * noAgents));
            Settings.NoAnts = noAgents;

            // From here, the Main of the original Program.cs
            var worldEnv = new WorldEnvironment(Settings.NoTurns); // derived from ActressMas.EnvironmentMas
            var world = worldEnv.Memory["World"];

            int noCells = Settings.GridSize * Settings.GridSize;

            int[] randVect = RandomPermutation(noCells);
            timer.Start();

            int i = 0;
            while (i < Settings.NoAnts)
            {
                var a = new AntAgent();

                var x = _rand.NextDouble() * Settings.GridSize;
                var y = _rand.NextDouble() * Settings.GridSize;
                a.setCoord(x, y);
                a.last_x = 0.0;
                a.last_y = 0.0;
                var xx = (int)(Math.Floor(x));
                var yy = (int)(Math.Floor(y));

                if (world.AddAgentToMap(a, xx, yy))
                {
                    worldEnv.Add(a, world.CreateName(a));
                    i++;
                }
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

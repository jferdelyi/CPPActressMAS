// PingPong by pairs for the CPPActressMAS performance protocol, on ActressMAS v3.1: the variant that the ActressMAS
// article cites without its code ("pairs of agents, 100 messages"), written after E30 Benchmarks/PingPong/Program.cs.
// 500 pairs of agents, no turn limit, 5 trials, a new environment per trial, the stopwatch covers the creation of the
// environment, the agents and Start(). In each pair, the first agent sends "msg" to the second at setup; each agent
// replies to every message until it has sent half of the messages of its pair, and stops once it has also received as
// many. The simulation ends when every agent has stopped, after exactly pairs * messages messages. The counter is
// interlocked, and the time of a trial is printed to the microsecond, the workload being short.
// Arguments: --sequential (parallel: false, randomOrder: false), --pairs N, --messages N (per pair, even).
// The C++ counterpart is tests/bench/PingPongPairs.cpp.
using ActressMas;
using System;
using System.Diagnostics;
using System.Globalization;
using System.Threading;

namespace PingPongPairs
{
    public static class Global
    {
        public static Stopwatch MyStopWatch;
        public static int NoMessages;
        public static int NoPairs = 500;
        public static int MessagesPerAgent = 50;
    }

    public class Program
    {
        private static void Main(string[] args)
        {
            bool sequential = false;
            for (int i = 0; i < args.Length; i++)
            {
                if (args[i] == "--sequential")
                    sequential = true;
                else if (args[i] == "--pairs" && i + 1 < args.Length)
                    Global.NoPairs = int.Parse(args[++i], CultureInfo.InvariantCulture);
                else if (args[i] == "--messages" && i + 1 < args.Length)
                    Global.MessagesPerAgent = int.Parse(args[++i], CultureInfo.InvariantCulture) / 2;
                else
                {
                    Console.Error.WriteLine("usage: PingPongPairs [--sequential] [--pairs N] [--messages N]");
                    Environment.Exit(2);
                }
            }

            int n = 5;
            double sum = 0;

            Global.MyStopWatch = new Stopwatch();

            for (int t = 0; t < n; t++)
            {
                Console.WriteLine($"Trial {t + 1}");
                Global.NoMessages = 0;

                Global.MyStopWatch.Restart();

                var env = sequential ? new EnvironmentMas(noTurns: 0, randomOrder: false, parallel: false) : new EnvironmentMas(noTurns: 0, parallel: true);

                for (int i = 0; i < 2 * Global.NoPairs; i++)
                {
                    var a = new MyAgent(i);
                    env.Add(a, $"a{i}");
                }

                env.Start();

                Global.MyStopWatch.Stop();

                double ms = Global.MyStopWatch.Elapsed.TotalMilliseconds;
                Console.WriteLine($"{Global.NoMessages} msg");
                Console.WriteLine(ms.ToString("F3", CultureInfo.InvariantCulture) + " ms\n");

                sum += (double)Global.NoMessages / ms;
            }

            Console.WriteLine((sum / n).ToString("F3", CultureInfo.InvariantCulture) + " msg/ms");
        }

        public class MyAgent : Agent
        {
            private int _id;
            private int _sent = 0;
            private int _received = 0;

            public MyAgent(int id)
            {
                _id = id;
            }

            public override void Setup()
            {
                if (_id % 2 == 0)
                {
                    Send($"a{_id + 1}", "msg");
                    _sent++;
                    Interlocked.Increment(ref Global.NoMessages);
                }
            }

            public override void Act(Message m)
            {
                _received++;
                if (_sent < Global.MessagesPerAgent)
                {
                    Send(m.Sender, "msg");
                    _sent++;
                    Interlocked.Increment(ref Global.NoMessages);
                }
                if (_sent == Global.MessagesPerAgent && _received == Global.MessagesPerAgent)
                    Stop();
            }
        }
    }
}

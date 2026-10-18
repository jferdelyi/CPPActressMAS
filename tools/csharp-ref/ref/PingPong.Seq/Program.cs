// Copy of ActressMAS v3.1 E30 Benchmarks/PingPong/Program.cs for the CPPActressMAS performance protocol: the only
// change is the environment, sequential in a fixed order (parallel: false, randomOrder: false), the deterministic
// variant that perf/PROTOCOLE.md takes as the main comparison (the C++ counterpart is bench_PingPong --sequential).
// The original program sets parallel: true and keeps an alternative line commented out with parallel: false.
using ActressMas;
using System;
using System.Diagnostics;

namespace PingPongSeq
{
    public static class Global
    {
        public static Stopwatch MyStopWatch;
        public static volatile int NoMessages;
        public static long MaxMessages = 10 * 1000 * 1000;
        public static int NoAgents = 10;
    }

    public class Program
    {
        private static void Main(string[] args)
        {
            int n = 5;
            double sum = 0;

            Global.MyStopWatch = new Stopwatch();

            for (int t = 0; t < n; t++)
            {
                Console.WriteLine($"Trial {t + 1}");
                Global.NoMessages = 0;

                Global.MyStopWatch.Restart();

                var env = new EnvironmentMas(noTurns: 10000, randomOrder: false, parallel: false);

                for (int i = 0; i < Global.NoAgents; i++)
                {
                    var a = new MyAgent(i);
                    env.Add(a, $"a{i}");
                }

                env.Start();

                Global.MyStopWatch.Stop();

                Console.WriteLine($"{Global.NoMessages} msg");
                Console.WriteLine($"{Global.MyStopWatch.ElapsedMilliseconds} ms\n");

                sum += (double)Global.NoMessages / (double)Global.MyStopWatch.ElapsedMilliseconds;
            }

            Console.WriteLine($"{sum / n:F3} msg/ms");
        }

        public class MyAgent : Agent
        {
            private int _id;

            public MyAgent(int id)
            {
                _id = id;
            }

            public override void Setup()
            {
                for (int i = 0; i < Global.NoAgents; i++)
                {
                    if (i != _id)
                    {
                        Send($"a{i}", "msg");
                        Global.NoMessages++;
                    }
                }
            }

            public override void Act(Message m)
            {
                if (Global.NoMessages < Global.MaxMessages)
                {
                    Send(m.Sender, "msg");
                    Global.NoMessages++;
                }
            }
        }
    }
}

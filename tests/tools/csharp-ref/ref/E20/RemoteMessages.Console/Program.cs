// E20 RemoteMessages (ActressMAS v3.1) without WinForms, for the CPPActressMAS harness: PingAgent, PongAgent
// and MasSetup.Configure are the originals (RemoteMessages/PingAgent.cs, PongAgent.cs, MasSetup.cs) with the
// RichTextBox replaced by the console; Program replaces MasMobileForm: create the Container, start it, wait for the
// registration (the form waits for a click), then RunMas, and stop after a bounded time.
// Usage: RemoteMessages.Console <server-ip> <server-port> <container-name> [start-delay-ms=2000] [run-seconds=20]
using System;
using System.Threading;
using ActressMas;

namespace RemoteMessages
{
    public static class Global
    {
        // The original appends to a RichTextBox; the console keeps the same text, line breaks included.
        public static void AppendText(string text) => Console.Write(text.Replace("\r\n", "\n"));
    }

    public class PingAgent : Agent
    {
        public override void Setup()
        {
            Global.AppendText($"[{Name}] Sending request\r\n");
            Send("pong-agent@Container2", "request-info");
        }

        public override void Act(Message message)
        {
            try
            {
                Global.AppendText($"\t{message.Format()}\r\n");
            }
            catch (Exception ex)
            {
                Global.AppendText($"{ex.Message}\r\n");
            }
        }
    }

    public class PongAgent : Agent
    {
        public override void Setup()
        {
            Global.AppendText($"[{Name}] Waiting for requests...\r\n");
        }

        public override void Act(Message message)
        {
            try
            {
                Global.AppendText($"\t{message.Format()}\r\n");

                if (message.Content == "request-info")
                {
                    string info = $"Info 1 from agent {Name} in container {Environment.ContainerName}";
                    Send(message.Sender, info);

                    info = $"Info 2 from agent {Name} in container {Environment.ContainerName}";
                    Send(message.Sender, info);
                }
            }
            catch (Exception ex)
            {
                Global.AppendText($"{ex.Message}\r\n");
            }
        }
    }

    public static class MasSetup
    {
        public static void Configure(EnvironmentMas env, string home)
        {
            if (string.IsNullOrEmpty(home))
            {
                Global.AppendText("Container not activated.\r\n");
                return;
            }

            Global.AppendText($"This is {home}\r\n");

            switch (home)
            {
                case "Container1":
                    var m = new PingAgent();
                    env.Add(m, "ping-agent");
                    break;

                case "Container2":
                    var a = new PongAgent();
                    env.Add(a, "pong-agent");
                    break;

                default:
                    break;
            }
        }
    }

    internal static class Program
    {
        private static void Main(string[] args)
        {
            string ip = args[0];
            int port = int.Parse(args[1]);
            string name = args[2];
            int startDelay = args.Length > 3 ? int.Parse(args[3]) : 2000;
            int seconds = args.Length > 4 ? int.Parse(args[4]) : 20;

            var container = new Container(ip, port, name);
            container.NewText += (source, e) =>
            {
                if (!e.Text.StartsWith("Message received") && !e.Text.StartsWith("Sending message"))
                    Console.WriteLine(e.Text);
            };
            container.Start();
            Thread.Sleep(startDelay);

            var env = new EnvironmentMas();
            MasSetup.Configure(env, name);
            container.RunMas(env);

            Thread.Sleep(seconds * 1000);
            Console.Out.Flush();
            System.Environment.Exit(0);
        }
    }
}

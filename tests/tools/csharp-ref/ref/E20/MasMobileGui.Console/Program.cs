// E20 MasMobileGui (ActressMAS v3.1) without WinForms, for the CPPActressMAS harness: MobileAgent.cs, StaticAgent.cs and
// MasSetup.cs are the original files, compiled unchanged. Program replaces MasMobileForm: "Connect" (buttonStart_Click:
// create and start the Container, the agents and the container writing to the text box), then "Run MAS"
// (buttonRunMas_Click) after a delay, and stop after a bounded time. The text box prints what it shows on the console;
// the list of the agents (timer) is not printed.
// Usage: MasMobileGui.Console <server-ip> <server-port> <container-name> [run-delay-ms=10000] [run-seconds=20]
using System;
using System.Threading;
using System.Windows.Forms;
using ActressMas;

namespace MasMobileGui
{
    internal static class Program
    {
        private static void Main(string[] args)
        {
            string serverIP = args[0];
            string serverPort = args[1];
            string containerName = args[2];
            int runDelay = args.Length > 3 ? int.Parse(args[3]) : 10000;
            int seconds = args.Length > 4 ? int.Parse(args[4]) : 20;
            var richTextBoxLog = new RichTextBox();

            // buttonStart_Click
            var container = new Container(serverIP, Convert.ToInt32(serverPort), containerName);
            Global.Rtb = richTextBoxLog; // agents will write to this
            container.NewText += (source, e) =>
            {
                // container_NewText
                if (!e.Text.StartsWith("Message received") && !e.Text.StartsWith("Sending message"))
                    richTextBoxLog.AppendText($"{e.Text}\r\n");
            };
            container.Start();
            Thread.Sleep(runDelay);

            // buttonRunMas_Click
            var environment = new EnvironmentMas();
            MasSetup.Configure(environment, containerName);
            container.RunMas(environment);

            Thread.Sleep(seconds * 1000);
            Console.Out.Flush();
            System.Environment.Exit(0);
        }
    }
}

namespace System.Windows.Forms
{
    // The text box of the form: what it shows, on the console, a whole text at a time
    public class RichTextBox
    {
        private readonly object _lock = new object();

        public void AppendText(string text)
        {
            lock (_lock)
                Console.Write(text.Replace("\r\n", "\n"));
        }
    }
}

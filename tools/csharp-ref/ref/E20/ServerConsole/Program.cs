// E20 MyServerConsole (ActressMAS v3.1) without its console UI, for the CPPActressMAS harness:
// the original ActressMas.Server, every NewText line printed with a timestamp (the original only clears the
// screen and prints the container list), and a bounded run instead of "Press ENTER" (no stdin in a container).
// Usage: ServerConsole [port=5000] [ping-ms=3000] [run-seconds=60]
using System;
using System.Threading;
using ActressMas;

internal static class Program
{
    private static void Main(string[] args)
    {
        int port = args.Length > 0 ? int.Parse(args[0]) : 5000;
        int ping = args.Length > 1 ? int.Parse(args[1]) : 3000;
        int seconds = args.Length > 2 ? int.Parse(args[2]) : 60;
        var server = new Server(port, ping);
        server.NewText += (source, e) => Console.WriteLine($"{DateTime.Now:HH:mm:ss.fff} {e.Text.Replace("\n", " | ")}");
        server.Start();
        Console.WriteLine($"Server listening on port {port}, ping {ping} ms, for {seconds} s.");
        Thread.Sleep(seconds * 1000);
        server.Stop();
        Console.WriteLine("Server stopped.");
    }
}

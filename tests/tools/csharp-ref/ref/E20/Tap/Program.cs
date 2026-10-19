// TCP relay for the CPPActressMAS harness: containers connect to the tap, the tap connects to the ActressMAS Server
// and forwards the bytes unchanged in both directions. Each direction is cut into frames with the 38-byte header of
// AsyncClientServerLib (01 FF, big-endian total length, two GUIDs) and every frame is logged: connection, direction,
// size, TCP read count, decoded ContainerMessage (Sender, Receiver, Info, Content) and full hex.
// Usage: Tap <listen-port> <server-port> <log-file> [run-seconds=60]
using System;
using System.IO;
using System.Net;
using System.Net.Sockets;
using System.Reflection;
using System.Text;
using System.Threading;
using ActressMas;

internal static class Program
{
    private const int HeaderLength = 38;
    private static readonly Type ContainerMessageType = typeof(Message).Assembly.GetType("ActressMas.ContainerMessage", true);
    private static readonly object Locker = new object();
    private static StreamWriter _log;
    private static int _connections;

    private static void Main(string[] args)
    {
        int listenPort = int.Parse(args[0]);
        int serverPort = int.Parse(args[1]);
        _log = new StreamWriter(args[2], false, new UTF8Encoding(false)) { AutoFlush = true };
        int seconds = args.Length > 3 ? int.Parse(args[3]) : 60;

        var listener = new TcpListener(IPAddress.Loopback, listenPort);
        listener.Start();
        Log($"tap listening on {listenPort}, server on {serverPort}");
        var acceptor = new Thread(() =>
        {
            while (true)
            {
                TcpClient client;
                try { client = listener.AcceptTcpClient(); }
                catch (SocketException) { return; }
                int id = Interlocked.Increment(ref _connections);
                var server = new TcpClient();
                server.Connect(IPAddress.Loopback, serverPort);
                client.NoDelay = true;
                server.NoDelay = true;
                Log($"connection {id} opened");
                StartPump(id, "container->server", client.GetStream(), server.GetStream(), server);
                StartPump(id, "server->container", server.GetStream(), client.GetStream(), client);
            }
        }) { IsBackground = true };
        acceptor.Start();
        Thread.Sleep(seconds * 1000);
        listener.Stop();
        Log("tap stopped");
    }

    private static void StartPump(int id, string direction, NetworkStream from, NetworkStream to, TcpClient peer)
    {
        new Thread(() => Pump(id, direction, from, to, peer)) { IsBackground = true }.Start();
    }

    private static void Pump(int id, string direction, NetworkStream from, NetworkStream to, TcpClient peer)
    {
        var pending = new MemoryStream();
        var buffer = new byte[65536];
        int reads = 0;
        try
        {
            while (true)
            {
                int n = from.Read(buffer, 0, buffer.Length);
                if (n <= 0)
                    break;
                reads++;
                to.Write(buffer, 0, n);
                pending.Write(buffer, 0, n);
                reads = Cut(id, direction, pending, reads);
            }
        }
        catch (Exception ex)
        {
            Log($"connection {id} {direction} error {ex.GetType().Name}: {ex.Message}");
        }
        Log($"connection {id} {direction} closed ({pending.Length} unframed bytes left)");
        try { peer.Client.Shutdown(SocketShutdown.Send); } catch (Exception) { }
    }

    // Logs every complete frame in pending and keeps the remainder; returns the read count of the unfinished frame.
    private static int Cut(int id, string direction, MemoryStream pending, int reads)
    {
        byte[] data = pending.ToArray();
        int offset = 0;
        while (data.Length - offset >= HeaderLength)
        {
            int length = (data[offset + 2] << 24) | (data[offset + 3] << 16) | (data[offset + 4] << 8) | data[offset + 5];
            if (data[offset] != 0x01 || data[offset + 1] != 0xFF || length < HeaderLength)
            {
                Log($"connection {id} {direction} INVALID header {Convert.ToHexString(data, offset, HeaderLength)}");
                offset = data.Length;
                break;
            }
            if (data.Length - offset < length)
                break;
            byte[] frame = new byte[length];
            Array.Copy(data, offset, frame, 0, length);
            // One call: the frame line and its hex stay together when two connections log at the same time
            Log($"connection {id} {direction} frame {length} bytes, {reads} read(s): {Decode(frame)}",
                $"hex {Convert.ToHexString(frame).ToLowerInvariant()}");
            offset += length;
            reads = 0;
        }
        pending.SetLength(0);
        pending.Write(data, offset, data.Length - offset);
        return reads;
    }

    private static string Decode(byte[] frame)
    {
        try
        {
            string base64 = Encoding.Unicode.GetString(frame, HeaderLength, frame.Length - HeaderLength);
            object cm = ContainerMessageType.GetMethod("Deserialize", BindingFlags.Public | BindingFlags.Static).Invoke(null, new object[] { base64 });
            string Get(string p) => (string)ContainerMessageType.GetProperty(p).GetValue(cm);
            string content = Get("Content") ?? "(null)";
            if (content.Length > 80)
                content = content.Substring(0, 80) + $"... ({content.Length} chars)";
            return $"{Get("Sender")} -> {Get("Receiver")}: {Get("Info")} # {content}";
        }
        catch (Exception ex)
        {
            Exception inner = ex is TargetInvocationException tie && tie.InnerException != null ? tie.InnerException : ex;
            return $"undecodable ({inner.GetType().Name}: {inner.Message})";
        }
    }

    private static void Log(params string[] lines)
    {
        lock (Locker)
        {
            foreach (string line in lines)
            {
                _log.WriteLine($"{DateTime.Now:HH:mm:ss.fff} {line}");
                Console.WriteLine(line.Length > 200 ? line.Substring(0, 200) + "..." : line);
            }
        }
    }
}

// Captures the ActressMAS v3.1 ContainerServer wire format from the ORIGINAL code, for the CPPActressMAS
// "actressmas" codec: ContainerMessage (internal) and its static Serialize, Message, a concrete AgentState, and the
// frame built by the internal AsyncClientServerLib BasicMessage.GetEnvelope (38-byte header + body), all reached
// by reflection so that nothing is re-implemented here.
// Output: one block per case, "name", "nrbf" (hex of the BinaryFormatter bytes), "base64", "frame" (hex, the two
// header GUIDs are random in the original and printed as is), then a round trip check through Deserialize.
// Usage: BfCapture [output-directory]   (also writes <case>.nrbf.hex, <case>.frame.hex, <case>.txt there)
using System;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Text;
using ActressMas;

namespace BfCapture
{
    [Serializable]
    public class CaptureAgentState : AgentState
    {
        public int Counter { get; set; }
    }

    internal static class Program
    {
        private static readonly Assembly Amas = typeof(Message).Assembly;
        private static readonly Type ContainerMessageType = Amas.GetType("ActressMas.ContainerMessage", true);
        private static readonly Type BasicMessageType = Amas.GetType("AsyncClientServerLib.Message.BasicMessage", true);
        private static string _outDir;

        private static void Main(string[] args)
        {
            _outDir = args.Length > 0 ? args[0] : null;
            if (_outDir != null)
                Directory.CreateDirectory(_outDir);

            Console.WriteLine($"runtime {System.Environment.Version} ({System.Runtime.InteropServices.RuntimeInformation.FrameworkDescription})");
            Console.WriteLine($"assembly {Amas.FullName}");

            // As Agent.Send builds them (conversationId defaults to ""), then Container.SendRemoteAgentMessage (Sender@container)
            var remote = new Message("ping-agent@Container1", "pong-agent@Container2", "request-info", "");
            var reply = new Message("pong-agent@Container2", "ping-agent@Container1", "Info 1 from agent pong-agent in container Container2", "");
            var nullConversation = new Message("a@C1", "b@C2", "hello");
            var conversation = new Message("a@C1", "b@C2", "hello", "conv-42");
            var empty = new Message("a@C1", "b@C2", "");
            var nullContent = new Message("a@C1", "b@C2", (string)null);
            var unicode = new Message("a@C1", "b@C2", "héllo → 世界");
            var objContent = new Message("a@C1", "b@C2", (object)"boxed-string");
            var objInt = new Message("a@C1", "b@C2", (object)42);

            // Message alone (the Content of "Send Remote Message")
            CaptureObject("message_remote", remote);
            CaptureObject("message_reply", reply);
            CaptureObject("message_conversation", conversation);
            CaptureObject("message_null_conversation", nullConversation);
            CaptureObject("message_empty_content", empty);
            CaptureObject("message_null_content", nullContent);
            CaptureObject("message_unicode", unicode);
            CaptureObject("message_contentobj_string", objContent);
            CaptureObject("message_contentobj_int", objInt);

            // ContainerMessage of every command, as Container.cs and Server.cs build them
            CaptureContainer("request_register", "Container1", "Server", "Request Register", "");
            CaptureContainer("inform_containers", "Server", "Container1", "Inform Containers", "Container2 Container1 ");
            CaptureContainer("inform_containers_empty", "Server", "Container1", "Inform Containers", "");
            CaptureContainer("inform_invalid_name", "Server", "Container1", "Inform Invalid Name", "");
            CaptureContainer("send_remote_message", "Container1", "Container2", "Send Remote Message", Serialize(remote));
            CaptureContainer("send_remote_reply", "Container2", "Container1", "Send Remote Message", Serialize(reply));
            CaptureContainer("null_content", "Container1", "Container2", "Send Remote Message", null);

            // Request Move Agent: Content = a serialized AgentState (AgentType is a System.Type)
            try
            {
                var state = new CaptureAgentState { AgentType = typeof(CaptureAgentState), Name = "mobile", Counter = 3 };
                string serializedState = Serialize(state);
                CaptureObject("agent_state", state);
                CaptureContainer("request_move_agent", "Container1", "Container2", "Request Move Agent", serializedState);
            }
            catch (Exception ex)
            {
                Console.WriteLine("== agent_state");
                Console.WriteLine($"FAILS {Unwrap(ex).GetType().FullName}: {Unwrap(ex).Message}");
                Write("agent_state.txt", $"FAILS {Unwrap(ex).GetType().FullName}: {Unwrap(ex).Message}\n");
            }
        }

        private static string Serialize(object o) =>
            (string)ContainerMessageType.GetMethod("Serialize", BindingFlags.Public | BindingFlags.Static).Invoke(null, new[] { o });

        private static object Deserialize(string s) =>
            ContainerMessageType.GetMethod("Deserialize", BindingFlags.Public | BindingFlags.Static).Invoke(null, new object[] { s });

        private static void CaptureObject(string name, object o)
        {
            string base64 = Serialize(o);
            byte[] nrbf = Convert.FromBase64String(base64);
            object back = Deserialize(base64);
            string check = back is Message m
                ? $"{m.Sender}|{m.Receiver}|{m.Content}|{m.ConversationId}|{(object)m.ContentObj}"
                : back?.GetType().FullName;
            Report(name, nrbf, base64, null, check);
        }

        private static void CaptureContainer(string name, string sender, string receiver, string info, string content)
        {
            object cm = Activator.CreateInstance(ContainerMessageType, BindingFlags.Public | BindingFlags.Instance, null,
                new object[] { sender, receiver, info, content }, null);
            string base64 = Serialize(cm);
            byte[] nrbf = Convert.FromBase64String(base64);
            byte[] body = Encoding.Unicode.GetBytes(base64);
            object message = Activator.CreateInstance(BasicMessageType, BindingFlags.Public | BindingFlags.Instance, null,
                new object[] { Guid.NewGuid(), body }, null);
            byte[] frame = (byte[])BasicMessageType.GetMethod("GetEnvelope").Invoke(message, null);
            object back = Deserialize(base64);
            string check = string.Join("|", new[] { "Sender", "Receiver", "Info", "Content" }
                .Select(p => (string)ContainerMessageType.GetProperty(p).GetValue(back)));
            Report(name, nrbf, base64, frame, check);
        }

        private static void Report(string name, byte[] nrbf, string base64, byte[] frame, string check)
        {
            var sb = new StringBuilder();
            sb.AppendLine($"== {name}");
            sb.AppendLine($"nrbf {nrbf.Length} {Hex(nrbf)}");
            sb.AppendLine($"base64 {base64.Length} {base64}");
            if (frame != null)
                sb.AppendLine($"frame {frame.Length} {Hex(frame)}");
            sb.AppendLine($"roundtrip {check}");
            Console.Write(sb.ToString());
            Write($"{name}.txt", sb.ToString());
            Write($"{name}.nrbf.hex", Hex(nrbf) + "\n");
            if (frame != null)
                Write($"{name}.frame.hex", Hex(frame) + "\n");
        }

        private static void Write(string file, string text)
        {
            if (_outDir != null)
                File.WriteAllText(Path.Combine(_outDir, file), text);
        }

        private static string Hex(byte[] bytes) =>
            Convert.ToHexString(bytes).ToLowerInvariant();

        private static Exception Unwrap(Exception ex) =>
            ex is TargetInvocationException tie && tie.InnerException != null ? tie.InnerException : ex;
    }
}

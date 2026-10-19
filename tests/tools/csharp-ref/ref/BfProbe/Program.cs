using System;
using System.IO;
using System.Runtime.Serialization.Formatters.Binary;

internal static class Program
{
    private static void Main()
    {
        try
        {
            var ms = new MemoryStream();
            new BinaryFormatter().Serialize(ms, new ActressMas.Message("a", "b", "hello"));
            Console.WriteLine($"BinaryFormatter OK on {Environment.Version}: {ms.Length} bytes");
        }
        catch (Exception e)
        {
            Console.WriteLine($"BinaryFormatter FAILS on {Environment.Version}: {e.GetType().FullName}: {e.Message}");
        }
    }
}

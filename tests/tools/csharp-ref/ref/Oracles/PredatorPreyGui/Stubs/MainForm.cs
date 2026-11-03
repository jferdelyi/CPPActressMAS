// Headless stand-in for the WinForms MainForm of E11 PredatorPreyGui and the program that shows it: Main runs at once
// what the "Run MAS" button runs (runMasButton_Click, copied unchanged), and the controls of the form print what they
// would show: the turn the text box shows after each turn, then the populations (world-state.txt) it loads at the end.
// The list of the agent names is not printed.
using System;
using System.IO;
using System.Windows.Forms;

namespace PredatorPreyGui
{
    public static class Program
    {
        private static void Main()
        {
            new MainForm().RunMas();
        }
    }

    public class MainForm
    {
        private World worldEnv = null;
        private ListBox listBox1 = new ListBox();
        private RichTextBox richTextBox1 = new RichTextBox();

        // runMasButton_Click
        public void RunMas()
        {
            worldEnv = new World(Settings.NoTurns); // derived from ActressMas.TurnBasedEnvironment
            worldEnv.SetControls(listBox1, richTextBox1);

            int noCells = Settings.GridSize * Settings.GridSize;
            int[] randVect = Settings.RandomPermutation(noCells);

            for (int i = 0; i < Settings.NoDoodlebugs; i++)
            {
                var a = new DoodlebugAgent();
                worldEnv.Add(a, worldEnv.CreateName(a)); // unique name
                worldEnv.AddAgentToMap(a, randVect[i]);
            }

            for (int i = Settings.NoDoodlebugs; i < Settings.NoDoodlebugs + Settings.NoAnts; i++)
            {
                var a = new AntAgent();
                worldEnv.Add(a, worldEnv.CreateName(a));
                worldEnv.AddAgentToMap(a, randVect[i]);
            }

            worldEnv.Start();

            richTextBox1.LoadFile(Settings.WorldStateFileName, RichTextBoxStreamType.PlainText);
        }
    }
}

namespace System.Windows.Forms
{
    public enum RichTextBoxStreamType { PlainText }

    public class ListBox
    {
        public class ObjectCollection
        {
            public void Clear() { }
            public void Add(object item) { }
        }

        public ObjectCollection Items { get; } = new ObjectCollection();

        public void BeginUpdate() { }

        public void EndUpdate() { }
    }

    public class RichTextBox
    {
        public void Clear() { }

        public void AppendText(string text) { Console.WriteLine(text); }

        public void LoadFile(string path, RichTextBoxStreamType fileType) { Console.Write(File.ReadAllText(path)); }
    }

    public static class Application
    {
        public static void DoEvents() { }
    }
}

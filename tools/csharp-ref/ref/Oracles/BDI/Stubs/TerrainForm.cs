// Headless stand-in for the WinForms TerrainForm of E02 (D86): the oracle runs without any interface and prints, at
// each refresh, the state the form would draw: the position of the patrol and the 15 cells (. normal, F fire, W water,
// w water being drawn). SetOwner only keeps the agent, ShowDialog returns at once.
using System;
using System.Text;

namespace Bdi
{
    public class TerrainForm
    {
        private TerrainAgent _ownerAgent;

        public void SetOwner(TerrainAgent a) { _ownerAgent = a; }

        public void ShowDialog() { }

        public void UpdateTerrainGUI()
        {
            if (_ownerAgent == null || _ownerAgent.States == null)
                return;
            var cells = new StringBuilder();
            foreach (var state in _ownerAgent.States)
                cells.Append(state == TerrainState.Fire ? 'F' : state == TerrainState.Water ? 'W' : state == TerrainState.GettingWater ? 'w' : '.');
            Console.WriteLine($"GUI pos={_ownerAgent.PatrolPosition} {cells}");
        }
    }
}

namespace System.Windows.Forms
{
    // TerrainAgent calls System.Windows.Forms.Application.Run() after the dialog: nothing to run headless
    public static class Application
    {
        public static void Run() { }
    }
}

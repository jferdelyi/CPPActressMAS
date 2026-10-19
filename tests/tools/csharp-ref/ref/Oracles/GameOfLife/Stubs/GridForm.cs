// Headless stand-in for the WinForms GridForm of E13: the oracle runs without any interface and prints, at each
// refresh, the grid the form would draw: one line per row y, one character per column x (x living, . dead). SetOwner
// only keeps the agent, ShowDialog returns at once.
using System;
using System.Text;

namespace GameOfLife
{
    public class GridForm
    {
        private GridAgent _ownerAgent;

        public void SetOwner(GridAgent a) { _ownerAgent = a; }

        public void ShowDialog() { }

        public void UpdatePlanetGUI()
        {
            if (_ownerAgent == null || _ownerAgent.CellStates == null)
                return;
            int size = _ownerAgent.CellStates.GetLength(0);
            for (int j = 0; j < size; j++)
            {
                var row = new StringBuilder();
                for (int i = 0; i < size; i++)
                    row.Append(_ownerAgent.CellStates[i, j] ? 'x' : '.');
                Console.WriteLine(row.ToString());
            }
        }
    }
}

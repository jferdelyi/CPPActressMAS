// Headless stand-in for the WinForms PlanetForm of E01: the oracle runs without any interface. The form only draws the
// positions the planet agent keeps, which the messages printed by the agents already show: UpdatePlanetGUI draws
// nothing. SetOwner only keeps the agent, ShowDialog returns at once.
namespace Reactive
{
    public class PlanetForm
    {
        private PlanetAgent _ownerAgent;

        public void SetOwner(PlanetAgent a) { _ownerAgent = a; }

        public void ShowDialog() { }

        public void UpdatePlanetGUI() { }
    }
}

namespace System.Windows.Forms
{
    // PlanetAgent calls System.Windows.Forms.Application.Run() after the dialog: nothing to run headless
    public static class Application
    {
        public static void Run() { }
    }
}

// Stand-ins for the Windows Forms types that flockers/InsectAgent.cs imports with `using static` and never uses, so
// that the original file compiles where Windows Forms does not exist (net10.0 on Linux).
namespace System.Windows.Forms
{
    public class LinkLabel
    {
    }

    namespace VisualStyles
    {
        public class VisualStyleElement
        {
            public class Tab
            {
            }
        }
    }
}

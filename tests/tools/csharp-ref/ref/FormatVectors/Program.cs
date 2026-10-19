// Vectors of the .NET number formatting and parsing used by the C# examples (D82), with the invariant culture of the
// harness (InvariantGlobalization). One line per case:
//   D|<IEEE-754 bits>|<F2>|<F3>|<F4>|<ToString()>
//   I|<UTF-16 code units of the input, hex>|<Convert.ToInt32 result, or the exception name>
using System;
using System.Globalization;
using System.Linq;
using System.Runtime.InteropServices;

internal static class Program
{
    private static readonly double[] Doubles =
    {
        0.0, -0.0, 0.125, 0.375, 0.5, 1.5, 2.5, -2.5, -7.5, 1.005, 2.675, 12.345, 49.995, 0.0049999, 0.005, -0.005,
        -0.001, 1.0 / 3, 2.0 / 3, -2.0 / 3, 0.1 + 0.2, 100.0, 123456.789, 123456789012345.0, 1234567890123456.0, 1e15,
        1e21, 1e-4, 1e-5, 1e-7, 0.97959183673469385, 1.7976931348623157e308, 5e-324, 2.2250738585072014e-308,
        double.NaN, double.PositiveInfinity, double.NegativeInfinity, 0.045, 0.0045, 1e16, 9.5, 10.5, -0.5,
        1e17, 1.2345678901234567e17, 99999999999999999.0, 0.00012345, 9.999e-5, -1e-5, -1e21, 123.0e-6, 1e100
    };

    private static readonly string[] Ints =
    {
        "42", " 42 ", "-7", "+7", "2147483647", "-2147483648", "2147483648", "-2147483649", "", "abc", "4 2", "\t13\n",
        "007", "1.0", "0x10", "−5", " 12", "- 3", "+-3", "  "
    };

    private static void Main()
    {
        Console.WriteLine("# runtime: " + RuntimeInformation.FrameworkDescription + ", " + RuntimeInformation.OSDescription);
        foreach (double d in Doubles)
        {
            Console.WriteLine($"D|0x{BitConverter.DoubleToInt64Bits(d):X16}|{d.ToString("F2")}|{d.ToString("F3")}|{d.ToString("F4")}|{d}");
        }
        foreach (string s in Ints)
        {
            string units = string.Join(" ", s.Select(c => ((int)c).ToString("X4")));
            string result;
            try { result = Convert.ToInt32(s).ToString(CultureInfo.InvariantCulture); }
            catch (Exception e) { result = e.GetType().Name; }
            Console.WriteLine($"I|{units}|{result}");
        }
    }
}

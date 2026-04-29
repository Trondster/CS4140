using System.Globalization;
using System.IO;

namespace ImageLabeler.Models;

public class LabelData
{
    public bool IsDrone { get; set; }
    public double X { get; set; }
    public double Y { get; set; }
    public double Width { get; set; }
    public double Height { get; set; }

    public static LabelData? TryLoad(string filePath)
    {
        if (!File.Exists(filePath)) return null;

        var parts = File.ReadAllText(filePath).Trim()
            .Split(' ', StringSplitOptions.RemoveEmptyEntries);

        if (parts.Length < 5) return null;
        if (!int.TryParse(parts[0], out int label)) return null;
        if (!TryParseDouble(parts[1], out double x)) return null;
        if (!TryParseDouble(parts[2], out double y)) return null;
        if (!TryParseDouble(parts[3], out double w)) return null;
        if (!TryParseDouble(parts[4], out double h)) return null;

        return new LabelData { IsDrone = label == 1, X = x, Y = y, Width = w, Height = h };
    }

    public void Save(string filePath)
    {
        Directory.CreateDirectory(Path.GetDirectoryName(filePath)!);
        string content = IsDrone
            ? $"1 {F(X)} {F(Y)} {F(Width)} {F(Height)}"
            : "0 0 0 0 0";
        File.WriteAllText(filePath, content);
    }

    public LabelData Clone() =>
        new() { IsDrone = IsDrone, X = X, Y = Y, Width = Width, Height = Height };

    private static string F(double v) => v.ToString("F6", CultureInfo.InvariantCulture);

    private static bool TryParseDouble(string s, out double v) =>
        double.TryParse(s, NumberStyles.Float, CultureInfo.InvariantCulture, out v);
}

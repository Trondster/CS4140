using System.IO;

namespace ImageLabeler.Models;

public class ImagePair
{
    public string Id { get; set; } = "";
    public string Category { get; set; } = "";       // "clear" or "drone"
    public string CurrentFramePath { get; set; } = "";
    public string DiffFramePath { get; set; } = "";
    public string LabelPath { get; set; } = "";

    public bool IsLabeled => File.Exists(LabelPath);
    public string DisplayText => IsLabeled ? Id : $"{Id} *";
}

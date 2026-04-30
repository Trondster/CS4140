using System.IO;
using System.Text.Json;

namespace ImageLabeler.Models;

public class UserSettings
{
    public string DatasetRoot { get; set; } = @"C:\Users\nicol\CS4140\train\dataset";
    public string MoveDestination { get; set; } = "";

    private static readonly string FilePath = Path.Combine(
        Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData),
        "ImageLabeler", "settings.json");

    public static UserSettings Load()
    {
        try
        {
            if (File.Exists(FilePath))
            {
                var loaded = JsonSerializer.Deserialize<UserSettings>(File.ReadAllText(FilePath));
                if (loaded != null) return loaded;
            }
        }
        catch { }
        return new UserSettings();
    }

    public void Save()
    {
        try
        {
            Directory.CreateDirectory(Path.GetDirectoryName(FilePath)!);
            File.WriteAllText(FilePath,
                JsonSerializer.Serialize(this, new JsonSerializerOptions { WriteIndented = true }));
        }
        catch { }
    }
}

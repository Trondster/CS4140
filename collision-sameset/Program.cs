using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Linq;

class Program
{
    static void Main(string[] args)
    {
        string datasetRoot = @"C:\repos\CS4140\train\dataset";
        string prefix = "42004242";

        string clearFolder = Path.Combine(datasetRoot, "grey", "clear");
        string droneFolder = Path.Combine(datasetRoot, "grey", "drone");
        string labelsFolder = Path.Combine(datasetRoot, "labels");
        string ignoredFolder = Path.Combine(datasetRoot, "ignored");

        var clearIds = CollectIds(clearFolder, "_clear_current_frame.png");
        var droneIds = CollectIds(droneFolder, "_drone_current_frame.png");

        var collidingIds = clearIds.Intersect(droneIds, StringComparer.OrdinalIgnoreCase).ToList();

        Console.WriteLine($"Found {clearIds.Count} clear ids, {droneIds.Count} drone ids, {collidingIds.Count} collisions:");
        Console.WriteLine(string.Join(',', collidingIds));

        foreach (var id in collidingIds)
        {
            Console.WriteLine($"Processing collision id: {id}");

            string labelPath = Path.Combine(labelsFolder, id + ".txt");
            bool renameLabel = false;

            if (File.Exists(labelPath))
            {
                renameLabel = ShouldRenameLabel(labelPath);
                Console.WriteLine(renameLabel
                    ? $"Label will be renamed: {Path.GetFileName(labelPath)}"
                    : $"Label stays untouched: {Path.GetFileName(labelPath)}");
            }
            else
            {
                Console.WriteLine($"Label file not found: {labelPath}");
            }

            RenameFullSet(datasetRoot, id, prefix);
            if (renameLabel)
            {
                RenameLabelFile(labelsFolder, id, prefix);
                RenameIgnoreFile(ignoredFolder, id, prefix);
            }
        }
    }

    static HashSet<string> CollectIds(string folder, string suffix)
    {
        if (!Directory.Exists(folder))
        {
            Console.WriteLine($"Directory does not exist: {folder}");
            return new HashSet<string>(StringComparer.OrdinalIgnoreCase);
        }

        var ids = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
        var files = Directory.EnumerateFiles(folder, "*" + suffix, SearchOption.TopDirectoryOnly);

        foreach (var file in files)
        {
            string fileName = Path.GetFileName(file);
            string id = ExtractId(fileName, suffix);
            if (!string.IsNullOrEmpty(id))
            {
                ids.Add(id);
            }
        }

        return ids;
    }

    static string ExtractId(string fileName, string suffix)
    {
        return fileName.EndsWith(suffix, StringComparison.OrdinalIgnoreCase)
            ? fileName.Substring(0, fileName.Length - suffix.Length)
            : null;
    }

    static bool ShouldRenameLabel(string labelPath)
    {
        var line = File.ReadLines(labelPath).FirstOrDefault()?.Trim();
        if (string.IsNullOrEmpty(line))
        {
            Console.WriteLine($"Empty label file: {labelPath}");
            return false;
        }

        var parts = line.Split(' ', StringSplitOptions.RemoveEmptyEntries);
        if (parts.Length != 5)
        {
            Console.WriteLine($"Unexpected label format in {labelPath}: {line}");
            return false;
        }

        if (!string.Equals(parts[0], "1", StringComparison.Ordinal))
        {
            return false;
        }

        for (int i = 1; i < parts.Length; i++)
        {
            if (!float.TryParse(parts[i], NumberStyles.Float, CultureInfo.InvariantCulture, out _))
            {
                Console.WriteLine($"Invalid float in label file {labelPath}: {parts[i]}");
                return false;
            }
        }

        return true;
    }

    static void RenameFullSet(string root, string id, string prefix)
    {
        string newId = prefix + id;

        var possiblePaths = new List<string>
        {
            // clear
            Path.Combine(root, "2x2", "clear", $"{id}_clear_current_frame.png"),
            Path.Combine(root, "2x2", "clear", $"{id}_clear_diff_frame.png"),
            Path.Combine(root, "3x3", "clear", $"{id}_clear_current_frame.png"),
            Path.Combine(root, "3x3", "clear", $"{id}_clear_diff_frame.png"),
            Path.Combine(root, "4x4", "clear", $"{id}_clear_current_frame.png"),
            Path.Combine(root, "4x4", "clear", $"{id}_clear_diff_frame.png"),
            Path.Combine(root, "grey", "clear", $"{id}_clear_current_frame.png"),
            Path.Combine(root, "grey", "clear", $"{id}_clear_diff_frame.png"),

            // drone
            Path.Combine(root, "2x2", "drone", $"{id}_drone_current_frame.png"),
            Path.Combine(root, "2x2", "drone", $"{id}_drone_diff_frame.png"),
            Path.Combine(root, "3x3", "drone", $"{id}_drone_current_frame.png"),
            Path.Combine(root, "3x3", "drone", $"{id}_drone_diff_frame.png"),
            Path.Combine(root, "4x4", "drone", $"{id}_drone_current_frame.png"),
            Path.Combine(root, "4x4", "drone", $"{id}_drone_diff_frame.png"),
            Path.Combine(root, "grey", "drone", $"{id}_drone_current_frame.png"),
            Path.Combine(root, "grey", "drone", $"{id}_drone_diff_frame.png"),

            // ignored (no extension)
            Path.Combine(root, "ignored", $"{id}_ignored")
        };

        foreach (var oldPath in possiblePaths)
        {
            if (!File.Exists(oldPath))
                continue;

            string directory = Path.GetDirectoryName(oldPath);
            string fileName = Path.GetFileName(oldPath);

            string newFileName = fileName.Replace(id, newId);
            string newPath = Path.Combine(directory, newFileName);

            RenameFile(oldPath, newPath);
        }
    }

    static void RenameLabelFile(string labelsFolder, string id, string prefix)
    {
        string oldPath = Path.Combine(labelsFolder, id + ".txt");
        string newPath = Path.Combine(labelsFolder, prefix + id + ".txt");
        RenameFile(oldPath, newPath);
    }

    static void RenameIgnoreFile(string ignoreFolder, string id, string prefix)
    {
        string oldPath = Path.Combine(ignoreFolder, id + "_ignored");
        if (!File.Exists(oldPath))
            return;
        string newPath = Path.Combine(ignoreFolder, prefix + id + "_ignored");
        RenameFile(oldPath, newPath);
    }

    static void RenameFile(string oldPath, string newPath)
    {
        if (string.Equals(oldPath, newPath, StringComparison.OrdinalIgnoreCase))
        {
            Console.WriteLine($"Skipping already prefixed file: {oldPath}");
            return;
        }

        if (File.Exists(newPath))
        {
            Console.WriteLine($"Cannot rename because destination already exists: {newPath}");
            return;
        }

        try
        {
            File.Move(oldPath, newPath);
            Console.WriteLine($"Renamed: {oldPath} -> {newPath}");
        }
        catch (Exception ex)
        {
            Console.WriteLine($"Failed to rename {oldPath}: {ex.Message}");
        }
    }
}

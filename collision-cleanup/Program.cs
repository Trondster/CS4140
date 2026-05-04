using System;
using System.IO;
using System.Linq;
using System.Collections.Generic;

class Program
{
    static void Main(string[] args)
    {
        string dataset = @"C:\repos\CS4140\train\dataset";
        string datasetPrev = @"C:\repos\CS4140\train\dataset_prev";
        string prefix = "42004200";

        string datasetLabels = Path.Combine(dataset, "labels");
        string prevLabels = Path.Combine(datasetPrev, "labels");

        if (!Directory.Exists(datasetLabels) || !Directory.Exists(prevLabels))
        {
            Console.WriteLine("Label directories not found.");
            return;
        }

        // Get IDs from dataset_prev
        var prevIds = new HashSet<string>(
            Directory.GetFiles(prevLabels, "*.txt")
                     .Select(f => Path.GetFileNameWithoutExtension(f))
        );

        // Get IDs from dataset
        var datasetIds = Directory.GetFiles(datasetLabels, "*.txt")
                                  .Select(f => Path.GetFileNameWithoutExtension(f));

        int renamedSets = 0;
        foreach (var id in datasetIds)
        {
            if (!prevIds.Contains(id))
                continue;

            ++renamedSets;
            Console.WriteLine($"Processing ID: {id}");

            RenameSet(dataset, id, prefix);
        }

        Console.WriteLine($"Done processing {datasetIds.Count()}, renamed {renamedSets}");
    }

    static void RenameSet(string root, string id, string prefix)
    {
        string newId = prefix + id;

        var possiblePaths = new List<string>
        {
            // label
            Path.Combine(root, "labels", $"{id}.txt"),

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
}
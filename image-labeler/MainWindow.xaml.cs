using System.Globalization;
using System.IO;
using System.IO.Compression;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Input;
using System.Windows.Media.Imaging;
using ImageLabeler.Models;
using Microsoft.Win32;
using Rectangle = System.Windows.Shapes.Rectangle;

namespace ImageLabeler;

public partial class MainWindow : Window
{
    // ── State ──────────────────────────────────────────────────────────────

    private UserSettings _settings = new();

    private List<ImagePair> _clearPairs = new();
    private List<ImagePair> _dronePairs = new();

    // Subsets currently shown in the lists (may be filtered by "hide labeled")
    private List<ImagePair> _visibleClearPairs = new();
    private List<ImagePair> _visibleDronePairs = new();

    private ImagePair? _selectedPair;
    private LabelData? _savedLabel;     // what is on disk for the selected pair
    private HashSet<string> _ignoredIds = new(StringComparer.OrdinalIgnoreCase);

    private string IgnoreFilePath => Path.Combine(DatasetRoot, "ignored.txt");

    private bool _isDragging;
    private Point _dragStart;
    private bool _suppressEvents;
    private bool _updatingLists;

    private const double CanvasW = 320;
    private const double CanvasH = 240;

    // ── Construction ────────────────────────────────────────────────────────

    public MainWindow()
    {
        InitializeComponent();
        _settings = UserSettings.Load();
        FolderPathBox.Text = _settings.DatasetRoot;
        MoveDestBox.Text   = _settings.MoveDestination;
        Loaded += (_, _) => ScanDataset();
    }

    private string DatasetRoot => FolderPathBox.Text.Trim();

    // ── Window events ────────────────────────────────────────────────────────

    private void Window_PreviewKeyDown(object sender, KeyEventArgs e)
    {
        if (Keyboard.FocusedElement is TextBox or CheckBox) return;

        switch (e.Key)
        {
            case Key.Space:
            case Key.Down:
                NavigateTo(FindNextPair());
                e.Handled = true;
                break;
            case Key.Up:
                NavigateTo(FindPreviousPair());
                e.Handled = true;
                break;
        }
    }

    private void NavigateTo(ImagePair? pair)
    {
        if (pair == null) return;
        if (pair.Category == "clear")
            ClearList.SelectedIndex = _visibleClearPairs.FindIndex(p => p.Id == pair.Id);
        else
            DroneList.SelectedIndex = _visibleDronePairs.FindIndex(p => p.Id == pair.Id);
    }

    private ImagePair? FindPreviousPair()
    {
        if (_selectedPair == null) return null;
        var visible = _selectedPair.Category == "clear" ? _visibleClearPairs : _visibleDronePairs;
        int idx = visible.FindIndex(p => p.Id == _selectedPair.Id);
        return idx > 0 ? visible[idx - 1] : null;
    }

    private void Window_Closing(object sender, System.ComponentModel.CancelEventArgs e)
    {
        _settings.DatasetRoot     = DatasetRoot;
        _settings.MoveDestination = MoveDestBox.Text.Trim();
        _settings.Save();
    }

    // ── Toolbar ─────────────────────────────────────────────────────────────

    private void Browse_Click(object sender, RoutedEventArgs e)
    {
        var dlg = new OpenFolderDialog { Title = "Select dataset root folder" };
        if (dlg.ShowDialog() == true)
            FolderPathBox.Text = dlg.FolderName;
    }

    private void MoveDestBrowse_Click(object sender, RoutedEventArgs e)
    {
        var dlg = new OpenFolderDialog { Title = "Select move destination root" };
        if (dlg.ShowDialog() == true)
        {
            MoveDestBox.Text = dlg.FolderName;
            _settings.MoveDestination = dlg.FolderName;
            _settings.Save();
        }
    }

    private void Rescan_Click(object sender, RoutedEventArgs e)
    {
        _settings.DatasetRoot = DatasetRoot;
        _settings.Save();
        ScanDataset();
    }

    private void LabelClearFiles_Click(object sender, RoutedEventArgs e)
    {
        int count = 0;
        foreach (var pair in _clearPairs.Where(p => !p.IsLabeled))
        {
            new LabelData { IsDrone = false }.Save(pair.LabelPath);
            count++;
        }
        string? currentId = _selectedPair?.Id;
        _selectedPair = null;
        RefreshLists(currentId);
        if (_selectedPair == null) ClearDisplay();
        MessageBox.Show($"Created {count} label file(s) for unlabeled clear pair(s).",
            "Label Clear Files", MessageBoxButton.OK, MessageBoxImage.Information);
    }

    // ── Extra view ───────────────────────────────────────────────────────────

    // Folder name under DatasetRoot, or null when "(none)" is selected.
    private string? ExtraViewFolder =>
        (ExtraViewCombo.SelectedItem as ComboBoxItem)?.Tag is string t && t.Length > 0 ? t : null;

    private void ExtraViewCombo_SelectionChanged(object sender, SelectionChangedEventArgs e)
    {
        if (ExtraCurrentLabel == null) return;   // fired during InitializeComponent, ignore

        string? folder = ExtraViewFolder;
        string viewName = (ExtraViewCombo.SelectedItem as ComboBoxItem)?.Content?.ToString() ?? "";

        ExtraCurrentLabel.Text = $"Current Frame ({viewName})";
        ExtraDiffLabel.Text    = $"Diff Frame ({viewName})";
        ExtraImagesPanel.Visibility = folder != null ? Visibility.Visible : Visibility.Collapsed;

        if (_selectedPair != null)
            LoadExtraImages(_selectedPair);

        // Sync bounding-box visibility to the new panel state
        bool boxVisible = BoundingRect1.Visibility == Visibility.Visible && folder != null;
        ExtraBoundingRect1.Visibility =
        ExtraBoundingRect2.Visibility = boxVisible ? Visibility.Visible : Visibility.Hidden;
    }

    private void LoadExtraImages(ImagePair pair)
    {
        string? folder = ExtraViewFolder;
        if (folder == null)
        {
            ExtraImage1.Source = ExtraImage2.Source = null;
            return;
        }
        string root = DatasetRoot;
        string cat  = pair.Category;
        string id   = pair.Id;
        ExtraImage1.Source = TryLoadBitmap(Path.Combine(root, folder, cat, $"{id}_{cat}_current_frame.png"));
        ExtraImage2.Source = TryLoadBitmap(Path.Combine(root, folder, cat, $"{id}_{cat}_diff_frame.png"));
    }

    // ── Dataset scanning ─────────────────────────────────────────────────────

    private void ScanDataset()
    {
        string? restoreId = _selectedPair?.Id;
        _selectedPair = null;

        string root      = DatasetRoot;
        string labelsDir = Path.Combine(root, "labels");
        _clearPairs = ScanCategory(Path.Combine(root, "grey", "clear"), "clear", labelsDir);
        _dronePairs = ScanCategory(Path.Combine(root, "grey", "drone"), "drone", labelsDir);

        LoadIgnoreList();

        foreach (var pair in _clearPairs.Concat(_dronePairs))
        {
            pair.HasMissingFiles = HasMissingExtraFiles(root, pair.Category, pair.Id);
            pair.IsIgnored       = _ignoredIds.Contains(pair.Id);
        }

        RefreshLists(restoreId);
        if (_selectedPair == null) ClearDisplay();
    }

    private static List<ImagePair> ScanCategory(string folder, string category, string labelsDir)
    {
        if (!Directory.Exists(folder)) return new();

        var map = new Dictionary<string, ImagePair>(StringComparer.OrdinalIgnoreCase);
        string sufCurrent = $"_{category}_current_frame";
        string sufDiff    = $"_{category}_diff_frame";

        foreach (string file in Directory.GetFiles(folder, "*.png"))
        {
            string stem = Path.GetFileNameWithoutExtension(file);
            string? id;
            bool isCurrent;

            if (stem.EndsWith(sufCurrent, StringComparison.OrdinalIgnoreCase))
            {
                id = stem[..^sufCurrent.Length];
                isCurrent = true;
            }
            else if (stem.EndsWith(sufDiff, StringComparison.OrdinalIgnoreCase))
            {
                id = stem[..^sufDiff.Length];
                isCurrent = false;
            }
            else continue;

            if (!map.TryGetValue(id, out var pair))
            {
                pair = new ImagePair
                {
                    Id = id,
                    Category = category,
                    LabelPath = Path.Combine(labelsDir, $"{id}.txt")
                };
                map[id] = pair;
            }

            if (isCurrent) pair.CurrentFramePath = file;
            else           pair.DiffFramePath    = file;
        }

        return map.Values
            .Where(p => p.CurrentFramePath != "" && p.DiffFramePath != "")
            .OrderBy(p => int.TryParse(p.Id, out int n) ? n : int.MaxValue)
            .ThenBy(p => p.Id)
            .ToList();
    }

    private static bool HasMissingExtraFiles(string root, string category, string id)
    {
        (string folder, string secondSuffix)[] extras =
        [
            ("2x2", "diff_frame"),
            ("3x3", "diff_frame"),
            ("4x4", "diff_frame"),
        ];
        foreach (var (folder, secondSuffix) in extras)
        {
            if (!File.Exists(Path.Combine(root, folder, category, $"{id}_{category}_current_frame.png")) ||
                !File.Exists(Path.Combine(root, folder, category, $"{id}_{category}_{secondSuffix}.png")))
                return true;
        }
        return false;
    }

    private void LoadIgnoreList()
    {
        _ignoredIds.Clear();
        string path = IgnoreFilePath;
        if (!File.Exists(path)) return;
        foreach (string line in File.ReadAllLines(path))
        {
            string id = line.Trim();
            if (!string.IsNullOrEmpty(id)) _ignoredIds.Add(id);
        }
    }

    private void SaveIgnoreList()
    {
        try
        {
            Directory.CreateDirectory(Path.GetDirectoryName(IgnoreFilePath)!);
            File.WriteAllLines(IgnoreFilePath, _ignoredIds.OrderBy(x => x));
        }
        catch (Exception ex)
        {
            MessageBox.Show($"Could not save ignore list:\n{ex.Message}",
                "Error", MessageBoxButton.OK, MessageBoxImage.Error);
        }
    }

    // ── List population ──────────────────────────────────────────────────────

    // Rebuilds both list views, updates counters, and optionally restores selection.
    // After this returns, _selectedPair is set iff a selection was restored.
    private void RefreshLists(string? restoreId = null)
    {
        bool hideClear        = HideClearLabeled.IsChecked == true;
        bool hideDrone        = HideDroneLabeled.IsChecked == true;
        bool hideIgnoredClear = HideClearIgnored.IsChecked == true;
        bool hideIgnoredDrone = HideDroneIgnored.IsChecked == true;

        _visibleClearPairs = _clearPairs
            .Where(p => (!hideClear        || !p.IsLabeled) && (!hideIgnoredClear || !p.IsIgnored))
            .ToList();
        _visibleDronePairs = _dronePairs
            .Where(p => (!hideDrone        || !p.IsLabeled) && (!hideIgnoredDrone || !p.IsIgnored))
            .ToList();

        UpdateCounter(ClearCounter, _clearPairs);
        UpdateCounter(DroneCounter, _dronePairs);

        ClearList.ItemsSource = null;
        ClearList.ItemsSource = _visibleClearPairs.Select(p => p.DisplayText).ToList();

        DroneList.ItemsSource = null;
        DroneList.ItemsSource = _visibleDronePairs.Select(p => p.DisplayText).ToList();

        if (restoreId == null) return;

        int ci = _visibleClearPairs.FindIndex(p => p.Id == restoreId);
        int di = _visibleDronePairs.FindIndex(p => p.Id == restoreId);

        if      (ci >= 0) ClearList.SelectedIndex = ci;   // fires SelectionChanged → sets _selectedPair
        else if (di >= 0) DroneList.SelectedIndex = di;
    }

    private static void UpdateCounter(TextBlock block, List<ImagePair> pairs)
    {
        int ignored   = pairs.Count(p => p.IsIgnored);
        int labeled   = pairs.Count(p => !p.IsIgnored && p.IsLabeled);
        int unlabeled = pairs.Count(p => !p.IsIgnored && !p.IsLabeled);
        block.Text = $"Labeled: {labeled}\nUnlabeled: {unlabeled}\nIgnored: {ignored}";
    }

    // ── Hide-labeled checkboxes ──────────────────────────────────────────────

    private void HideLabeled_Changed(object sender, RoutedEventArgs e)
    {
        string? restoreId = _selectedPair?.Id;
        _selectedPair = null;
        RefreshLists(restoreId);
        if (_selectedPair == null) ClearDisplay();
    }

    // ── List selection ───────────────────────────────────────────────────────

    private void ClearList_SelectionChanged(object sender, SelectionChangedEventArgs e)
    {
        if (_updatingLists || ClearList.SelectedIndex < 0) return;
        _updatingLists = true;
        DroneList.SelectedIndex = -1;
        _updatingLists = false;

        _selectedPair = _visibleClearPairs[ClearList.SelectedIndex];
        LoadPair(_selectedPair);
    }

    private void DroneList_SelectionChanged(object sender, SelectionChangedEventArgs e)
    {
        if (_updatingLists || DroneList.SelectedIndex < 0) return;
        _updatingLists = true;
        ClearList.SelectedIndex = -1;
        _updatingLists = false;

        _selectedPair = _visibleDronePairs[DroneList.SelectedIndex];
        LoadPair(_selectedPair);
    }

    // ── Pair loading ─────────────────────────────────────────────────────────

    private void LoadPair(ImagePair pair)
    {
        Path1Label.Text = $"Current frame: {pair.CurrentFramePath}";
        Path2Label.Text = $"Diff frame:    {pair.DiffFramePath}";

        CurrentImage1.Source = TryLoadBitmap(pair.CurrentFramePath);
        CurrentImage2.Source = TryLoadBitmap(pair.DiffFramePath);
        LoadExtraImages(pair);

        UpdateIgnoreButton(pair);
        _savedLabel = LabelData.TryLoad(pair.LabelPath);
        ApplyLabel(_savedLabel);
    }

    private static BitmapImage? TryLoadBitmap(string path)
    {
        if (!File.Exists(path)) return null;
        var bmp = new BitmapImage();
        bmp.BeginInit();
        bmp.UriSource = new Uri(path, UriKind.Absolute);
        bmp.CacheOption = BitmapCacheOption.OnLoad;
        bmp.EndInit();
        bmp.Freeze();
        return bmp;
    }

    private void ClearDisplay()
    {
        CurrentImage1.Source = null;
        CurrentImage2.Source = null;
        ExtraImage1.Source   = null;
        ExtraImage2.Source   = null;
        Path1Label.Text = "Current frame: (none)";
        Path2Label.Text = "Diff frame:    (none)";
        UpdateIgnoreButton(null);
        ApplyLabel(null);
    }

    // ── Label → UI ───────────────────────────────────────────────────────────

    private void ApplyLabel(LabelData? label)
    {
        _suppressEvents = true;

        if (label == null)
        {
            IsDroneCheckbox.IsChecked = false;
            XBox.Text = YBox.Text = WBox.Text = HBox.Text = "";
            HideBoxes();
        }
        else
        {
            IsDroneCheckbox.IsChecked = label.IsDrone;
            XBox.Text = Fmt(label.X);
            YBox.Text = Fmt(label.Y);
            WBox.Text = Fmt(label.Width);
            HBox.Text = Fmt(label.Height);

            if (label.IsDrone) ShowBox(label.X, label.Y, label.Width, label.Height);
            else               HideBoxes();
        }

        _suppressEvents = false;
    }

    // ── Bounding-box overlay ─────────────────────────────────────────────────

    private void ShowBox(double xc, double yc, double w, double h)
    {
        double left   = (xc - w / 2) * CanvasW;
        double top    = (yc - h / 2) * CanvasH;
        double width  = w * CanvasW;
        double height = h * CanvasH;
        PlaceRect(BoundingRect1,      left, top, width, height);
        PlaceRect(BoundingRect2,      left, top, width, height);
        PlaceRect(ExtraBoundingRect1, left, top, width, height);
        PlaceRect(ExtraBoundingRect2, left, top, width, height);
        BoundingRect1.Visibility = BoundingRect2.Visibility = Visibility.Visible;
        bool extraShown = ExtraImagesPanel.Visibility == Visibility.Visible;
        ExtraBoundingRect1.Visibility =
        ExtraBoundingRect2.Visibility = extraShown ? Visibility.Visible : Visibility.Hidden;
    }

    private void HideBoxes()
    {
        BoundingRect1.Visibility      = BoundingRect2.Visibility      = Visibility.Hidden;
        ExtraBoundingRect1.Visibility = ExtraBoundingRect2.Visibility = Visibility.Hidden;
    }

    private static void PlaceRect(Rectangle r, double left, double top, double w, double h)
    {
        System.Windows.Controls.Canvas.SetLeft(r, left);
        System.Windows.Controls.Canvas.SetTop(r, top);
        r.Width  = Math.Max(0, w);
        r.Height = Math.Max(0, h);
    }

    // ── Canvas mouse — drag to draw bounding box ─────────────────────────────

    private void Canvas_MouseLeftButtonDown(object sender, MouseButtonEventArgs e)
    {
        if (_selectedPair == null) return;
        _isDragging = true;
        _dragStart  = e.GetPosition((System.Windows.Controls.Canvas)sender);
        ((System.Windows.Controls.Canvas)sender).CaptureMouse();
    }

    private void Canvas_MouseMove(object sender, MouseEventArgs e)
    {
        var canvas = (System.Windows.Controls.Canvas)sender;
        Point pos = e.GetPosition(canvas);
        MoveCrosshair(canvas, pos);
        if (_isDragging) UpdateDrag(pos);
    }

    private void Canvas_MouseEnter(object sender, MouseEventArgs e)
    {
        var canvas = (System.Windows.Controls.Canvas)sender;
        var (h, v) = CrosshairFor(canvas);
        h.Visibility = v.Visibility = Visibility.Visible;
    }

    private void Canvas_MouseLeave(object sender, MouseEventArgs e)
    {
        var canvas = (System.Windows.Controls.Canvas)sender;
        var (h, v) = CrosshairFor(canvas);
        h.Visibility = v.Visibility = Visibility.Hidden;
    }

    private void MoveCrosshair(System.Windows.Controls.Canvas canvas, Point pos)
    {
        var (h, v) = CrosshairFor(canvas);
        h.Y1 = h.Y2 = pos.Y;
        v.X1 = v.X2 = pos.X;
    }

    private (System.Windows.Shapes.Line h, System.Windows.Shapes.Line v) CrosshairFor(
        System.Windows.Controls.Canvas canvas) =>
        canvas == Canvas1 ? (CrossH1, CrossV1) : (CrossH2, CrossV2);

    private void Canvas_MouseLeftButtonUp(object sender, MouseButtonEventArgs e)
    {
        if (!_isDragging) return;
        _isDragging = false;
        var canvas = (System.Windows.Controls.Canvas)sender;
        canvas.ReleaseMouseCapture();
        UpdateDrag(e.GetPosition(canvas));
        canvas.Focus();     // so Enter key is handled by Canvas_KeyDown
    }

    private void Canvas_KeyDown(object sender, KeyEventArgs e)
    {
        if (e.Key == Key.Enter)
        {
            DoSaveAndAdvance();
            e.Handled = true;
        }
    }

    private void UpdateDrag(Point end)
    {
        double left   = Math.Max(0, Math.Min(_dragStart.X, end.X));
        double top    = Math.Max(0, Math.Min(_dragStart.Y, end.Y));
        double right  = Math.Min(CanvasW, Math.Max(_dragStart.X, end.X));
        double bottom = Math.Min(CanvasH, Math.Max(_dragStart.Y, end.Y));
        double w = right - left;
        double h = bottom - top;

        PlaceRect(BoundingRect1,      left, top, w, h);
        PlaceRect(BoundingRect2,      left, top, w, h);
        PlaceRect(ExtraBoundingRect1, left, top, w, h);
        PlaceRect(ExtraBoundingRect2, left, top, w, h);
        BoundingRect1.Visibility = BoundingRect2.Visibility = Visibility.Visible;
        bool extraShown = ExtraImagesPanel.Visibility == Visibility.Visible;
        ExtraBoundingRect1.Visibility =
        ExtraBoundingRect2.Visibility = extraShown ? Visibility.Visible : Visibility.Hidden;

        _suppressEvents = true;
        IsDroneCheckbox.IsChecked = true;
        XBox.Text = Fmt((left + w / 2) / CanvasW);
        YBox.Text = Fmt((top  + h / 2) / CanvasH);
        WBox.Text = Fmt(w / CanvasW);
        HBox.Text = Fmt(h / CanvasH);
        _suppressEvents = false;
    }

    // ── Field-change events ──────────────────────────────────────────────────

    private void BBoxField_TextChanged(object sender, TextChangedEventArgs e)
    {
        if (_suppressEvents) return;
        RefreshBoxFromFields();
    }

    private void IsDroneCheckbox_Changed(object sender, RoutedEventArgs e)
    {
        if (_suppressEvents) return;
        if (IsDroneCheckbox.IsChecked == true) RefreshBoxFromFields();
        else                                   HideBoxes();
    }

    private void RefreshBoxFromFields()
    {
        if (IsDroneCheckbox.IsChecked == true
            && TryParseFields(out double x, out double y, out double w, out double h))
            ShowBox(x, y, w, h);
        else
            HideBoxes();
    }

    // ── Save / Save and Advance / Cancel ────────────────────────────────────

    private void Save_Click(object sender, RoutedEventArgs e)
    {
        string? currentId = _selectedPair?.Id;
        if (!DoSave()) return;
        _selectedPair = null;
        RefreshLists(currentId);
        if (_selectedPair == null) ClearDisplay();
    }

    private void SaveAndAdvance_Click(object sender, RoutedEventArgs e) => DoSaveAndAdvance();

    private void DoSaveAndAdvance()
    {
        // Capture the next visible item BEFORE saving (list may shrink when "hide labeled" is on)
        string? nextId    = FindNextPair()?.Id;
        string? currentId = _selectedPair?.Id;
        if (!DoSave()) return;
        _selectedPair = null;
        RefreshLists(nextId ?? currentId);
        if (_selectedPair == null) ClearDisplay();
    }

    private void Cancel_Click(object sender, RoutedEventArgs e) => ApplyLabel(_savedLabel);

    private void IgnoreToggle_Click(object sender, RoutedEventArgs e)
    {
        var pair = _selectedPair;
        if (pair == null) return;
        if (pair.IsIgnored)
            RemoveFromIgnoreListCore(pair);
        else
            AddToIgnoreListCore(pair);
        _selectedPair = null;
        RefreshLists(pair.Id);
        if (_selectedPair == null) ClearDisplay();
    }

    private void AddToIgnoreListAndAdvance_Click(object sender, RoutedEventArgs e)
    {
        var pair = _selectedPair;
        if (pair == null) return;
        string? nextId = FindNextPair()?.Id;
        AddToIgnoreListCore(pair);
        _selectedPair = null;
        RefreshLists(nextId ?? pair.Id);
        if (_selectedPair == null) ClearDisplay();
    }

    private void AddToIgnoreListCore(ImagePair pair)
    {
        pair.IsIgnored = true;
        _ignoredIds.Add(pair.Id);
        SaveIgnoreList();
    }

    private void RemoveFromIgnoreListCore(ImagePair pair)
    {
        pair.IsIgnored = false;
        _ignoredIds.Remove(pair.Id);
        SaveIgnoreList();
    }

    private void UpdateIgnoreButton(ImagePair? pair)
    {
        IgnoreToggleButton.Content = pair?.IsIgnored == true
            ? "Remove from Ignore List"
            : "Add to Ignore List";
    }

    // ── Zip ──────────────────────────────────────────────────────────────────

    private void CreateZip_Click(object sender, RoutedEventArgs e)
    {
        if (!Directory.Exists(DatasetRoot))
        {
            MessageBox.Show("Set a valid dataset root folder first.",
                "No dataset", MessageBoxButton.OK, MessageBoxImage.Warning);
            return;
        }

        var dlg = new SaveFileDialog
        {
            Title = "Save dataset zip",
            Filter = "ZIP files|*.zip",
            FileName = Path.GetFileName(DatasetRoot) + ".zip"
        };
        if (dlg.ShowDialog() != true) return;

        var excluded = GetIgnoredRelativePaths();

        try
        {
            Mouse.OverrideCursor = Cursors.Wait;

            string zipPath = dlg.FileName;
            if (File.Exists(zipPath)) File.Delete(zipPath);

            int count = 0;
            using (var zip = ZipFile.Open(zipPath, ZipArchiveMode.Create))
            {
                foreach (string file in Directory.GetFiles(DatasetRoot, "*", SearchOption.AllDirectories))
                {
                    string rel = Path.GetRelativePath(DatasetRoot, file).Replace('\\', '/');
                    if (excluded.Contains(rel)) continue;
                    zip.CreateEntryFromFile(file, rel);
                    count++;
                }
            }

            MessageBox.Show($"Created zip with {count} file(s):\n{zipPath}",
                "Zip complete", MessageBoxButton.OK, MessageBoxImage.Information);
        }
        catch (Exception ex)
        {
            MessageBox.Show($"Zip failed:\n{ex.Message}",
                "Error", MessageBoxButton.OK, MessageBoxImage.Error);
        }
        finally
        {
            Mouse.OverrideCursor = null;
        }
    }

    private HashSet<string> GetIgnoredRelativePaths()
    {
        var paths = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
        string[] viewFolders = ["grey", "2x2", "3x3", "4x4"];

        foreach (var pair in _clearPairs.Concat(_dronePairs).Where(p => p.IsIgnored))
        {
            string cat = pair.Category;
            string id  = pair.Id;
            paths.Add($"labels/{id}.txt");
            foreach (string folder in viewFolders)
            {
                paths.Add($"{folder}/{cat}/{id}_{cat}_current_frame.png");
                paths.Add($"{folder}/{cat}/{id}_{cat}_diff_frame.png");
            }
        }
        return paths;
    }

    private void MoveSelected_Click(object sender, RoutedEventArgs e)
    {
        if (_selectedPair == null) return;

        string dest = MoveDestBox.Text.Trim();
        if (string.IsNullOrEmpty(dest))
        {
            MessageBox.Show("Set a move destination folder in the toolbar first.",
                "No destination", MessageBoxButton.OK, MessageBoxImage.Warning);
            return;
        }
        if (string.Equals(Path.GetFullPath(dest), Path.GetFullPath(DatasetRoot),
                StringComparison.OrdinalIgnoreCase))
        {
            MessageBox.Show("Destination must differ from the current dataset root.",
                "Invalid destination", MessageBoxButton.OK, MessageBoxImage.Warning);
            return;
        }

        try
        {
            string? nextId = FindNextPair()?.Id;
            MoveFilesForPair(_selectedPair, DatasetRoot, dest);

            if (_selectedPair.Category == "clear") _clearPairs.Remove(_selectedPair);
            else                                   _dronePairs.Remove(_selectedPair);

            _selectedPair = null;
            RefreshLists(nextId);
            if (_selectedPair == null) ClearDisplay();
        }
        catch (Exception ex)
        {
            MessageBox.Show($"Move failed:\n{ex.Message}", "Error",
                MessageBoxButton.OK, MessageBoxImage.Error);
        }
    }

    private static void MoveFilesForPair(ImagePair pair, string sourceRoot, string destRoot)
    {
        string cat = pair.Category;
        string id  = pair.Id;

        // All view folders and their two frame suffixes
        (string folder, string current, string second)[] views =
        [
            ("grey", "current_frame", "diff_frame"),
            ("2x2",  "current_frame", "diff_frame"),
            ("3x3",  "current_frame", "diff_frame"),
            ("4x4",  "current_frame", "diff_frame"),
        ];

        foreach (var (folder, cur, sec) in views)
        {
            MoveIfExists(
                Path.Combine(sourceRoot, folder, cat, $"{id}_{cat}_{cur}.png"),
                Path.Combine(destRoot,   folder, cat, $"{id}_{cat}_{cur}.png"));
            MoveIfExists(
                Path.Combine(sourceRoot, folder, cat, $"{id}_{cat}_{sec}.png"),
                Path.Combine(destRoot,   folder, cat, $"{id}_{cat}_{sec}.png"));
        }

        // Label file
        MoveIfExists(
            Path.Combine(sourceRoot, "labels", $"{id}.txt"),
            Path.Combine(destRoot,   "labels", $"{id}.txt"));
    }

    private static void MoveIfExists(string src, string dst)
    {
        if (!File.Exists(src)) return;
        Directory.CreateDirectory(Path.GetDirectoryName(dst)!);
        File.Move(src, dst, overwrite: false);
    }

    // Validates, writes the label file, updates _savedLabel. Returns false on failure.
    private bool DoSave()
    {
        if (_selectedPair == null) return false;

        LabelData label;
        if (IsDroneCheckbox.IsChecked == true)
        {
            if (!TryParseFields(out double x, out double y, out double w, out double h))
            {
                MessageBox.Show("Enter valid numeric values for X, Y, W, H before saving.",
                    "Validation", MessageBoxButton.OK, MessageBoxImage.Warning);
                return false;
            }
            label = new LabelData { IsDrone = true, X = x, Y = y, Width = w, Height = h };
        }
        else
        {
            label = new LabelData { IsDrone = false };
        }

        try
        {
            label.Save(_selectedPair.LabelPath);
            _savedLabel = label;
            return true;
        }
        catch (Exception ex)
        {
            MessageBox.Show($"Could not save label:\n{ex.Message}",
                "Error", MessageBoxButton.OK, MessageBoxImage.Error);
            return false;
        }
    }

    // ── Navigation ───────────────────────────────────────────────────────────

    private ImagePair? FindNextPair()
    {
        if (_selectedPair == null) return null;
        var visible = _selectedPair.Category == "clear" ? _visibleClearPairs : _visibleDronePairs;
        int idx = visible.FindIndex(p => p.Id == _selectedPair.Id);
        return idx >= 0 && idx + 1 < visible.Count ? visible[idx + 1] : null;
    }

    // ── Helpers ──────────────────────────────────────────────────────────────

    private bool TryParseFields(out double x, out double y, out double w, out double h)
    {
        x = y = w = h = 0;
        return ParseDouble(XBox.Text, out x) && ParseDouble(YBox.Text, out y)
            && ParseDouble(WBox.Text, out w) && ParseDouble(HBox.Text, out h);
    }

    private static bool ParseDouble(string s, out double v) =>
        double.TryParse(s, NumberStyles.Float, CultureInfo.InvariantCulture, out v);

    private static string Fmt(double v) =>
        v.ToString("F6", CultureInfo.InvariantCulture);
}

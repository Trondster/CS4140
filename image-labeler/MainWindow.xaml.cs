using System.Globalization;
using System.IO;
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
        Loaded += (_, _) => ScanDataset();
    }

    private string DatasetRoot => FolderPathBox.Text.Trim();

    // ── Window events ────────────────────────────────────────────────────────

    private void Window_PreviewKeyDown(object sender, KeyEventArgs e)
    {
        if (Keyboard.FocusedElement is TextBox or CheckBox or Button) return;

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
        _settings.DatasetRoot = DatasetRoot;
        _settings.Save();
    }

    // ── Toolbar ─────────────────────────────────────────────────────────────

    private void Browse_Click(object sender, RoutedEventArgs e)
    {
        var dlg = new OpenFolderDialog { Title = "Select dataset root folder" };
        if (dlg.ShowDialog() == true)
            FolderPathBox.Text = dlg.FolderName;
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

    // ── Dataset scanning ─────────────────────────────────────────────────────

    private void ScanDataset()
    {
        string? restoreId = _selectedPair?.Id;
        _selectedPair = null;

        string root      = DatasetRoot;
        string labelsDir = Path.Combine(root, "labels");
        _clearPairs = ScanCategory(Path.Combine(root, "grey", "clear"), "clear", labelsDir);
        _dronePairs = ScanCategory(Path.Combine(root, "grey", "drone"), "drone", labelsDir);

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

    // ── List population ──────────────────────────────────────────────────────

    // Rebuilds both list views, updates counters, and optionally restores selection.
    // After this returns, _selectedPair is set iff a selection was restored.
    private void RefreshLists(string? restoreId = null)
    {
        bool hideClear = HideClearLabeled.IsChecked == true;
        bool hideDrone = HideDroneLabeled.IsChecked == true;

        _visibleClearPairs = hideClear
            ? _clearPairs.Where(p => !p.IsLabeled).ToList()
            : _clearPairs.ToList();
        _visibleDronePairs = hideDrone
            ? _dronePairs.Where(p => !p.IsLabeled).ToList()
            : _dronePairs.ToList();

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
        int labeled   = pairs.Count(p => p.IsLabeled);
        int unlabeled = pairs.Count - labeled;
        block.Text = $"Labeled: {labeled}\nUnlabeled: {unlabeled}";
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
        Path1Label.Text = "Current frame: (none)";
        Path2Label.Text = "Diff frame:    (none)";
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
        PlaceRect(BoundingRect1, left, top, width, height);
        PlaceRect(BoundingRect2, left, top, width, height);
        BoundingRect1.Visibility = BoundingRect2.Visibility = Visibility.Visible;
    }

    private void HideBoxes()
    {
        BoundingRect1.Visibility = BoundingRect2.Visibility = Visibility.Hidden;
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

        PlaceRect(BoundingRect1, left, top, w, h);
        PlaceRect(BoundingRect2, left, top, w, h);
        BoundingRect1.Visibility = BoundingRect2.Visibility = Visibility.Visible;

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

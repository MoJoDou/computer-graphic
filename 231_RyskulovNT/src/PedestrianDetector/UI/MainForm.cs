using System;
using System.Collections.Generic;
using System.Drawing;
using System.IO;
using System.Threading.Tasks;
using System.Windows.Forms;
using PedestrianDetector.Core;

namespace PedestrianDetector.UI
{
    public class MainForm : Form
    {
         // Controls
        
        private TabControl _tabs = null!;

        // Tab 1 – Train
        private TextBox  _trainImgDir = null!;
        private TextBox  _trainAnnFile = null!;
        private TextBox  _trainModelOut = null!;
        private NumericUpDown _svmC = null!;
        private NumericUpDown _svmIter = null!;
        private NumericUpDown _negPerImg = null!;
        private CheckBox _doBootstrap = null!;
        private CheckBox _doCrossVal = null!;
        private Button   _btnTrain = null!;
        private RichTextBox _trainLog = null!;

        // Tab 2 – Detect
        private TextBox  _detImgDir = null!;
        private TextBox  _detModelFile = null!;
        private TextBox  _detOutFile = null!;
        private TextBox  _gtFile = null!;
        private NumericUpDown _detStep = null!;
        private NumericUpDown _detThresh = null!;
        private CheckBox _doNms = null!;
        private Button   _btnDetect = null!;
        private RichTextBox _detLog = null!;

        // Tab 3 – Preview
        private PictureBox _preview = null!;
        private Button   _btnLoadImg = null!;
        private Button   _btnLoadModel = null!;
        private Button   _btnRunPreview = null!;
        private Label    _lblModelStatus = null!;
        private NumericUpDown _prevThresh = null!;
        private string?  _previewImagePath;
        private SvmClassifier? _loadedClassifier;

    

        public MainForm()
        {
            Text = "Pedestrian Detector — 231_RyskulovNT";
            Size = new Size(860, 680);
            MinimumSize = new Size(760, 600);
            BuildUI();
        }

        //  UI CONSTRUCTION 

        private void BuildUI()
        {
            _tabs = new TabControl { Dock = DockStyle.Fill };
            Controls.Add(_tabs);

            _tabs.TabPages.Add(BuildTrainTab());
            _tabs.TabPages.Add(BuildDetectTab());
            _tabs.TabPages.Add(BuildPreviewTab());
        }

        //  Train Tab 

        private TabPage BuildTrainTab()
        {
            var page = new TabPage("Обучение");
            var panel = new TableLayoutPanel
            {
                Dock = DockStyle.Fill, ColumnCount = 3, RowCount = 10,
                Padding = new Padding(8)
            };
            panel.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 200));
            panel.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));
            panel.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 90));

            int row = 0;

            _trainImgDir  = AddFileRow(panel, row++, "Папка с обуч. изображениями:", true, null);
            _trainAnnFile = AddFileRow(panel, row++, "Файл аннотаций:", false, "txt files|*.txt|all|*.*");
            _trainModelOut = AddFileRow(panel, row++, "Сохранить модель в:", false, "zip|*.zip|all|*.*",
                                        isSave: true);

            // Hyperparams
            _svmC     = AddNumRow(panel, row++, "SVM C (lambda):", 0.01m, 0.0001m, 10m, 4);
            _svmIter  = AddNumRow(panel, row++, "Итераций SVM:", 100, 10, 2000, 0);
            _negPerImg = AddNumRow(panel, row++, "Негативов на изображение:", 15, 1, 200, 0);

            _doBootstrap = AddCheckRow(panel, row++, "Bootstrapping (hard negatives)");
            _doCrossVal  = AddCheckRow(panel, row++, "Кросс-валидация параметра C");

            _btnTrain = new Button { Text = "Study", Height = 34, Margin = new Padding(4) };
            _btnTrain.Click += OnTrainClicked;
            panel.Controls.Add(_btnTrain, 0, row);
            panel.SetColumnSpan(_btnTrain, 3);
            row++;

            _trainLog = new RichTextBox
            {
                Dock = DockStyle.Fill, ReadOnly = true,
                BackColor = Color.FromArgb(20, 20, 20), ForeColor = Color.LightGreen,
                Font = new Font("Consolas", 9f)
            };
            panel.Controls.Add(_trainLog, 0, row);
            panel.SetColumnSpan(_trainLog, 3);
            panel.RowStyles.Add(new RowStyle(SizeType.Percent, 100));

            page.Controls.Add(panel);
            return page;
        }

        //  Detect Tab 

        private TabPage BuildDetectTab()
        {
            var page = new TabPage("Детектирование");
            var panel = new TableLayoutPanel
            {
                Dock = DockStyle.Fill, ColumnCount = 3, RowCount = 10,
                Padding = new Padding(8)
            };
            panel.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 200));
            panel.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));
            panel.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 90));

            int row = 0;
            _detImgDir   = AddFileRow(panel, row++, "Папка с изображениями:", true, null);
            _detModelFile = AddFileRow(panel, row++, "Файл модели:", false, "zip|*.zip|all|*.*");
            _detOutFile  = AddFileRow(panel, row++, "Файл результатов:", false, "txt|*.txt|all|*.*",
                                       isSave: true);
            _gtFile      = AddFileRow(panel, row++, "GT файл (для оценки, необяз.):", false,
                                       "txt|*.txt|all|*.*");

            _detStep   = AddNumRow(panel, row++, "Шаг окна (пикселей):", 4, 1, 40, 0);
            _detThresh = AddNumRow(panel, row++, "Порог классификатора:", 0m, -10m, 10m, 2);
            _doNms     = AddCheckRow(panel, row++, "Подавление немаксимумов (NMS)");

            _btnDetect = new Button { Text = "Detect", Height = 34, Margin = new Padding(4) };
            _btnDetect.Click += OnDetectClicked;
            panel.Controls.Add(_btnDetect, 0, row);
            panel.SetColumnSpan(_btnDetect, 3);
            row++;

            _detLog = new RichTextBox
            {
                Dock = DockStyle.Fill, ReadOnly = true,
                BackColor = Color.FromArgb(20, 20, 20), ForeColor = Color.LightGreen,
                Font = new Font("Consolas", 9f)
            };
            panel.Controls.Add(_detLog, 0, row);
            panel.SetColumnSpan(_detLog, 3);
            panel.RowStyles.Add(new RowStyle(SizeType.Percent, 100));

            page.Controls.Add(panel);
            return page;
        }

        // ---- Preview Tab -------------------------------------------------

        private TabPage BuildPreviewTab()
        {
            var page = new TabPage("Просмотр");
            var top  = new FlowLayoutPanel { Dock = DockStyle.Top, Height = 42, Padding = new Padding(4) };

            _btnLoadImg   = new Button { Text = "Загрузить изображение", Height = 30, AutoSize = true };
            _btnLoadModel = new Button { Text = "Загрузить модель",       Height = 30, AutoSize = true };
            _prevThresh   = new NumericUpDown
            {
                Minimum = -10m, Maximum = 10m, DecimalPlaces = 2,
                Increment = 0.1m, Value = 0m, Width = 80, Height = 30
            };
            _lblModelStatus = new Label
            {
                Text = "Модель не загружена", ForeColor = Color.Gray,
                AutoSize = true, TextAlign = ContentAlignment.MiddleLeft
            };
            _btnRunPreview = new Button
            {
                Text = "Запустить детектор", Height = 30, AutoSize = true,
                BackColor = Color.FromArgb(0, 122, 204), ForeColor = Color.White
            };

            _btnLoadImg.Click   += OnLoadImageClicked;
            _btnLoadModel.Click += OnLoadModelClicked;
            _btnRunPreview.Click += OnRunPreviewClicked;

            top.Controls.AddRange(new Control[]
            {
                _btnLoadImg, _btnLoadModel,
                new Label { Text = "Порог:", AutoSize = true, TextAlign = ContentAlignment.MiddleLeft, Height = 30 },
                _prevThresh, _btnRunPreview, _lblModelStatus
            });

            _preview = new PictureBox
            {
                Dock = DockStyle.Fill, SizeMode = PictureBoxSizeMode.Zoom,
                BackColor = Color.FromArgb(40, 40, 40)
            };

            page.Controls.Add(_preview);
            page.Controls.Add(top);
            return page;
        }

        //  EVENT HANDLERS 

        private async void OnTrainClicked(object? sender, EventArgs e)
        {
            _btnTrain.Enabled = false;
            _trainLog.Clear();

            string imgDir   = _trainImgDir.Text.Trim();
            string annFile  = _trainAnnFile.Text.Trim();
            string modelOut = _trainModelOut.Text.Trim();

            if (!Directory.Exists(imgDir))  { MsgErr("Папка изображений не найдена"); _btnTrain.Enabled = true; return; }
            if (!File.Exists(annFile))       { MsgErr("Файл аннотаций не найден");     _btnTrain.Enabled = true; return; }
            if (string.IsNullOrEmpty(modelOut)) { MsgErr("Укажите путь для сохранения модели"); _btnTrain.Enabled = true; return; }

            float  c        = (float)_svmC.Value;
            int    iter     = (int)_svmIter.Value;
            int    negPer   = (int)_negPerImg.Value;
            bool   boot     = _doBootstrap.Checked;
            bool   crossval = _doCrossVal.Checked;

            await Task.Run(() =>
            {
                void Log(string s) => Invoke(() => { _trainLog.AppendText(s + "\n"); _trainLog.ScrollToCaret(); });

                try
                {
                    Log("Сбор обучающих образцов...");
                    var (pos, neg) = DataLoader.CollectTrainingSamples(imgDir, annFile, negPer);
                    Log($"  + {pos.Count} пешеходов, {neg.Count} фонов");

                    var clf = new SvmClassifier();

                    if (crossval)
                    {
                        Log("Кросс-валидация C...");
                        float[] cVals = { 0.0001f, 0.001f, 0.01f, 0.1f, 1f };
                        var cvRes = clf.CrossValidate(pos, neg, cVals, log: Log);
                        float bestC = 0.01f; double bestAcc = 0;
                        foreach (var kv in cvRes) if (kv.Value > bestAcc) { bestAcc = kv.Value; bestC = kv.Key; }
                        Log($"Лучший C={bestC} (acc={bestAcc:F4}), использую для обучения.");
                        c = bestC;
                    }

                    Log("Обучение SVM...");
                    clf.Train(pos, neg, svmC: c, iterations: iter, log: Log);

                    if (boot)
                    {
                        Log("Bootstrapping: сбор трудных примеров...");
                        var hardNegs = DataLoader.CollectHardNegatives(imgDir, annFile, clf, 2000);
                        Log($"  Найдено {hardNegs.Count} трудных негативов, переобучаем...");
                        neg.AddRange(hardNegs);
                        clf.Train(pos, neg, svmC: c, iterations: iter, log: Log);
                    }

                    clf.Save(modelOut);
                    Log($"\n✓ Модель сохранена в: {modelOut}");
                }
                catch (Exception ex)
                {
                    Log("ОШИБКА: " + ex.Message);
                }
            });

            _btnTrain.Enabled = true;
        }

        private async void OnDetectClicked(object? sender, EventArgs e)
        {
            _btnDetect.Enabled = false;
            _detLog.Clear();

            string imgDir    = _detImgDir.Text.Trim();
            string modelFile = _detModelFile.Text.Trim();
            string outFile   = _detOutFile.Text.Trim();
            string gtFile    = _gtFile.Text.Trim();
            int    step      = (int)_detStep.Value;
            float  thresh    = (float)_detThresh.Value;
            bool   nms       = _doNms.Checked;

            if (!Directory.Exists(imgDir))  { MsgErr("Папка изображений не найдена"); _btnDetect.Enabled = true; return; }
            if (!File.Exists(modelFile))     { MsgErr("Файл модели не найден");         _btnDetect.Enabled = true; return; }

            await Task.Run(() =>
            {
                void Log(string s) => Invoke(() => { _detLog.AppendText(s + "\n"); _detLog.ScrollToCaret(); });

                try
                {
                    var clf = new SvmClassifier();
                    clf.Load(modelFile);
                    Log("Модель загружена.");

                    var allDet = new List<PedestrianAnnotation>();
                    var images = Directory.GetFiles(imgDir, "*.png");
                    Log($"Обрабатываю {images.Length} изображений...");

                    foreach (var imgPath in images)
                    {
                        string fname = Path.GetFileNameWithoutExtension(imgPath);
                        using var bmp = new Bitmap(imgPath);
                        var dets = Detector.Detect(bmp, clf, step, thresh);
                        if (nms) dets = Detector.NonMaxSuppression(dets, 40, thresh);

                        foreach (var d in dets)
                            allDet.Add(new PedestrianAnnotation(fname, d.X0, d.X1));
                        Log($"  {fname}: {dets.Count} обнаружений");
                    }

                    if (!string.IsNullOrEmpty(outFile))
                    {
                        DataLoader.SaveAnnotations(outFile, allDet);
                        Log($"Результаты сохранены: {outFile}");
                    }

                    if (File.Exists(gtFile))
                    {
                        var eval = Evaluator.EvaluateFiles(gtFile, outFile);
                        Log("\n=== Качество ===");
                        Log(eval.ToString());
                    }
                }
                catch (Exception ex) { Log("ОШИБКА: " + ex.Message); }
            });

            _btnDetect.Enabled = true;
        }

        private void OnLoadImageClicked(object? sender, EventArgs e)
        {
            using var dlg = new OpenFileDialog { Filter = "PNG|*.png|Images|*.jpg;*.bmp;*.png" };
            if (dlg.ShowDialog() != DialogResult.OK) return;
            _previewImagePath = dlg.FileName;
            _preview.Image?.Dispose();
            _preview.Image = new Bitmap(dlg.FileName);
        }

        private void OnLoadModelClicked(object? sender, EventArgs e)
        {
            using var dlg = new OpenFileDialog { Filter = "zip|*.zip|all|*.*" };
            if (dlg.ShowDialog() != DialogResult.OK) return;
            try
            {
                _loadedClassifier = new SvmClassifier();
                _loadedClassifier.Load(dlg.FileName);
                _lblModelStatus.Text = "✓ " + Path.GetFileName(dlg.FileName);
                _lblModelStatus.ForeColor = Color.Green;
            }
            catch (Exception ex)
            {
                MsgErr("Ошибка загрузки: " + ex.Message);
                _loadedClassifier = null;
            }
        }

        private async void OnRunPreviewClicked(object? sender, EventArgs e)
        {
            if (_previewImagePath == null || !File.Exists(_previewImagePath))
            { MsgErr("Сначала загрузите изображение"); return; }
            if (_loadedClassifier == null)
            { MsgErr("Сначала загрузите модель"); return; }

            _btnRunPreview.Enabled = false;
            float thresh = (float)_prevThresh.Value;

            var clf = _loadedClassifier;
            var imgPath = _previewImagePath;

            var dets = await Task.Run(() =>
            {
                using var bmp = new Bitmap(imgPath);
                var raw = Detector.Detect(bmp, clf, step: 4, threshold: thresh);
                return Detector.NonMaxSuppression(raw, 40, thresh);
            });

            // Draw result
            var bmpDraw = new Bitmap(_previewImagePath);
            using var g = Graphics.FromImage(bmpDraw);
            using var pen = new Pen(Color.Lime, 2);

            foreach (var d in dets)
                g.DrawRectangle(pen, d.X0, 0, HogDescriptor.PatchWidth, HogDescriptor.PatchHeight);

            _preview.Image?.Dispose();
            _preview.Image = bmpDraw;

            _btnRunPreview.Enabled = true;
            MessageBox.Show($"Найдено пешеходов: {dets.Count}", "Детектирование завершено",
                MessageBoxButtons.OK, MessageBoxIcon.Information);
        }

        //  UI HELPERS 

        private TextBox AddFileRow(TableLayoutPanel p, int row, string label,
            bool isDir, string? filter, bool isSave = false)
        {
            p.Controls.Add(new Label
            {
                Text = label, Dock = DockStyle.Fill,
                TextAlign = ContentAlignment.MiddleRight, Margin = new Padding(2)
            }, 0, row);

            var tb = new TextBox { Dock = DockStyle.Fill, Margin = new Padding(2) };
            p.Controls.Add(tb, 1, row);

            var btn = new Button { Text = "...", Width = 30, Height = 24, Margin = new Padding(2) };
            btn.Click += (_, _) =>
            {
                if (isDir)
                {
                    using var dlg = new FolderBrowserDialog();
                    if (dlg.ShowDialog() == DialogResult.OK) tb.Text = dlg.SelectedPath;
                }
                else if (isSave)
                {
                    using var dlg = new SaveFileDialog { Filter = filter ?? "all|*.*" };
                    if (dlg.ShowDialog() == DialogResult.OK) tb.Text = dlg.FileName;
                }
                else
                {
                    using var dlg = new OpenFileDialog { Filter = filter ?? "all|*.*" };
                    if (dlg.ShowDialog() == DialogResult.OK) tb.Text = dlg.FileName;
                }
            };
            p.Controls.Add(btn, 2, row);
            return tb;
        }

        private NumericUpDown AddNumRow(TableLayoutPanel p, int row, string label,
            decimal value, decimal min, decimal max, int decimals)
        {
            p.Controls.Add(new Label
            {
                Text = label, Dock = DockStyle.Fill,
                TextAlign = ContentAlignment.MiddleRight, Margin = new Padding(2)
            }, 0, row);

            var num = new NumericUpDown
            {
                Minimum = min, Maximum = max, Value = value,
                DecimalPlaces = decimals, Dock = DockStyle.Fill, Margin = new Padding(2)
            };
            p.Controls.Add(num, 1, row);
            return num;
        }

        private CheckBox AddCheckRow(TableLayoutPanel p, int row, string text)
        {
            var cb = new CheckBox { Text = text, Checked = true, Margin = new Padding(2) };
            p.Controls.Add(cb, 1, row);
            p.SetColumnSpan(cb, 2);
            return cb;
        }

        private void MsgErr(string msg) =>
            MessageBox.Show(msg, "Ошибка", MessageBoxButtons.OK, MessageBoxIcon.Warning);
    }
}

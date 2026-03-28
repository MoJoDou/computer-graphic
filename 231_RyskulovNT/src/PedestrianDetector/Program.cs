using System;
using System.Collections.Generic;
using System.IO;
using System.Windows.Forms;
using PedestrianDetector.Core;
using PedestrianDetector.UI;

namespace PedestrianDetector
{
    internal static class Program
    {
        [STAThread]
        static void Main(string[] args)
        {
            if (args.Length == 0)
            {
                Application.EnableVisualStyles();
                Application.SetCompatibleTextRenderingDefault(false);
                Application.Run(new MainForm());
                return;
            }

            switch (args[0].ToLower())
            {
                case "train":
                    RunTrain(args);
                    break;
                case "detect":
                    RunDetect(args);
                    break;
                case "eval":
                    RunEval(args);
                    break;
                default:
                    PrintUsage();
                    break;
            }
        }


        static void RunTrain(string[] args)
        {
            if (args.Length < 4) { PrintUsage(); return; }

            string imgDir    = args[1];
            string annFile   = args[2];
            string modelOut  = args[3];
            float  c         = args.Length > 4 ? float.Parse(args[4]) : 0.01f;
            int    iter      = args.Length > 5 ? int.Parse(args[5])   : 100;
            bool   bootstrap = args.Length > 6 && args[6] == "1";

            Console.WriteLine($"Collecting samples from {imgDir}...");
            var (pos, neg) = DataLoader.CollectTrainingSamples(imgDir, annFile, negPerImage: 15);
            Console.WriteLine($"  Positives: {pos.Count}  Negatives: {neg.Count}");

            var clf = new SvmClassifier();
            Console.WriteLine("Training SVM...");
            clf.Train(pos, neg, svmC: c, iterations: iter, log: Console.WriteLine);

            if (bootstrap)
            {
                Console.WriteLine("Bootstrapping...");
                var hardNegs = DataLoader.CollectHardNegatives(imgDir, annFile, clf, 2000);
                Console.WriteLine($"  Hard negatives collected: {hardNegs.Count}");
                neg.AddRange(hardNegs);
                clf.Train(pos, neg, svmC: c, iterations: iter, log: Console.WriteLine);
            }

            clf.Save(modelOut);
            Console.WriteLine($"Model saved to: {modelOut}");
        }

        static void RunDetect(string[] args)
        {
            if (args.Length < 4) { PrintUsage(); return; }

            string imgDir    = args[1];
            string modelFile = args[2];
            string outFile   = args[3];
            int    step      = args.Length > 4 ? int.Parse(args[4])   : 4;
            float  thresh    = args.Length > 5 ? float.Parse(args[5]) : 0f;
            bool   nms       = args.Length <= 6 || args[6] != "0";
            string? gtFile   = args.Length > 7 ? args[7] : null;

            var clf = new SvmClassifier();
            clf.Load(modelFile);
            Console.WriteLine("Model loaded.");

            var allDet = new List<PedestrianAnnotation>();
            foreach (var imgPath in Directory.GetFiles(imgDir, "*.png"))
            {
                string fname = Path.GetFileNameWithoutExtension(imgPath);
                using var bmp = new System.Drawing.Bitmap(imgPath);
                var dets = Detector.Detect(bmp, clf, step, thresh);
                if (nms) dets = Detector.NonMaxSuppression(dets, 40, thresh);

                foreach (var d in dets)
                    allDet.Add(new PedestrianAnnotation(fname, d.X0, d.X1));
                Console.WriteLine($"  {fname}: {dets.Count} detections");
            }

            DataLoader.SaveAnnotations(outFile, allDet);
            Console.WriteLine($"Results saved to: {outFile}");

            if (gtFile != null && File.Exists(gtFile))
            {
                var eval = Evaluator.EvaluateFiles(gtFile, outFile);
                Console.WriteLine("\n=== Evaluation ===");
                Console.WriteLine(eval.ToString());
            }
        }

        static void RunEval(string[] args)
        {
            if (args.Length < 3) { PrintUsage(); return; }
            var eval = Evaluator.EvaluateFiles(args[1], args[2]);
            Console.WriteLine(eval.ToString());
        }

        static void PrintUsage()
        {
            Console.WriteLine("Usage:");
            Console.WriteLine("  PedestrianDetector                                           (GUI)");
            Console.WriteLine("  PedestrianDetector train  <imgDir> <ann> <model> [C] [iter] [boot]");
            Console.WriteLine("  PedestrianDetector detect <imgDir> <model> <out> [step] [thresh] [nms] [gt]");
            Console.WriteLine("  PedestrianDetector eval   <gtFile> <detFile>");
        }
    }
}

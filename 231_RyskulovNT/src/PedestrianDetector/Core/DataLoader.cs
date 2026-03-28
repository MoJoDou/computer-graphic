using System;
using System.Collections.Generic;
using System.Drawing;
using System.IO;

namespace PedestrianDetector.Core
{
    public record PedestrianAnnotation(string FileName, int X0, int X1);

    public static class DataLoader
    {


        public static List<PedestrianAnnotation> LoadAnnotations(string annotationFile)
        {
            var result = new List<PedestrianAnnotation>();
            foreach (var line in File.ReadAllLines(annotationFile))
            {
                var parts = line.Trim().Split(new[] { ' ', '\t' }, StringSplitOptions.RemoveEmptyEntries);
                if (parts.Length < 5) continue;
                string fname = parts[0];
                int x0 = int.Parse(parts[2]);
                int x1 = int.Parse(parts[4]);
                result.Add(new PedestrianAnnotation(fname, x0, x1));
            }
            return result;
        }

        public static void SaveAnnotations(string outputFile, IEnumerable<PedestrianAnnotation> annotations)
        {
            using var sw = new StreamWriter(outputFile);
            foreach (var ann in annotations)
                sw.WriteLine($"{ann.FileName} 0 {ann.X0} 200 {ann.X1}");
        }

        // Build training data
     
        public static (List<float[]> positives, List<float[]> negatives) CollectTrainingSamples(
            string imageDir,
            string annotationFile,
            int negPerImage = 10,
            Random? rng = null)
        {
            rng ??= new Random(42);
            var annotations = LoadAnnotations(annotationFile);

            // Group annotations by file
            var byFile = new Dictionary<string, List<PedestrianAnnotation>>();
            foreach (var ann in annotations)
            {
                if (!byFile.ContainsKey(ann.FileName))
                    byFile[ann.FileName] = new List<PedestrianAnnotation>();
                byFile[ann.FileName].Add(ann);
            }

            var positives = new List<float[]>();
            var negatives = new List<float[]>();

            foreach (var (fname, anns) in byFile)
            {
                string imgPath = Path.Combine(imageDir, fname + ".png");
                if (!File.Exists(imgPath)) continue;

                using var bmp = new Bitmap(imgPath);
                var gray = HogDescriptor.ToGrayscale(bmp);
                int W = bmp.Width;

                // Positives
                foreach (var ann in anns)
                {
                    var patch = HogDescriptor.CropPatch(gray, ann.X0);
                    positives.Add(HogDescriptor.ComputeFromGray(patch));
                }

                // Negatives — random windows not overlapping pedestrians
                var pedXRanges = new List<(int left, int right)>();
                foreach (var ann in anns)
                    pedXRanges.Add((ann.X0, ann.X1));

                int attempts = 0;
                int collected = 0;
                while (collected < negPerImage && attempts < negPerImage * 20)
                {
                    attempts++;
                    int maxX = W - HogDescriptor.PatchWidth;
                    if (maxX <= 0) break;
                    int x0 = rng.Next(0, maxX);
                    int x1 = x0 + HogDescriptor.PatchWidth;

                    bool overlaps = false;
                    foreach (var (pl, pr) in pedXRanges)
                    {
                        int overlapLeft  = Math.Max(x0, pl);
                        int overlapRight = Math.Min(x1, pr);
                        if (overlapRight - overlapLeft > 40) { overlaps = true; break; }
                    }
                    if (overlaps) continue;

                    var patch = HogDescriptor.CropPatch(gray, x0);
                    negatives.Add(HogDescriptor.ComputeFromGray(patch));
                    collected++;
                }
            }

            return (positives, negatives);
        }

  
        public static List<float[]> CollectHardNegatives(
            string imageDir,
            string annotationFile,
            SvmClassifier classifier,
            int maxHardNegs = 2000,
            float scoreThreshold = 0f)
        {
            var annotations = LoadAnnotations(annotationFile);
            var byFile = new Dictionary<string, List<PedestrianAnnotation>>();
            foreach (var ann in annotations)
            {
                if (!byFile.ContainsKey(ann.FileName))
                    byFile[ann.FileName] = new List<PedestrianAnnotation>();
                byFile[ann.FileName].Add(ann);
            }

            var hardNegs = new List<float[]>();

            foreach (var (fname, anns) in byFile)
            {
                if (hardNegs.Count >= maxHardNegs) break;

                string imgPath = Path.Combine(imageDir, fname + ".png");
                if (!File.Exists(imgPath)) continue;

                using var bmp = new Bitmap(imgPath);
                var gray = HogDescriptor.ToGrayscale(bmp);
                int W = bmp.Width;

                var pedXRanges = new List<(int l, int r)>();
                foreach (var ann in anns) pedXRanges.Add((ann.X0, ann.X1));

                for (int x = 0; x <= W - HogDescriptor.PatchWidth; x += 4)
                {
                    // Skip if overlaps ground truth
                    bool isTruePos = false;
                    foreach (var (pl, pr) in pedXRanges)
                    {
                        if (Math.Abs(x - pl) <= 40) { isTruePos = true; break; }
                    }
                    if (isTruePos) continue;

                    var patch = HogDescriptor.CropPatch(gray, x);
                    var feat  = HogDescriptor.ComputeFromGray(patch);
                    float score = classifier.PredictScore(feat);
                    if (score > scoreThreshold)
                    {
                        hardNegs.Add(feat);
                        if (hardNegs.Count >= maxHardNegs) break;
                    }
                }
            }

            return hardNegs;
        }
    }
}

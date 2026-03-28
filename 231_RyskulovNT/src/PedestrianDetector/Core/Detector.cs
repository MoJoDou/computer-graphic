using System;
using System.Collections.Generic;
using System.Drawing;

namespace PedestrianDetector.Core
{
    public record Detection(int X0, float Score)
    {
        public int X1 => X0 + HogDescriptor.PatchWidth;
    }

    public static class Detector
    {
        // Sliding window detection on a single image

        public static List<Detection> Detect(
            Bitmap image,
            SvmClassifier classifier,
            int step = 4,
            float threshold = 0f)
        {
            var gray = HogDescriptor.ToGrayscale(image);
            return Detect(gray, image.Width, classifier, step, threshold);
        }

        public static List<Detection> Detect(
            float[,] gray,
            int imageWidth,
            SvmClassifier classifier,
            int step = 4,
            float threshold = 0f)
        {
            var detections = new List<Detection>();
            int maxX = imageWidth - HogDescriptor.PatchWidth;

            for (int x = 0; x <= maxX; x += step)
            {
                var patch = HogDescriptor.CropPatch(gray, x);
                var feat  = HogDescriptor.ComputeFromGray(patch);
                float score = classifier.PredictScore(feat);
                if (score > threshold)
                    detections.Add(new Detection(x, score));
            }

            return detections;
        }

        // Non-Maximum Suppression (greedy, 1-D overlap on x-axis)
        


        public static List<Detection> NonMaxSuppression(
            List<Detection> detections,
            int suppressRadius = 40,
            float threshold = 0f)
        {
            if (detections.Count == 0) return detections;

            // Sort by score descending
            var sorted = new List<Detection>(detections);
            sorted.Sort((a, b) => b.Score.CompareTo(a.Score));

            var result   = new List<Detection>();
            var suppressed = new bool[sorted.Count];

            for (int i = 0; i < sorted.Count; i++)
            {
                if (suppressed[i]) continue;
                if (sorted[i].Score <= threshold) break;

                result.Add(sorted[i]);

                for (int j = i + 1; j < sorted.Count; j++)
                {
                    if (suppressed[j]) continue;
                    if (Math.Abs(sorted[j].X0 - sorted[i].X0) <= suppressRadius)
                        suppressed[j] = true;
                }
            }

            return result;
        }
    }
}

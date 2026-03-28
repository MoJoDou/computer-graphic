using System;
using System.Collections.Generic;
using System.IO;

namespace PedestrianDetector.Core
{
    public record EvalResult(int TP, int FP, int FN, int GT, int DET)
    {
        public double Precision => DET > 0 ? (double)TP / DET : 0;
        public double Recall    => GT  > 0 ? (double)TrueGT / GT : 0;
        // Number of ground-truth pedestrians found (≠ TP detections count)
        public int TrueGT { get; init; }

        public double FMeasure =>
            (Precision + Recall) > 0
                ? 2 * Precision * Recall / (Precision + Recall)
                : 0;

        public override string ToString() =>
            $"Precision={Precision:P1}  Recall={Recall:P1}  F1={FMeasure:P1}  " +
            $"(TP={TP} FP={FP} FN={FN} GT={GT} DET={DET})";
    }

    public static class Evaluator
    {
        
        // Overlap criterion:
        //   TP detection  ↔  |det.X0 - gt.X0| ≤ 40 (≥50% IoU in x-axis for 80px windows)
        //   A GT pedestrian is found if any detection matches it.
       
        private const int OverlapThreshold = 40;

        public static EvalResult Evaluate(
            IEnumerable<PedestrianAnnotation> groundTruth,
            IEnumerable<PedestrianAnnotation> detected)
        {
            var gtList  = new List<PedestrianAnnotation>(groundTruth);
            var detList = new List<PedestrianAnnotation>(detected);

            bool[] gtMatched  = new bool[gtList.Count];
            bool[] detMatched = new bool[detList.Count];

            // Match detections to GT (one-to-one)
            for (int di = 0; di < detList.Count; di++)
            {
                var det = detList[di];
                for (int gi = 0; gi < gtList.Count; gi++)
                {
                    if (gtList[gi].FileName != det.FileName) continue;
                    if (Math.Abs(det.X0 - gtList[gi].X0) <= OverlapThreshold)
                    {
                        detMatched[di] = true;
                        gtMatched[gi]  = true;
                        break; // one match per detection
                    }
                }
            }

            int tp = 0; foreach (var m in detMatched) if (m) tp++;
            int fp = detList.Count - tp;
            int trueGT = 0; foreach (var m in gtMatched) if (m) trueGT++;
            int fn = gtList.Count - trueGT;

            return new EvalResult(tp, fp, fn, gtList.Count, detList.Count)
            {
                TrueGT = trueGT
            };
        }

        // Convenience: evaluate over a whole folder

        public static EvalResult EvaluateFiles(string gtFile, string detFile)
        {
            var gt  = DataLoader.LoadAnnotations(gtFile);
            var det = DataLoader.LoadAnnotations(detFile);
            return Evaluate(gt, det);
        }
    }
}

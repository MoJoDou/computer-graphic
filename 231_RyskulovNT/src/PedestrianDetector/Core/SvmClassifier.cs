using System;
using System.Collections.Generic;
using Microsoft.ML;
using Microsoft.ML.Data;
using Microsoft.ML.Trainers;

namespace PedestrianDetector.Core
{
    // ML.Net schema types

    public class HogInput
    {
        // 7776 = 24*9 blocks * 4 cells * 9 bins  (patch 200x80, cell 8, block 2x2)
        [VectorType(7776)]
        [ColumnName("Features")]
        public float[] Features { get; set; } = Array.Empty<float>();

        [ColumnName("Label")]
        public bool Label { get; set; }
    }

    public class SvmPrediction
    {
        [ColumnName("PredictedLabel")]
        public bool PredictedLabel { get; set; }

        [ColumnName("Score")]
        public float Score { get; set; }
    }

    // Classifier

    public class SvmClassifier
    {
        private readonly MLContext _mlContext;
        private ITransformer? _model;
        private PredictionEngine<HogInput, SvmPrediction>? _predEngine;

        public SvmClassifier()
        {
            _mlContext = new MLContext(seed: 42);
        }

        // Training
        

        public void Train(
            List<float[]> positives,
            List<float[]> negatives,
            float svmC = 0.01f,
            int iterations = 100,
            Action<string>? log = null)
        {
            log?.Invoke($"Training SVM: {positives.Count} positives, {negatives.Count} negatives");

            var data     = BuildDataList(positives, negatives);
            var dataView = _mlContext.Data.LoadFromEnumerable(data);

            var pipeline = _mlContext.BinaryClassification.Trainers.LinearSvm(
                new LinearSvmTrainer.Options
                {
                    LabelColumnName    = "Label",
                    FeatureColumnName  = "Features",
                    NumberOfIterations = iterations,
                    Lambda             = svmC
                });

            log?.Invoke("Fitting model...");
            _model      = pipeline.Fit(dataView);
            _predEngine = _mlContext.Model.CreatePredictionEngine<HogInput, SvmPrediction>(_model);
            log?.Invoke("Training complete.");
        }

        // Prediction

        public bool Predict(float[] features)
        {
            EnsureLoaded();
            return _predEngine!.Predict(new HogInput { Features = features }).PredictedLabel;
        }

        public float PredictScore(float[] features)
        {
            EnsureLoaded();
            return _predEngine!.Predict(new HogInput { Features = features }).Score;
        }

        // Save / Load

        public void Save(string path)
        {
            EnsureLoaded();
            _mlContext.Model.Save(_model!, null, path);
        }

        public void Load(string path)
        {
            _model      = _mlContext.Model.Load(path, out _);
            _predEngine = _mlContext.Model.CreatePredictionEngine<HogInput, SvmPrediction>(_model);
        }

        // Cross-validation

        public Dictionary<float, double> CrossValidate(
            List<float[]> positives,
            List<float[]> negatives,
            float[] cValues,
            int kFolds = 5,
            Action<string>? log = null)
        {
            var results = new Dictionary<float, double>();

            var data = BuildDataList(positives, negatives);
            var rng  = new Random(42);
            for (int i = data.Count - 1; i > 0; i--)
            {
                int j = rng.Next(i + 1);
                (data[i], data[j]) = (data[j], data[i]);
            }

            foreach (float c in cValues)
            {
                log?.Invoke($"Cross-validating C={c}...");
                double totalAcc = 0;
                int foldSize    = data.Count / kFolds;

                for (int fold = 0; fold < kFolds; fold++)
                {
                    var train = new List<HogInput>();
                    var test  = new List<HogInput>();
                    for (int i = 0; i < data.Count; i++)
                    {
                        if (i >= fold * foldSize && i < (fold + 1) * foldSize)
                            test.Add(data[i]);
                        else
                            train.Add(data[i]);
                    }

                    var trainView = _mlContext.Data.LoadFromEnumerable(train);
                    var pipeline  = _mlContext.BinaryClassification.Trainers.LinearSvm(
                        new LinearSvmTrainer.Options
                        {
                            NumberOfIterations = 50,
                            Lambda             = c
                        });
                    var model = pipeline.Fit(trainView);
                    var pe    = _mlContext.Model.CreatePredictionEngine<HogInput, SvmPrediction>(model);

                    int correct = 0;
                    foreach (var sample in test)
                    {
                        var pred = pe.Predict(sample);
                        if (pred.PredictedLabel == sample.Label) correct++;
                    }
                    totalAcc += (double)correct / test.Count;
                }

                results[c] = totalAcc / kFolds;
                log?.Invoke($"  C={c} mean accuracy={results[c]:F4}");
            }

            return results;
        }

       
        // Helpers

        private static List<HogInput> BuildDataList(List<float[]> positives, List<float[]> negatives)
        {
            var list = new List<HogInput>(positives.Count + negatives.Count);
            foreach (var f in positives) list.Add(new HogInput { Features = f, Label = true  });
            foreach (var f in negatives) list.Add(new HogInput { Features = f, Label = false });
            return list;
        }

        private void EnsureLoaded()
        {
            if (_model == null || _predEngine == null)
                throw new InvalidOperationException("Model not trained or loaded.");
        }
    }
}

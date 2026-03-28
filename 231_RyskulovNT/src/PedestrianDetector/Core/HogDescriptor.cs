using System;
using System.Drawing;
using System.Drawing.Imaging;

namespace PedestrianDetector.Core
{

    public static class HogDescriptor
    {
        public const int PatchHeight = 200;
        public const int PatchWidth  = 80;
        public const int CellSize    = 8;   // pixels per cell
        public const int BlockSize   = 2;   // cells per block side
        public const int NumBins     = 9;   // orientation bins (0–180°)

        private static readonly int CellRows = PatchHeight / CellSize; // 25
        private static readonly int CellCols = PatchWidth  / CellSize; // 10
        public  static readonly int FeatureSize =
            (CellRows - BlockSize + 1) * (CellCols - BlockSize + 1)
            * BlockSize * BlockSize * NumBins; // 24*9*4*9 = 7776

        // Public API

        public static float[] Compute(Bitmap bmp)
        {
            var patch = EnsureSize(bmp);
            float[,] gray = ToGrayscale(patch);
            if (!ReferenceEquals(patch, bmp)) patch.Dispose();
            return ComputeFromGray(gray);
        }

        public static float[] ComputeFromGray(float[,] gray)
        {
            int H = gray.GetLength(0);
            int W = gray.GetLength(1);

            //  1. Gradients 
            var mag = new float[H, W];
            var ang = new float[H, W]; // 0–180 unsigned

            for (int y = 1; y < H - 1; y++)
            for (int x = 1; x < W - 1; x++)
            {
                float gx = gray[y, x + 1] - gray[y, x - 1];
                float gy = gray[y + 1, x] - gray[y - 1, x];
                mag[y, x] = MathF.Sqrt(gx * gx + gy * gy);
                float a = MathF.Atan2(gy, gx) * (180f / MathF.PI);
                if (a < 0) a += 180f;
                if (a >= 180f) a -= 180f;
                ang[y, x] = a;
            }

            // 2. Cell histograms
            int cellRows = H / CellSize;
            int cellCols = W / CellSize;
            var hist = new float[cellRows, cellCols, NumBins];

            float binWidth = 180f / NumBins;

            for (int cy = 0; cy < cellRows; cy++)
            for (int cx = 0; cx < cellCols; cx++)
            for (int py = 0; py < CellSize; py++)
            for (int px = 0; px < CellSize; px++)
            {
                int y = cy * CellSize + py;
                int x = cx * CellSize + px;
                float m = mag[y, x];
                float a = ang[y, x];

                float binF = a / binWidth;
                int b0 = (int)binF % NumBins;
                int b1 = (b0 + 1) % NumBins;
                float w1 = binF - MathF.Floor(binF);
                float w0 = 1f - w1;

                hist[cy, cx, b0] += w0 * m;
                hist[cy, cx, b1] += w1 * m;
            }

            //  3. Block normalization (L2-Hys) 
            int blockRowCount = cellRows - BlockSize + 1;
            int blockColCount = cellCols - BlockSize + 1;
            var features = new float[blockRowCount * blockColCount * BlockSize * BlockSize * NumBins];
            int idx = 0;

            for (int by = 0; by <= cellRows - BlockSize; by++)
            for (int bx = 0; bx <= cellCols - BlockSize; bx++)
            {
                int blockLen = BlockSize * BlockSize * NumBins;
                var block = new float[blockLen];
                int bi = 0;
                for (int cy = by; cy < by + BlockSize; cy++)
                for (int cx = bx; cx < bx + BlockSize; cx++)
                for (int b  = 0;  b  < NumBins;       b++)
                    block[bi++] = hist[cy, cx, b];

                // L2 norm
                float norm2 = 1e-6f;
                foreach (var v in block) norm2 += v * v;
                float invNorm = 1f / MathF.Sqrt(norm2);
                for (int i = 0; i < blockLen; i++) block[i] *= invNorm;

                // Clamp (Hys)
                for (int i = 0; i < blockLen; i++) block[i] = MathF.Min(block[i], 0.2f);

                // L2 again
                norm2 = 1e-6f;
                foreach (var v in block) norm2 += v * v;
                invNorm = 1f / MathF.Sqrt(norm2);
                for (int i = 0; i < blockLen; i++)
                    features[idx++] = block[i] * invNorm;
            }

            return features;
        }

        // Helpers

        public static float[,] ToGrayscale(Bitmap bmp)
        {
            int H = bmp.Height, W = bmp.Width;
            var gray = new float[H, W];

            var rect = new Rectangle(0, 0, W, H);
            var data = bmp.LockBits(rect, ImageLockMode.ReadOnly, PixelFormat.Format32bppArgb);
            int stride = data.Stride;
            unsafe
            {
                byte* ptr = (byte*)data.Scan0.ToPointer();
                for (int y = 0; y < H; y++)
                {
                    byte* row = ptr + y * stride;
                    for (int x = 0; x < W; x++)
                    {
                        // BGRA layout
                        float b = row[x * 4 + 0];
                        float g = row[x * 4 + 1];
                        float r = row[x * 4 + 2];
                        gray[y, x] = 0.299f * r + 0.587f * g + 0.114f * b;
                    }
                }
            }
            bmp.UnlockBits(data);
            return gray;
        }

        public static float[,] CropPatch(float[,] gray, int x0)
        {
            var patch = new float[PatchHeight, PatchWidth];
            int H = gray.GetLength(0);
            int W = gray.GetLength(1);
            for (int y = 0; y < PatchHeight && y < H; y++)
            for (int x = 0; x < PatchWidth; x++)
            {
                int sx = x0 + x;
                patch[y, x] = (sx >= 0 && sx < W) ? gray[y, sx] : 0f;
            }
            return patch;
        }

        private static Bitmap EnsureSize(Bitmap bmp)
        {
            if (bmp.Width == PatchWidth && bmp.Height == PatchHeight) return bmp;
            var resized = new Bitmap(PatchWidth, PatchHeight);
            using var g = Graphics.FromImage(resized);
            g.DrawImage(bmp, 0, 0, PatchWidth, PatchHeight);
            return resized;
        }
    }
}

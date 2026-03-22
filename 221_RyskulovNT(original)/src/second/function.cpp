/* NAME: Ryskulov Niyaz epi-2-23
 * ASGN: N2
 */

#include "function.h"
#include <iostream>
#include <fstream>
#include <algorithm>
#include <cstring>
#include <unordered_map>
#include <functional>
#include <climits>

// ===========================================================================
// Construction / destruction
// ===========================================================================

CImage::CImage()
    : m_width(0), m_height(0), m_data(nullptr)
{}

CImage::~CImage()
{
    delete[] m_data;
}

// ===========================================================================
// I/O
// ===========================================================================

bool CImage::LoadBmp(const char* filename)
{
    std::ifstream file(filename, std::ios::binary);
    if (!file)
    {
        std::cout << "Error: cannot open '" << filename << "'" << std::endl;
        return false;
    }

    // Read full 54-byte BMP/DIB header
    unsigned char hdr[54];
    file.read(reinterpret_cast<char*>(hdr), 54);

    if (hdr[0] != 'B' || hdr[1] != 'M')
    {
        std::cout << "Error: not a BMP file." << std::endl;
        return false;
    }

    int data_offset = *reinterpret_cast<int*>(&hdr[10]);
    m_width         = *reinterpret_cast<int*>(&hdr[18]);
    m_height        = *reinterpret_cast<int*>(&hdr[22]);
    int bit_depth   = *reinterpret_cast<short*>(&hdr[28]);

    if (bit_depth != 24)
    {
        std::cout << "Error: only 24-bit BMP supported." << std::endl;
        return false;
    }

    // Negative height means top-down storage; normalise to positive
    bool top_down = (m_height < 0);
    if (top_down) m_height = -m_height;

    file.seekg(data_offset, std::ios::beg);

    int row_padded = (m_width * 3 + 3) & (~3);
    delete[] m_data;
    m_data = new unsigned char[row_padded * m_height];
    memset(m_data, 0, row_padded * m_height);

    file.read(reinterpret_cast<char*>(m_data),
              static_cast<std::streamsize>(row_padded) * m_height);
    file.close();

    // If the file was top-down, flip rows so SetPixel/GetPixel work uniformly
    if (top_down)
    {
        std::vector<unsigned char> tmp(row_padded);
        for (int y = 0; y < m_height / 2; y++)
        {
            unsigned char* rowA = m_data + y * row_padded;
            unsigned char* rowB = m_data + (m_height - 1 - y) * row_padded;
            memcpy(tmp.data(), rowA, row_padded);
            memcpy(rowA, rowB, row_padded);
            memcpy(rowB, tmp.data(), row_padded);
        }
    }

    std::cout << "Loaded: " << filename
              << " (" << m_width << "x" << m_height << ")" << std::endl;
    return true;
}

bool CImage::SaveBmp(const char* filename) const
{
    if (!m_data) return false;

    std::ofstream file(filename, std::ios::binary);
    if (!file)
    {
        std::cout << "Error: cannot create '" << filename << "'" << std::endl;
        return false;
    }

    int row_padded = (m_width * 3 + 3) & (~3);
    int image_size = row_padded * m_height;
    int file_size  = 54 + image_size;

    unsigned char hdr[54] = {};
    hdr[0] = 'B'; hdr[1] = 'M';
    *reinterpret_cast<int*>(&hdr[2])   = file_size;
    *reinterpret_cast<int*>(&hdr[10])  = 54;
    *reinterpret_cast<int*>(&hdr[14])  = 40;
    *reinterpret_cast<int*>(&hdr[18])  = m_width;
    *reinterpret_cast<int*>(&hdr[22])  = m_height;
    *reinterpret_cast<short*>(&hdr[26])= 1;
    *reinterpret_cast<short*>(&hdr[28])= 24;
    *reinterpret_cast<int*>(&hdr[34])  = image_size;

    file.write(reinterpret_cast<const char*>(hdr), 54);
    file.write(reinterpret_cast<const char*>(m_data), image_size);
    file.close();

    std::cout << "Saved: " << filename << std::endl;
    return true;
}

// ===========================================================================
// Pixel access
// BMP stores rows bottom-up; row y=0 in logical coords is the last raw row.
// ===========================================================================

void CImage::SetPixel(int x, int y,
                      unsigned char r, unsigned char g, unsigned char b)
{
    if (x < 0 || x >= m_width || y < 0 || y >= m_height) return;
    int row_padded = (m_width * 3 + 3) & (~3);
    int idx = (m_height - 1 - y) * row_padded + x * 3;
    m_data[idx]     = b;   // BMP is BGR
    m_data[idx + 1] = g;
    m_data[idx + 2] = r;
}

void CImage::GetPixel(int x, int y,
                      unsigned char& r, unsigned char& g, unsigned char& b) const
{
    if (x < 0 || x >= m_width || y < 0 || y >= m_height)
    {
        r = g = b = 0;
        return;
    }
    int row_padded = (m_width * 3 + 3) & (~3);
    int idx = (m_height - 1 - y) * row_padded + x * 3;
    b = m_data[idx];
    g = m_data[idx + 1];
    r = m_data[idx + 2];
}

// 
// Contrast correction – Grey World
// Scales each channel independently so its average equals 128.
// 
void CImage::GrayWorldCorrection()
{
    if (!m_data) return;

    long long sumR = 0, sumG = 0, sumB = 0;
    long long n    = static_cast<long long>(m_width) * m_height;

    for (int y = 0; y < m_height; y++)
        for (int x = 0; x < m_width; x++)
        {
            unsigned char r, g, b;
            GetPixel(x, y, r, g, b);
            sumR += r; sumG += g; sumB += b;
        }

    double meanR = sumR / static_cast<double>(n);
    double meanG = sumG / static_cast<double>(n);
    double meanB = sumB / static_cast<double>(n);

    // Avoid division by zero
    double scaleR = (meanR > 0) ? 128.0 / meanR : 1.0;
    double scaleG = (meanG > 0) ? 128.0 / meanG : 1.0;
    double scaleB = (meanB > 0) ? 128.0 / meanB : 1.0;

    for (int y = 0; y < m_height; y++)
        for (int x = 0; x < m_width; x++)
        {
            unsigned char r, g, b;
            GetPixel(x, y, r, g, b);
            int nr = std::min(255, static_cast<int>(r * scaleR));
            int ng = std::min(255, static_cast<int>(g * scaleG));
            int nb = std::min(255, static_cast<int>(b * scaleB));
            SetPixel(x, y,
                     static_cast<unsigned char>(nr),
                     static_cast<unsigned char>(ng),
                     static_cast<unsigned char>(nb));
        }

    std::cout << "Grey-world correction applied"
              << " (scale R=" << scaleR
              << " G=" << scaleG
              << " B=" << scaleB << ")" << std::endl;
}

// Contrast correction – Range Stretch
// Linearly maps [min, max] per channel to [0, 255].
void CImage::StretchContrast()
{
    if (!m_data) return;

    unsigned char minR = 255, maxR = 0;
    unsigned char minG = 255, maxG = 0;
    unsigned char minB = 255, maxB = 0;

    for (int y = 0; y < m_height; y++)
        for (int x = 0; x < m_width; x++)
        {
            unsigned char r, g, b;
            GetPixel(x, y, r, g, b);
            minR = std::min(minR, r); maxR = std::max(maxR, r);
            minG = std::min(minG, g); maxG = std::max(maxG, g);
            minB = std::min(minB, b); maxB = std::max(maxB, b);
        }

    auto stretch = [](unsigned char v, unsigned char lo, unsigned char hi) -> unsigned char
    {
        if (hi == lo) return v;
        return static_cast<unsigned char>((v - lo) * 255 / (hi - lo));
    };

    for (int y = 0; y < m_height; y++)
        for (int x = 0; x < m_width; x++)
        {
            unsigned char r, g, b;
            GetPixel(x, y, r, g, b);
            SetPixel(x, y,
                     stretch(r, minR, maxR),
                     stretch(g, minG, maxG),
                     stretch(b, minB, maxB));
        }

    std::cout << "Range-stretch correction applied" << std::endl;
}


// Noise reduction – Median filter
// Applies per-channel median over a square (2*radius+1) x (2*radius+1) window.
// Border pixels are handled by clamping coordinates.

void CImage::MedianFilter(int radius)
{
    if (!m_data || radius <= 0) return;

    int row_padded = (m_width * 3 + 3) & (~3);
    unsigned char* out = new unsigned char[row_padded * m_height];
    memset(out, 0, row_padded * m_height);

    int kernel = (2 * radius + 1) * (2 * radius + 1);
    std::vector<unsigned char> rv(kernel), gv(kernel), bv(kernel);

    for (int y = 0; y < m_height; y++)
    {
        for (int x = 0; x < m_width; x++)
        {
            int k = 0;
            for (int dy = -radius; dy <= radius; dy++)
            {
                for (int dx = -radius; dx <= radius; dx++)
                {
                    int nx = std::max(0, std::min(m_width  - 1, x + dx));
                    int ny = std::max(0, std::min(m_height - 1, y + dy));
                    // Read directly from raw buffer (BGR, bottom-up)
                    int idx = (m_height - 1 - ny) * row_padded + nx * 3;
                    bv[k] = m_data[idx];
                    gv[k] = m_data[idx + 1];
                    rv[k] = m_data[idx + 2];
                    k++;
                }
            }
            std::sort(rv.begin(), rv.end());
            std::sort(gv.begin(), gv.end());
            std::sort(bv.begin(), bv.end());

            int mid    = kernel / 2;
            int out_idx = (m_height - 1 - y) * row_padded + x * 3;
            out[out_idx]     = bv[mid];
            out[out_idx + 1] = gv[mid];
            out[out_idx + 2] = rv[mid];
        }
    }

    delete[] m_data;
    m_data = out;
    std::cout << "Median filter applied (radius=" << radius << ")" << std::endl;
}

// ===========================================================================
// Binarization
// ===========================================================================

void CImage::Binarize(int threshold)
{
    if (!m_data) return;
    for (int y = 0; y < m_height; y++)
        for (int x = 0; x < m_width; x++)
        {
            unsigned char r, g, b;
            GetPixel(x, y, r, g, b);
            // max channel: correctly detects red/green/blue/white objects
            // against black. Average fails for saturated colors:
            // pure red RGB(236,23,8) → avg=89 but max=236.
            int intensity = std::max({(int)r, (int)g, (int)b});
            unsigned char val = (intensity > threshold) ? 255 : 0;
            SetPixel(x, y, val, val, val);
        }
}

// ---------------------------------------------------------------------------
// Otsu's threshold
// Maximises between-class variance: σ²_B = wB * wF * (μB - μF)²
// ---------------------------------------------------------------------------
int CImage::OtsuThreshold() const
{
    long long hist[256] = {};
    for (int y = 0; y < m_height; y++)
        for (int x = 0; x < m_width; x++)
        {
            unsigned char r, g, b;
            GetPixel(x, y, r, g, b);
            hist[std::max({(int)r, (int)g, (int)b})]++;
        }

    long long total = static_cast<long long>(m_width) * m_height;
    double sum_all  = 0;
    for (int i = 0; i < 256; i++) sum_all += i * hist[i];

    double sumB = 0, wB = 0;
    double best_var = 0;
    int threshold = 0;

    for (int t = 0; t < 256; t++)
    {
        wB += hist[t];
        if (wB == 0) continue;
        double wF = total - wB;
        if (wF == 0) break;

        sumB += t * hist[t];
        double mB = sumB / wB;
        double mF = (sum_all - sumB) / wF;
        double var = wB * wF * (mB - mF) * (mB - mF);
        if (var > best_var) { best_var = var; threshold = t; }
    }

    std::cout << "Otsu threshold: " << threshold << std::endl;
    return threshold;
}

// ---------------------------------------------------------------------------
// Symmetric-Peak threshold
// Finds the valley between the two dominant histogram peaks.
// Reliable for dark-background images where Gaussian peaks are well separated.
// ---------------------------------------------------------------------------
int CImage::SymmetricPeakThreshold() const
{
    long long hist[256] = {};
    for (int y = 0; y < m_height; y++)
        for (int x = 0; x < m_width; x++)
        {
            unsigned char r, g, b;
            GetPixel(x, y, r, g, b);
            hist[std::max({(int)r, (int)g, (int)b})]++;
        }

    // Smooth histogram slightly (3-tap box) to suppress noise spikes
    long long smoothed[256];
    smoothed[0]   = (hist[0] + hist[1]) / 2;
    smoothed[255] = (hist[254] + hist[255]) / 2;
    for (int i = 1; i < 255; i++)
        smoothed[i] = (hist[i - 1] + hist[i] + hist[i + 1]) / 3;

    // First peak (background – typically the tallest)
    int peak1 = 0;
    for (int i = 1; i < 256; i++)
        if (smoothed[i] > smoothed[peak1]) peak1 = i;

    // Second peak: largest bin outside a ±30 neighbourhood of peak1
    int guard = 30;
    int peak2  = (peak1 < 128) ? 255 : 0;
    for (int i = 0; i < 256; i++)
    {
        if (std::abs(i - peak1) <= guard) continue;
        if (smoothed[i] > smoothed[peak2]) peak2 = i;
    }

    // Valley: minimum between the two peaks
    int lo = std::min(peak1, peak2);
    int hi = std::max(peak1, peak2);
    int valley = lo;
    for (int i = lo; i <= hi; i++)
        if (smoothed[i] < smoothed[valley]) valley = i;

    std::cout << "Symmetric-peak threshold: " << valley
              << " (peaks at " << peak1 << " and " << peak2 << ")" << std::endl;
    return valley;
}

// ===========================================================================
// Mathematical morphology  (operate on binary images: 0 = bg, 255 = fg)
// Square structuring element of half-size radius.
// ===========================================================================

void CImage::Erode(int radius)
{
    if (!m_data || radius <= 0) return;

    int row_padded = (m_width * 3 + 3) & (~3);
    unsigned char* out = new unsigned char[row_padded * m_height];
    memset(out, 0, row_padded * m_height);   // default: black

    for (int y = 0; y < m_height; y++)
        for (int x = 0; x < m_width; x++)
        {
            bool all_fg = true;
            for (int dy = -radius; dy <= radius && all_fg; dy++)
                for (int dx = -radius; dx <= radius && all_fg; dx++)
                {
                    int nx = x + dx, ny = y + dy;
                    if (nx < 0 || nx >= m_width || ny < 0 || ny >= m_height)
                    { all_fg = false; break; }
                    int idx = (m_height - 1 - ny) * row_padded + nx * 3;
                    if (m_data[idx + 2] <= 128) all_fg = false;  // R channel
                }
            if (all_fg)
            {
                int oi = (m_height - 1 - y) * row_padded + x * 3;
                out[oi] = out[oi+1] = out[oi+2] = 255;
            }
        }

    delete[] m_data;
    m_data = out;
}

void CImage::Dilate(int radius)
{
    if (!m_data || radius <= 0) return;

    int row_padded = (m_width * 3 + 3) & (~3);
    unsigned char* out = new unsigned char[row_padded * m_height];
    memset(out, 0, row_padded * m_height);

    for (int y = 0; y < m_height; y++)
        for (int x = 0; x < m_width; x++)
        {
            bool any_fg = false;
            for (int dy = -radius; dy <= radius && !any_fg; dy++)
                for (int dx = -radius; dx <= radius && !any_fg; dx++)
                {
                    int nx = x + dx, ny = y + dy;
                    if (nx < 0 || nx >= m_width || ny < 0 || ny >= m_height) continue;
                    int idx = (m_height - 1 - ny) * row_padded + nx * 3;
                    if (m_data[idx + 2] > 128) any_fg = true;
                }
            if (any_fg)
            {
                int oi = (m_height - 1 - y) * row_padded + x * 3;
                out[oi] = out[oi+1] = out[oi+2] = 255;
            }
        }

    delete[] m_data;
    m_data = out;
}

void CImage::Open(int radius)
{
    Erode(radius);
    Dilate(radius);
}

void CImage::Close(int radius)
{
    Dilate(radius);
    Erode(radius);
}

// ===========================================================================
// Connected-component labeling – Flood Fill (4-connectivity)
// ===========================================================================
void CImage::FindObjects(CImage& orig_img, std::vector<SObject>& objects)
{
    if (!m_data) return;

    std::vector<bool> visited(m_width * m_height, false);

    for (int y = 0; y < m_height; y++)
        for (int x = 0; x < m_width; x++)
        {
            unsigned char r, g, b;
            GetPixel(x, y, r, g, b);
            if (r <= 128 || visited[y * m_width + x]) continue;

            SObject obj;
            obj.min_x = x; obj.max_x = x;
            obj.min_y = y; obj.max_y = y;
            obj.area  = 0;
            long long sx = 0, sy = 0, sr = 0, sg = 0, sb = 0;

            std::vector<std::pair<int,int>> stk;
            stk.push_back({x, y});
            visited[y * m_width + x] = true;

            while (!stk.empty())
            {
                std::pair<int,int> top = stk.back(); stk.pop_back();
                int px = top.first, py = top.second;
                obj.area++;
                sx += px; sy += py;
                obj.pixels.push_back({px, py});
                if (px < obj.min_x) obj.min_x = px;
                if (px > obj.max_x) obj.max_x = px;
                if (py < obj.min_y) obj.min_y = py;
                if (py > obj.max_y) obj.max_y = py;

                unsigned char or_, og, ob;
                orig_img.GetPixel(px, py, or_, og, ob);
                sr += or_; sg += og; sb += ob;

                static const int DX[] = {-1,1,0,0};
                static const int DY[] = {0,0,-1,1};
                for (int i = 0; i < 4; i++)
                {
                    int nx = px + DX[i], ny = py + DY[i];
                    if (nx < 0 || nx >= m_width || ny < 0 || ny >= m_height) continue;
                    if (visited[ny * m_width + nx]) continue;
                    unsigned char nr, ng, nb;
                    GetPixel(nx, ny, nr, ng, nb);
                    if (nr > 128)
                    {
                        visited[ny * m_width + nx] = true;
                        stk.push_back({nx, ny});
                    }
                }
            }

            if (obj.area > 50)
            {
                obj.center_x = static_cast<int>(sx / obj.area);
                obj.center_y = static_cast<int>(sy / obj.area);
                obj.r = static_cast<unsigned char>(sr / obj.area);
                obj.g = static_cast<unsigned char>(sg / obj.area);
                obj.b = static_cast<unsigned char>(sb / obj.area);
                obj.angle = 0.0;
                objects.push_back(std::move(obj));
            }
        }
}

// ===========================================================================
// Connected-component labeling – Sequential Scan (two-pass with union-find)
// ===========================================================================
void CImage::FindObjectsSeqScan(CImage& orig_img, std::vector<SObject>& objects)
{
    if (!m_data) return;

    // ---- Union-Find with path compression ----
    std::vector<int> uf;
    uf.reserve(512);
    uf.push_back(0); // index 0 = background label

    auto uf_make = [&]() -> int {
        int id = static_cast<int>(uf.size());
        uf.push_back(id);
        return id;
    };

    std::function<int(int)> uf_find = [&](int x) -> int {
        if (uf[x] != x) uf[x] = uf_find(uf[x]);
        return uf[x];
    };

    auto uf_union = [&](int a, int b) {
        int ra = uf_find(a), rb = uf_find(b);
        if (ra != rb) uf[ra] = rb;
    };

    // ---- Pass 1: assign provisional labels ----
    std::vector<int> lbl(m_width * m_height, 0);

    for (int y = 0; y < m_height; y++)
        for (int x = 0; x < m_width; x++)
        {
            unsigned char r, g, b;
            GetPixel(x, y, r, g, b);
            if (r <= 128) continue; // background

            int L = (x > 0) ? lbl[y * m_width + (x - 1)] : 0;
            int T = (y > 0) ? lbl[(y - 1) * m_width + x] : 0;

            if (L == 0 && T == 0)
                lbl[y * m_width + x] = uf_make();
            else if (L != 0 && T == 0)
                lbl[y * m_width + x] = L;
            else if (L == 0 && T != 0)
                lbl[y * m_width + x] = T;
            else
            {
                uf_union(L, T);
                lbl[y * m_width + x] = L;
            }
        }

    // ---- Pass 2: resolve labels to root ----
    for (int i = 0; i < m_width * m_height; i++)
        if (lbl[i] > 0) lbl[i] = uf_find(lbl[i]);

    // ---- Build SObject list ----
    // Use a flat accumulator indexed by root label
    // We need long-long sums; store them in parallel unordered_map
    struct Accum { long long sx, sy, sr, sg, sb; };
    std::unordered_map<int, Accum> acc;
    std::unordered_map<int, SObject> obj_map;

    for (int y = 0; y < m_height; y++)
        for (int x = 0; x < m_width; x++)
        {
            int id = lbl[y * m_width + x];
            if (id == 0) continue;

            auto& obj = obj_map[id];
            auto& a   = acc[id];

            if (obj.area == 0)
            {
                obj.min_x = obj.max_x = x;
                obj.min_y = obj.max_y = y;
                a = {0, 0, 0, 0, 0};
            }
            obj.area++;
            obj.pixels.push_back({x, y});
            if (x < obj.min_x) obj.min_x = x;
            if (x > obj.max_x) obj.max_x = x;
            if (y < obj.min_y) obj.min_y = y;
            if (y > obj.max_y) obj.max_y = y;

            a.sx += x; a.sy += y;

            unsigned char or_, og, ob;
            orig_img.GetPixel(x, y, or_, og, ob);
            a.sr += or_; a.sg += og; a.sb += ob;
        }

    for (std::unordered_map<int,SObject>::iterator it = obj_map.begin(); it != obj_map.end(); ++it)
    {
        SObject& obj = it->second;
        if (obj.area <= 50) continue;
        Accum& a = acc[it->first];
        obj.center_x = static_cast<int>(a.sx / obj.area);
        obj.center_y = static_cast<int>(a.sy / obj.area);
        obj.r = static_cast<unsigned char>(a.sr / obj.area);
        obj.g = static_cast<unsigned char>(a.sg / obj.area);
        obj.b = static_cast<unsigned char>(a.sb / obj.area);
        obj.angle = 0.0;
        objects.push_back(obj);
    }
}

// ===========================================================================
// Drawing helpers
// ===========================================================================

void CImage::DrawRect(int x1, int y1, int x2, int y2,
                      unsigned char r, unsigned char g, unsigned char b)
{
    for (int x = x1; x <= x2; x++) { SetPixel(x, y1, r, g, b); SetPixel(x, y2, r, g, b); }
    for (int y = y1; y <= y2; y++) { SetPixel(x1, y, r, g, b); SetPixel(x2, y, r, g, b); }
}

void CImage::DrawLine(int x1, int y1, int x2, int y2,
                      unsigned char r, unsigned char g, unsigned char b)
{
    int dx =  std::abs(x2 - x1), sx = (x1 < x2) ? 1 : -1;
    int dy = -std::abs(y2 - y1), sy = (y1 < y2) ? 1 : -1;
    int err = dx + dy;
    while (true)
    {
        SetPixel(x1, y1, r, g, b);
        if (x1 == x2 && y1 == y2) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x1 += sx; }
        if (e2 <= dx) { err += dx; y1 += sy; }
    }
}

// ---------------------------------------------------------------------------
// DrawContour
// Draws the morphological (inner) boundary of mask_obj onto *this* image.
// A pixel is a boundary pixel if it belongs to the object AND at least one
// 8-connected neighbour is NOT in the object (i.e. is background in the
// binary image).  This is equivalent to: boundary = object − erosion(object).
// ---------------------------------------------------------------------------
void CImage::DrawContour(const SObject& mask_obj,
                         unsigned char r, unsigned char g, unsigned char b)
{
    // Build a fast lookup set for the object's pixel coordinates
    // (y*width + x) stored as integers for O(1) membership test
    std::vector<bool> in_obj(m_width * m_height, false);
    for (size_t i = 0; i < mask_obj.pixels.size(); i++)
    {
        int px = mask_obj.pixels[i].first;
        int py = mask_obj.pixels[i].second;
        if (px >= 0 && px < m_width && py >= 0 && py < m_height)
            in_obj[py * m_width + px] = true;
    }

    static const int DX8[] = {-1,0,1,-1,1,-1,0,1};
    static const int DY8[] = {-1,-1,-1,0,0,1,1,1};

    for (size_t i = 0; i < mask_obj.pixels.size(); i++)
    {
        int px = mask_obj.pixels[i].first;
        int py = mask_obj.pixels[i].second;
        bool boundary = false;
        for (int d = 0; d < 8 && !boundary; d++)
        {
            int nx = px + DX8[d], ny = py + DY8[d];
            if (nx < 0 || nx >= m_width || ny < 0 || ny >= m_height)
            { boundary = true; break; }
            if (!in_obj[ny * m_width + nx]) boundary = true;
        }
        if (boundary)
            SetPixel(px, py, r, g, b);
    }
}

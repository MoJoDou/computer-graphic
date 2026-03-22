/* NAME: Ryskulov Niyaz epi-2-23
 * ASGN: N2
 */

#include <iostream>
#include <vector>
#include <cmath>
#include <string>
#include <limits>
#include <climits>
#include <algorithm>
#include "function.h"

#define PI 3.14159265358979323846

// ===========================================================================
// Object classification
// ===========================================================================
enum ObjType { OBJ_ARROW, OBJ_START, OBJ_TREASURE, OBJ_TRAP };

// Returns the type of an object based on its average color.
// Uses relative channel dominance so noisy/dim images still work.
static ObjType ClassifyObject(const SObject& obj)
{
    int r = obj.r, g = obj.g, b = obj.b;

    // Colour saturation: how far the dominant channel is from the others
    int max_ch = std::max(r, std::max(g, b));
    int min_ch = std::min(r, std::min(g, b));
    int sat    = max_ch - min_ch;

    // Unsaturated (grey/white) → regular path arrow, no further tests needed
    if (sat < 25)
        return OBJ_ARROW;

    // Red dominant → start arrow
    if (r == max_ch && r > g + 20 && r > b + 20)
        return OBJ_START;

    // Blue dominant → treasure
    if (b == max_ch && b > r + 20 && b > g - 10)
        return OBJ_TREASURE;

    // Green dominant → trap (clover, leaf, etc.) — must NOT be a path waypoint
    if (g == max_ch && g > r + 20 && g > b + 20)
        return OBJ_TRAP;

    // Any other saturated colour (orange sun, gold, etc.) → also a trap/decoration
    return OBJ_TRAP;
}

// ===========================================================================
// Arrow orientation
// ===========================================================================

// Compute principal axis angle via second-order central moments → [-PI/2, PI/2]
static double ComputeAngle(const SObject& obj)
{
    if (obj.pixels.empty()) return 0.0;
    double M20 = 0, M02 = 0, M11 = 0;
    for (size_t i = 0; i < obj.pixels.size(); i++)
    {
        double dx = obj.pixels[i].first  - obj.center_x;
        double dy = obj.pixels[i].second - obj.center_y;
        M20 += dx * dx;
        M02 += dy * dy;
        M11 += dx * dy;
    }
    return 0.5 * atan2(2.0 * M11, M20 - M02);
}

// Resolve 180-degree ambiguity: the arrowhead tip extends further from
// the centroid than the tail → choose the direction with the larger
// maximum projection.
static double DisambiguateAngle(const SObject& obj, double raw_angle)
{
    double ax = cos(raw_angle), ay = sin(raw_angle);
    double max_p = -1e18, min_p = 1e18;
    for (size_t i = 0; i < obj.pixels.size(); i++)
    {
        double proj = (obj.pixels[i].first  - obj.center_x) * ax
                    + (obj.pixels[i].second - obj.center_y) * ay;
        if (proj > max_p) max_p = proj;
        if (proj < min_p) min_p = proj;
    }
    return (fabs(max_p) >= fabs(min_p)) ? raw_angle : raw_angle + PI;
}

// ===========================================================================
// Processing settings (configurable via menu)
// ===========================================================================
struct Settings
{
    // Threshold method: 0 = Otsu (default), 1 = SymmetricPeak, 2 = Manual
    int  threshold_method  = 0;
    int  manual_threshold  = 50;

    // Contrast: 0 = none (default), 1 = Grey World, 2 = Range Stretch
    int  contrast_method   = 0;

    // Noise filter: 0 = none (default), > 0 = median radius
    int  median_radius     = 0;

    // Morphology on binary image: 0 = none, 1 = open, 2 = close, 3 = open+close
    int  morph_mode        = 1;   // default: opening to remove noise
    int  morph_radius      = 1;

    // Component algorithm: 0 = Flood Fill (default), 1 = Sequential Scan
    int  component_alg     = 0;

    // Treasure outline: 0 = bounding box (default), 1 = morphological contour
    int  contour_mode      = 1;

    // Minimum object area to keep (filters noise fragments on noisy images)
    // simple=50, noise images=400-800
    int  min_area          = 50;
};

// ===========================================================================
// Single-image processing pipeline
// ===========================================================================
static void ProcessImage(const char* in_file, const char* out_file,
                         const Settings& s)
{
    std::cout << "\n=== Processing: " << in_file << " ===" << std::endl;

    CImage img_orig, img_proc;

    if (!img_orig.LoadBmp(in_file) || !img_proc.LoadBmp(in_file))
    {
        std::cout << "  Skipped (could not load)." << std::endl;
        return;
    }

    // ---- Step 1: Contrast correction (on processing copy only) ----
    if (s.contrast_method == 1)
        img_proc.GrayWorldCorrection();
    else if (s.contrast_method == 2)
        img_proc.StretchContrast();

    // ---- Step 2: Noise reduction ----
    if (s.median_radius > 0)
        img_proc.MedianFilter(s.median_radius);

    // ---- Step 3: Auto or manual threshold ----
    int threshold = s.manual_threshold;
    if      (s.threshold_method == 0) threshold = img_proc.OtsuThreshold();
    else if (s.threshold_method == 1) threshold = img_proc.SymmetricPeakThreshold();

    img_proc.Binarize(threshold);

    // ---- Step 4: Morphological cleanup of binary image ----
    if (s.morph_mode == 1 || s.morph_mode == 3) img_proc.Open(s.morph_radius);
    if (s.morph_mode == 2 || s.morph_mode == 3) img_proc.Close(s.morph_radius);

    // ---- Step 5: Find connected objects ----
    std::vector<SObject> objects;
    if (s.component_alg == 1)
        img_proc.FindObjectsSeqScan(img_orig, objects);
    else
        img_proc.FindObjects(img_orig, objects);

    // Remove fragments smaller than min_area threshold
    objects.erase(
        std::remove_if(objects.begin(), objects.end(),
                       [&](const SObject& o){ return o.area < s.min_area; }),
        objects.end());

    std::cout << "  Objects found: " << objects.size() << std::endl;

    // ---- Step 6: Compute arrow angles ----
    for (auto& obj : objects)
        obj.angle = DisambiguateAngle(obj, ComputeAngle(obj));

    // ---- Step 7: Locate start and treasure ----
    int start_idx    = -1;
    int treasure_idx = -1;

    for (int i = 0; i < static_cast<int>(objects.size()); i++)
    {
        ObjType t = ClassifyObject(objects[i]);
        if (t == OBJ_START    && start_idx    == -1) start_idx    = i;
        if (t == OBJ_TREASURE && treasure_idx == -1) treasure_idx = i;
    }

    if (start_idx == -1)
    {
        std::cout << "  ERROR: no start (red) arrow found!" << std::endl;
        return;
    }
    if (treasure_idx == -1)
    {
        std::cout << "  WARNING: no treasure found. Attempting path anyway." << std::endl;
    }

    std::cout << "  Start obj=" << start_idx
              << " (" << objects[start_idx].center_x
              << "," << objects[start_idx].center_y << ")" << std::endl;

    // ---- Step 8: Greedy path following ----
    std::vector<bool> visited(objects.size(), false);
    int curr = start_idx;
    visited[curr] = true;

    while (true)
    {
        // Mark treasure when we arrive
        if (curr == treasure_idx)
        {
            if (s.contour_mode == 1)
            {
                // Morphological contour (inner boundary)
                img_orig.DrawContour(objects[curr], 0, 255, 0);
                // Second contour pass at +1px offset for visibility
                for (size_t pi = 0; pi < objects[curr].pixels.size(); pi++)
                    img_proc.SetPixel(objects[curr].pixels[pi].first,
                                      objects[curr].pixels[pi].second,
                                      255, 255, 255);
                img_orig.DrawContour(objects[curr], 0, 255, 0);
            }
            else
            {
                // Bounding box (fallback)
                img_orig.DrawRect(objects[curr].min_x - 2, objects[curr].min_y - 2,
                                  objects[curr].max_x + 2, objects[curr].max_y + 2,
                                  0, 255, 0);
                img_orig.DrawRect(objects[curr].min_x - 3, objects[curr].min_y - 3,
                                  objects[curr].max_x + 3, objects[curr].max_y + 3,
                                  0, 255, 0);
            }
            std::cout << "  Treasure found at obj=" << curr
                      << " (" << objects[curr].center_x
                      << "," << objects[curr].center_y << ")" << std::endl;
            break;
        }

        double dir_x = cos(objects[curr].angle);
        double dir_y = sin(objects[curr].angle);

        // Find best next candidate in the arrow's direction.
        // OBJ_TRAP objects (clover, sun, decorations) are never waypoints.
        int    best_next  = -1;
        double best_score = 1e18;

        for (int i = 0; i < static_cast<int>(objects.size()); i++)
        {
            if (visited[i]) continue;
            if (ClassifyObject(objects[i]) == OBJ_TRAP) continue; // skip traps

            double dx = objects[i].center_x - objects[curr].center_x;
            double dy = objects[i].center_y - objects[curr].center_y;
            double along = dx * dir_x + dy * dir_y;
            double perp  = fabs(dx * dir_y - dy * dir_x);

            if (along < 5.0)   continue;  // must be in front
            if (perp > along * 1.5) continue; // must be aligned

            double score = along + perp * 3.0;
            if (score < best_score) { best_score = score; best_next = i; }
        }

        if (best_next == -1)
        {
            std::cout << "  Path dead end at obj=" << curr << std::endl;
            break;
        }

        // Draw 3-pixel-wide cyan path line
        int cx1 = objects[curr].center_x,      cy1 = objects[curr].center_y;
        int cx2 = objects[best_next].center_x,  cy2 = objects[best_next].center_y;
        img_orig.DrawLine(cx1,   cy1,   cx2,   cy2,   0, 200, 255);
        img_orig.DrawLine(cx1+1, cy1,   cx2+1, cy2,   0, 200, 255);
        img_orig.DrawLine(cx1,   cy1+1, cx2,   cy2+1, 0, 200, 255);

        curr = best_next;
        visited[curr] = true;
    }

    // ---- Step 9: Save result ----
    img_orig.SaveBmp(out_file);
}

// ===========================================================================
// Menu helpers
// ===========================================================================

static void PrintSettings(const Settings& s)
{
    const char* thr_names[]  = {"Otsu (auto)", "Symmetric-Peak (auto)", "Manual"};
    const char* ctr_names[]  = {"None", "Grey World", "Range Stretch"};
    const char* mop_names[]  = {"None", "Open", "Close", "Open+Close"};
    const char* alg_names[]  = {"Flood Fill", "Sequential Scan"};
    const char* cnt_names[]  = {"Bounding Box", "Morphological Contour"};

    std::cout << "\n  Current settings:" << std::endl;
    std::cout << "    [1] Threshold method  : " << thr_names[s.threshold_method];
    if (s.threshold_method == 2) std::cout << " (" << s.manual_threshold << ")";
    std::cout << std::endl;
    std::cout << "    [2] Contrast correct  : " << ctr_names[s.contrast_method]  << std::endl;
    std::cout << "    [3] Median filter     : ";
    if (s.median_radius > 0) std::cout << "radius=" << s.median_radius;
    else                     std::cout << "Off";
    std::cout << std::endl;
    std::cout << "    [4] Morphology        : " << mop_names[s.morph_mode]
              << " (radius=" << s.morph_radius << ")" << std::endl;
    std::cout << "    [5] Component alg.    : " << alg_names[s.component_alg] << std::endl;
    std::cout << "    [6] Treasure outline  : " << cnt_names[s.contour_mode]   << std::endl;
    std::cout << "    [7] Min object area   : " << s.min_area
              << " (simple=50, noise=400-800)" << std::endl;
}

static void ConfigureSettings(Settings& s)
{
    while (true)
    {
        PrintSettings(s);
        std::cout << "\n  Choose setting to change (1-7) or 0 to go back: ";
        int opt;
        std::cin >> opt;
        if (std::cin.fail()) { std::cin.clear(); std::cin.ignore(INT_MAX, '\n'); continue; }

        if (opt == 0) break;

        if (opt == 1)
        {
            std::cout << "  Threshold method: 0=Otsu  1=SymmetricPeak  2=Manual: ";
            std::cin >> s.threshold_method;
            if (s.threshold_method == 2)
            {
                std::cout << "  Enter threshold value (0-255): ";
                std::cin >> s.manual_threshold;
            }
        }
        else if (opt == 2)
        {
            std::cout << "  Contrast: 0=None  1=GreyWorld  2=RangeStretch: ";
            std::cin >> s.contrast_method;
        }
        else if (opt == 3)
        {
            std::cout << "  Median filter radius (0=off, 1, 2, 3 ...): ";
            std::cin >> s.median_radius;
        }
        else if (opt == 4)
        {
            std::cout << "  Morphology: 0=None  1=Open  2=Close  3=Open+Close: ";
            std::cin >> s.morph_mode;
            std::cout << "  Morphology SE radius (1, 2, ...): ";
            std::cin >> s.morph_radius;
        }
        else if (opt == 5)
        {
            std::cout << "  Component algorithm: 0=FloodFill  1=SeqScan: ";
            std::cin >> s.component_alg;
        }
        else if (opt == 6)
        {
            std::cout << "  Treasure outline: 0=BoundingBox  1=MorphContour: ";
            std::cin >> s.contour_mode;
        }
        else if (opt == 7)
        {
            std::cout << "  Min object area (simple=50, noise=400-800): ";
            std::cin >> s.min_area;
        }
    }
}

// ===========================================================================
// main
// ===========================================================================
int main()
{
    const char* IN_FILES[]  = { "Klad00.bmp", "Klad01.bmp", "Klad02.bmp" };
    const char* OUT_FILES[] = { "Result_00.bmp", "Result_01.bmp", "Result_02.bmp" };
    constexpr int N_FILES   = 3;

    Settings settings;  // default settings (Otsu + morphological contour)

    while (true)
    {
        std::cout << "\n========================================" << std::endl;
        std::cout << "   Image Processing Lab 2              " << std::endl;
        std::cout << "========================================" << std::endl;
        std::cout << "  1. Process all images (current settings)" << std::endl;
        std::cout << "  2. Configure settings"                     << std::endl;
        std::cout << "  0. Exit"                                   << std::endl;
        std::cout << "Choice: ";

        int choice;
        std::cin >> choice;
        if (std::cin.fail()) { std::cin.clear(); std::cin.ignore(INT_MAX, '\n'); continue; }

        if (choice == 0) break;

        if (choice == 2)
        {
            ConfigureSettings(settings);
            continue;
        }

        if (choice == 1)
        {
            PrintSettings(settings);
            std::cout << std::endl;
            for (int f = 0; f < N_FILES; f++)
                ProcessImage(IN_FILES[f], OUT_FILES[f], settings);
            std::cout << "\nAll images processed." << std::endl;
        }
    }

    return 0;
}

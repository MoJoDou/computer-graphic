/* NAME: Ryskulov Niyaz epi-2-23
 * ASGN: N2
 */
#ifndef _IMAGE_H_
#define _IMAGE_H_

#include <vector>
#include <cmath>

// -------------------------------------------------------------------------
// SObject – properties of a single connected region found on the image
// -------------------------------------------------------------------------
struct SObject
{
    int min_x, max_x, min_y, max_y;        // axis-aligned bounding box
    int center_x, center_y;                 // centroid (pixel coordinates)
    int area;                                // foreground pixel count
    unsigned char r, g, b;                  // average color (sampled from original)
    double angle;                            // principal axis angle (radians)
    std::vector<std::pair<int,int>> pixels;  // list of (x, y) member pixels
};

// -------------------------------------------------------------------------
// CImage – 24-bit RGB BMP image container and image-processing routines
// -------------------------------------------------------------------------
class CImage
{
public:
    int            m_width;
    int            m_height;
    unsigned char* m_data;   // raw BGR pixel buffer (standard BMP bottom-up layout)

    CImage();
    ~CImage();

    // ---- I/O ----------------------------------------------------------
    bool LoadBmp(const char* filename);
    bool SaveBmp(const char* filename) const;

    // ---- Pixel access -------------------------------------------------
    void SetPixel(int x, int y, unsigned char r, unsigned char g, unsigned char b);
    void GetPixel(int x, int y, unsigned char& r, unsigned char& g, unsigned char& b) const;

    // ---- Contrast correction ------------------------------------------
    // Grey-world: scale each channel so its mean equals 128
    void GrayWorldCorrection();
    // Range stretch: per-channel linear stretch to [0, 255]
    void StretchContrast();

    // ---- Noise reduction ----------------------------------------------
    // Median filter: per-channel, square neighbourhood of size (2*radius+1)^2
    void MedianFilter(int radius);

    // ---- Binarization -------------------------------------------------
    // Fixed-threshold binarization (pixel > threshold → white)
    void Binarize(int threshold);
    // Auto threshold: Otsu's maximum between-class variance method
    // Returns the computed threshold; does NOT modify the image
    int OtsuThreshold() const;
    // Auto threshold: symmetric-peak (valley between two histogram modes)
    // Returns the computed threshold; does NOT modify the image
    int SymmetricPeakThreshold() const;

    // ---- Mathematical morphology (binary images) ----------------------
    void Erode(int radius);   // keep pixel white only if all SE neighbours are white
    void Dilate(int radius);  // set pixel white if any SE neighbour is white
    void Open(int radius);    // Erode then Dilate  (removes isolated blobs)
    void Close(int radius);   // Dilate then Erode  (fills small holes)

    // ---- Connected-component labeling ---------------------------------
    // Flood-fill variant (4-connectivity)
    void FindObjects(CImage& orig_img, std::vector<SObject>& objects);
    // Sequential-scan two-pass variant with union-find (4-connectivity)
    void FindObjectsSeqScan(CImage& orig_img, std::vector<SObject>& objects);

    // ---- Drawing helpers ----------------------------------------------
    void DrawRect(int x1, int y1, int x2, int y2,
                  unsigned char r, unsigned char g, unsigned char b);
    void DrawLine(int x1, int y1, int x2, int y2,
                  unsigned char r, unsigned char g, unsigned char b);
    // Morphological contour: draw boundary pixels of mask_obj onto this image
    void DrawContour(const SObject& mask_obj,
                     unsigned char r, unsigned char g, unsigned char b);
};

#endif 

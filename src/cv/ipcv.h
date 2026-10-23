#pragma once
#include "inkpath_config.h"
#include <iostream>
#include <opencv2/core.hpp>
#include <opencv2/core/mat.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/opencv.hpp>

namespace inkp {

typedef struct Shapes {
    std::vector<std::vector<cv::Point>> contours;
    std::vector<cv::Vec4i> hierarchy;
} Shapes;

// class StrokeFinder {
// }

// ipcv.cpp
cv::Mat thresholdWithPrep(cv::Mat img, std::string output_dir, const InkpathConfig* config = nullptr);
cv::Mat skeletonize(cv::Mat img_inv, std::string output_dir, const InkpathConfig* config = nullptr);
std::vector<std::vector<cv::Point>> findStrokes(cv::Mat img,
                                                std::string output_dir, const InkpathConfig* config = nullptr);

// Post-process white-background / black-skeleton image. Outputs unchanged when disabled.
struct DenoiseStats {
    int componentsBefore=0, componentsRemoved=0, pixelsRemoved=0;
    int densityComponentsRemoved=0;
};
cv::Mat denoiseSkeleton(const cv::Mat& skeleton, const InkpathConfig& cfg,
                       DenoiseStats* stats=nullptr, cv::Mat* removedMask=nullptr);

// Centerline graph mode (separate editable polyline strokes).
struct ReconstructionStats {
    int initialSegments=0, outputPaths=0, gapJoins=0, junctionPairs=0;
};
cv::Mat thinZhangSuen(const cv::Mat& blackOnWhite);
std::vector<std::vector<cv::Point>> reconstructStrokes(
    const cv::Mat& blackOnWhite, const InkpathConfig& cfg,
    ReconstructionStats* stats=nullptr, cv::Mat* addedMask=nullptr);

// quadrangle.cpp
cv::Mat getWhiteboard(cv::Mat image, std::string output_dir, const InkpathConfig* config = nullptr);
std::vector<std::vector<cv::Point>> filterQuadrangles(cv::Mat image,
                                                      std::string output_path, const InkpathConfig* config = nullptr);
void findQuadrangles(cv::Mat& image,
                     std::vector<std::vector<cv::Point>>& squares, const InkpathConfig* config = nullptr);
void sortPointsClockwise(std::vector<cv::Point>& points);
double angle(cv::Point pt1, cv::Point pt2, cv::Point pt0);
void drawSquares(cv::Mat& image,
                 const std::vector<std::vector<cv::Point>>& squares,
                 cv::Scalar color);

} // namespace inkp

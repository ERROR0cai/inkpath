#include "ipcv.h"
#include <opencv2/imgproc.hpp>
#include <opencv2/geometry.hpp>

namespace inkp {

// Grayscale and apply a threshold to the provided image
cv::Mat thresholdWithPrep(cv::Mat img, std::string output_dir, const InkpathConfig* config) {
    const InkpathConfig defaults;
    const auto& cfg = config ? *config : defaults;
    // Convert to grayscale for thresholding
    cv::Mat thresh_input;
    cvtColor(img, thresh_input, cv::COLOR_BGR2GRAY);

    // Use the same pixel-coordinate scale for both small and large images.
    // Do not automatically enlarge images whose side is below 1000 px:
    // that discontinuity changes the output stroke size and processing scale.
    cv::Mat upsampled = thresh_input;

    // Blur improves results
    cv::Mat blur;
    if (cfg.gaussianKernel == 1) blur = upsampled;
    else GaussianBlur(upsampled, blur, cv::Size(cfg.gaussianKernel, cfg.gaussianKernel), 0, 0);

    // adaptive thresholding after gaussian filtering
    cv::Mat gauss_thresh;

    // https://stackoverflow.com/questions/65891315/opencv-adaptive-thresholding-effective-noise-reduction
    // I was getting a lot of Rice Krispies in the image. This SO post told me
    // to increase C, and that made it mostly acceptable.

    // I also increased the blockSize from 11 -> 51, because typically I'm dealing with images
    // that are pretty high resolution (cellphone cameras)
    adaptiveThreshold(blur, gauss_thresh, 255, cv::ADAPTIVE_THRESH_GAUSSIAN_C,
                      cv::THRESH_BINARY, cfg.adaptiveBlockSize, cfg.adaptiveC);

#ifdef INKPATH_DEBUG
    if (output_dir != "") {
        std::string opath = output_dir + "otsu.jpg";
        cv::imwrite(opath, gauss_thresh);
        std::cout << "Image has been written to " << opath << "\n";
    }
#endif // INKPATH_DEBUG

    return gauss_thresh;
}

// Skeletonization algorithm to shrink the fat strokes that should be visible in
// the thresholded image into 1-pixel-wide lines in prep for contour detection.
// TODO: Find a way to keep track of the thickness of contours and pass that
// to Xournal++ for better transcription.
// TODO: Where did I find this?
cv::Mat skeletonize(cv::Mat img_inv, std::string output_dir, const InkpathConfig* config) {
    const InkpathConfig defaults;
    const auto& cfg = config ? *config : defaults;
    cv::Mat img;
    bitwise_not(img_inv, img);
    cv::Mat skel(img.size(), CV_8UC1, cv::Scalar(0));
    cv::Mat temp;
    cv::Mat eroded;

    cv::Mat element =
        cv::getStructuringElement(
            cfg.skeletonShape == "RECT" ? cv::MORPH_RECT :
            cfg.skeletonShape == "ELLIPSE" ? cv::MORPH_ELLIPSE : cv::MORPH_CROSS,
            cv::Size(cfg.skeletonKernel, cfg.skeletonKernel));

    bool done;
    do {
        cv::erode(img, eroded, element);
        cv::dilate(eroded, temp, element); // temp = open(img)
        cv::subtract(img, temp, temp);
        cv::bitwise_or(skel, temp, skel);
        eroded.copyTo(img);

        done = (cv::countNonZero(img) == 0);
    } while (!done);

    cv::Mat skel_invert;
    bitwise_not(skel, skel_invert);

#ifdef INKPATH_DEBUG
    if (output_dir != "") {
        imwrite(output_dir + "skel.jpg", skel_invert);
        std::cout << "Image has been written to " << output_dir << "\n";
    }
#endif // INKPATH_DEBUG

    return skel_invert;
}

// Perform contour detection
// Prereqs: Must be binary color image, target must be black
std::vector<std::vector<cv::Point>> findStrokes(cv::Mat img,
                                                std::string output_dir, const InkpathConfig* config) {
    const InkpathConfig defaults;
    const auto& cfg = config ? *config : defaults;
    // XXX (wdn): Does this bitwise not matter?
    // From the docs, object to be found should be white, and background
    // should be black. Maybe I should check if there's more black than
    // white in the image and flip if necessary
    cv::Mat src;
    bitwise_not(img, src);

    cv::Mat dst = cv::Mat::zeros(src.rows, src.cols, CV_8UC3);
    src = src > 1;
    std::vector<std::vector<cv::Point>> strokes;
    const int retrieval = cfg.retrievalMode == "RETR_EXTERNAL" ? cv::RETR_EXTERNAL :
        cfg.retrievalMode == "RETR_TREE" ? cv::RETR_TREE :
        cfg.retrievalMode == "RETR_CCOMP" ? cv::RETR_CCOMP : cv::RETR_LIST;
    const int chain = cfg.chainMode == "CHAIN_APPROX_SIMPLE" ? cv::CHAIN_APPROX_SIMPLE : cv::CHAIN_APPROX_NONE;
    findContours(src, strokes, retrieval, chain);

    // Keep small contours, including thin and near-zero-area strokes.
    // Earlier area filtering removed legitimate fine handwriting segments.
    // The Lua side still skips contours with fewer than two points.

#ifdef INKPATH_DEBUG
    if (output_dir != "") {
        // iterate through all the top-level strokes,
        // draw each connected component with its own random color
        for (size_t i = 0; i < strokes.size(); i++) {
            cv::Scalar color(rand() & 255, rand() & 255,
                             rand() & 255); // Random color
            drawContours(dst, strokes, (int)i, color, 2, cv::LINE_8,
                         cv::noArray(), 0);
        }
        std::string opath = output_dir + "strokes.jpg";

        imwrite(opath, dst);
        std::cout << "Image has been written to " << opath << "\n";
    }
#endif

    return strokes;
}
// Component-based post processing: black is foreground in the supplied skeleton.
// A candidate is removed only if it is small AND has no sizable component
// nearby. Density adaptation can raise the allowed candidate size in sparse areas.
cv::Mat denoiseSkeleton(const cv::Mat& skeleton, const InkpathConfig& cfg,
                       DenoiseStats* stats, cv::Mat* removedMask) {
    if (skeleton.empty() || skeleton.type()!=CV_8UC1)
        throw std::runtime_error("Denoise expects a grayscale skeleton image");
    if (stats) *stats = DenoiseStats{};
    if (removedMask) *removedMask = cv::Mat::zeros(skeleton.size(), CV_8UC1);
    if (cfg.denoiseEnabled==0) return skeleton.clone();

    cv::Mat foreground;
    cv::bitwise_not(skeleton, foreground);
    cv::threshold(foreground, foreground, 0, 255, cv::THRESH_BINARY);
    cv::Mat labels, components, centroids;
    const int n = cv::connectedComponentsWithStats(foreground, labels, components,
                                                   centroids, 8, CV_32S);
    if (stats) stats->componentsBefore = n-1;
    if (n<=1) return skeleton.clone();

    // The reference mask contains only large, trustworthy components. Exclude
    // small components so two nearby specks cannot protect each other.
    cv::Mat anchors = cv::Mat::zeros(foreground.size(), CV_8UC1);
    // Keep conservative-mode anchors independent of density-only settings.
    // Density mode may raise the candidate size, so its anchors must be
    // larger than that mode's maximum candidate size.
    const int activeCandidateCap = cfg.denoiseDensityEnabled == 1
        ? cfg.denoiseSparseMaxPixels : cfg.denoiseMaxComponentPixels;
    const int anchorLimit = std::max(cfg.denoiseAnchorMinPixels,
                                     activeCandidateCap + 1);
    std::vector<unsigned char> anchorLabel(static_cast<size_t>(n),0);
    for (int i=1;i<n;++i)
        if (components.at<int>(i,cv::CC_STAT_AREA)>=anchorLimit)
            anchorLabel[static_cast<size_t>(i)] = 1;
    for (int y=0;y<labels.rows;++y) {
        const int* ids=labels.ptr<int>(y);
        unsigned char* out=anchors.ptr<unsigned char>(y);
        for (int x=0;x<labels.cols;++x)
            if (anchorLabel[static_cast<size_t>(ids[x])]) out[x]=255;
    }

    cv::Mat protectedPixels;
    if(cfg.denoiseNeighborRadius>0) {
        // Distance to the nearest anchor pixel; background distanceTransform
        // has zero at anchor pixels.
        cv::Mat nonAnchors;
        cv::bitwise_not(anchors, nonAnchors);
        cv::distanceTransform(nonAnchors, protectedPixels, cv::DIST_L2, 3);
    } else {
        protectedPixels = cv::Mat::zeros(skeleton.size(), CV_32F);
        protectedPixels.setTo(1e9f);
        protectedPixels.setTo(0.f, anchors);
    }

    cv::Mat density;
    if(cfg.denoiseDensityEnabled==1) {
        // Density is computed only from anchor components, avoiding circular
        // self-support by the candidate and suppressing isolated speck clusters.
        cv::Mat anchorFloat;
        anchors.convertTo(anchorFloat,CV_32F,1.0/255.0);
        cv::boxFilter(anchorFloat,density,CV_32F,
            cv::Size(cfg.denoiseDensityWindow,cfg.denoiseDensityWindow),
            cv::Point(-1,-1),true,cv::BORDER_CONSTANT);
    }

    std::vector<unsigned char> remove(static_cast<size_t>(n),0);
    for(int i=1;i<n;++i) {
        const int area=components.at<int>(i,cv::CC_STAT_AREA);
        // When density adaptation is off, never consult sparse_max_pixels.
        if(area>activeCandidateCap) continue;
        const int left=components.at<int>(i,cv::CC_STAT_LEFT);
        const int top=components.at<int>(i,cv::CC_STAT_TOP);
        const int width=components.at<int>(i,cv::CC_STAT_WIDTH);
        const int height=components.at<int>(i,cv::CC_STAT_HEIGHT);
        float nearest=1e9f;
        double maxDensity=0;
        for(int y=top;y<top+height;++y) {
            const int* ids=labels.ptr<int>(y);
            const float* distances=protectedPixels.ptr<float>(y);
            const float* densities=density.empty()?nullptr:density.ptr<float>(y);
            for(int x=left;x<left+width;++x) {
                if(ids[x]!=i) continue;
                nearest=std::min(nearest,distances[x]);
                if(densities) maxDensity=std::max(maxDensity,static_cast<double>(densities[x]));
            }
        }
        if(nearest<=static_cast<float>(cfg.denoiseNeighborRadius)) continue;
        const bool sparse=cfg.denoiseDensityEnabled==1 &&
                          maxDensity<cfg.denoiseLowDensityThreshold;
        const int cap=sparse?cfg.denoiseSparseMaxPixels:cfg.denoiseMaxComponentPixels;
        if(area<=cap) {
            remove[static_cast<size_t>(i)]=1;
            if(stats) {
                ++stats->componentsRemoved;
                stats->pixelsRemoved+=area;
                if(sparse && area>cfg.denoiseMaxComponentPixels)
                    ++stats->densityComponentsRemoved;
            }
        }
    }
    cv::Mat output=skeleton.clone();
    for(int y=0;y<labels.rows;++y) {
        const int* ids=labels.ptr<int>(y);
        unsigned char* dst=output.ptr<unsigned char>(y);
        unsigned char* mark=removedMask?removedMask->ptr<unsigned char>(y):nullptr;
        for(int x=0;x<labels.cols;++x)
            if(remove[static_cast<size_t>(ids[x])]) {
                dst[x]=255;
                if(mark) mark[x]=255;
            }
    }
    return output;
}

} // namespace inkp

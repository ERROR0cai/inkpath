#include "plugin.h"
#include "../cv/edge_line_filter.h"
#include <algorithm>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <vector>
#include <stdexcept>
#include <string>
#include <opencv2/imgproc.hpp>
#include <opencv2/imgcodecs.hpp>
#include <cstdlib>
#include <sstream>
#include <chrono>
#include <iomanip>
#include <cmath>
#include <climits>
#include <cstddef>

// Validate a live Inkpath userdata before accessing its native object.
static Inkpath* checkedInkpath(lua_State* L) {
    auto** slot = static_cast<Inkpath**>(luaL_checkudata(L, 1, LUA_INKPATH));
    if (*slot == nullptr) {
        luaL_error(L, "Inkpath object has already been destroyed");
        return nullptr;
    }
    return *slot;
}

// Lua uses zero-based stroke indexes for this plugin's existing API.
static size_t checkedStrokeIndex(lua_State* L, lua_Integer index, size_t count) {
    if (index < 0 || static_cast<unsigned long long>(index) >= count) {
        luaL_error(L, "Stroke index out of range (count=%llu)",
                   static_cast<unsigned long long>(count));
        return 0;
    }
    return static_cast<size_t>(index);
}

// Register inkpath to Lua
static void registerInkpath(lua_State* L) {
    lua_register(L, LUA_INKPATH, inkpath_new);
    luaL_newmetatable(L, LUA_INKPATH);
    lua_pushcfunction(L, inkpath_delete);
    lua_setfield(L, -2, "__gc");
    lua_pushvalue(L, -1);
    lua_setfield(L, -2, "__index");
    lua_pushcfunction(L, inkpath_getStrokeCount);
    lua_setfield(L, -2, "getStrokeCount");
    lua_pushcfunction(L, inkpath_getStrokeLength);
    lua_setfield(L, -2, "getStrokeLength");
    lua_pushcfunction(L, inkpath_getStroke);
    lua_setfield(L, -2, "getStroke");
    lua_pushcfunction(L, [](lua_State* state) -> int {
        auto* obj = checkedInkpath(state);
        const auto& report = obj->getDiagnosticReport();
        lua_pushlstring(state, report.data(), report.size());
        return 1;
    });
    lua_setfield(L, -2, "getDiagnosticReport");
    lua_pop(L, 1);
}

extern "C" {
// Loads function headers into memory so Lua can access them
WINEXPORT int luaopen_loadInkpath(lua_State* L) {
    printf("Entered Inkpath.");
    luaL_openlibs(L);
    registerInkpath(L);
    lua_pushboolean(L, 1);
    return 1;
}
}

// Mode: 0 = auto, 1 = force dark strokes on light background,
//       2 = force light strokes on dark background (invert first).
static int cv_perform_processing_mode(const char* image_path, Inkpath* data, int mode, const inkp::InkpathConfig& cfg);

// Create & return inkpath instance to Lua.
static int inkpath_new(lua_State* L) {
    const char* image_path = luaL_checkstring(L, 1);
    const lua_Integer polarity_mode = luaL_optinteger(L, 2, 0);
    const char* configPath = luaL_optstring(L, 3, "");
    if (polarity_mode < 0 || polarity_mode > 2)
        return luaL_error(L, "Invalid polarity mode (expected 0, 1 or 2)");

    // Allocate the Lua userdata first. On failure it remains null and __gc is safe.
    auto** slot = static_cast<Inkpath**>(lua_newuserdata(L, sizeof(Inkpath*)));
    *slot = nullptr;
    luaL_setmetatable(L, LUA_INKPATH);

    bool failed = false;
    {
        std::string error;
        try {
            *slot = new Inkpath();
            const inkp::InkpathConfig config = inkp::loadInkpathConfig(configPath);
            cv_perform_processing_mode(image_path, *slot, static_cast<int>(polarity_mode), config);
        } catch (const cv::Exception& e) {
            error = std::string("OpenCV processing failed: ") + e.what();
        } catch (const std::exception& e) {
            error = std::string("Image processing failed: ") + e.what();
        } catch (...) {
            error = "Unknown native image processing error";
        }
        if (!error.empty()) {
            // Clean up before raising a Lua error (Lua may longjmp).
            delete *slot;
            *slot = nullptr;
            lua_pushlstring(L, error.data(), error.size());
            failed = true;
        }
    }
    if (failed) return lua_error(L);
    std::cout << "CV Processing complete!\n";
    return 1;
}

// Get the number of strokes that Inkpath found in the image.
static int inkpath_getStrokeCount(lua_State* L) {
    const auto& strokes = checkedInkpath(L)->get();
    lua_pushinteger(L, static_cast<lua_Integer>(strokes.size()));
    return 1;
}

// Given a zero-based index, get the length of that stroke.
static int inkpath_getStrokeLength(lua_State* L) {
    Inkpath* obj = checkedInkpath(L);
    lua_Integer inputIndex = luaL_checkinteger(L, 2);
    const auto& strokes = obj->get();
    size_t index = checkedStrokeIndex(L, inputIndex, strokes.size());
    lua_pushinteger(L, static_cast<lua_Integer>(strokes[index].size()));
    return 1;
}

// Push x and y coordinates as two Lua arrays, preserving the existing API.
static int inkpath_getStroke(lua_State* L) {
    Inkpath* obj = checkedInkpath(L);
    lua_Integer inputIndex = luaL_checkinteger(L, 2);
    double scalingFactor = luaL_checknumber(L, 3);
    if (!std::isfinite(scalingFactor))
        return luaL_error(L, "Invalid scaling factor (expected finite number)");
    const auto& strokes = obj->get();
    size_t index = checkedStrokeIndex(L, inputIndex, strokes.size());
    const auto& selectedStroke = strokes[index];

    lua_createtable(L, static_cast<int>(std::min<size_t>(selectedStroke.size(), INT_MAX)), 0);
    for (size_t i = 0; i < selectedStroke.size(); ++i) {
        lua_pushnumber(L, static_cast<lua_Number>(selectedStroke[i].x) * scalingFactor);
        lua_rawseti(L, -2, static_cast<lua_Integer>(i + 1));
    }

    lua_createtable(L, static_cast<int>(std::min<size_t>(selectedStroke.size(), INT_MAX)), 0);
    for (size_t i = 0; i < selectedStroke.size(); ++i) {
        lua_pushnumber(L, static_cast<lua_Number>(selectedStroke[i].y) * scalingFactor);
        lua_rawseti(L, -2, static_cast<lua_Integer>(i + 1));
    }
    return 2;
}

// Release native object at most once. __gc must not throw.
static int inkpath_delete(lua_State* L) {
    auto** slot = static_cast<Inkpath**>(luaL_testudata(L, 1, LUA_INKPATH));
    if (slot != nullptr && *slot != nullptr) {
        delete *slot;
        *slot = nullptr;
    }
    return 0;
}

// Save diagnostic PNG files using filesystem paths, including Unicode paths on Windows.
static void saveDiagnosticPng(const std::filesystem::path& path, const cv::Mat& mat) {
    std::vector<unsigned char> encoded;
    if (!cv::imencode(".png", mat, encoded))
        throw std::runtime_error("Could not encode diagnostic PNG");
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) throw std::runtime_error("Cannot create diagnostic image file");
    out.write(reinterpret_cast<const char*>(encoded.data()),
              static_cast<std::streamsize>(encoded.size()));
    if (!out) throw std::runtime_error("Cannot write diagnostic image file");
}

static std::filesystem::path diagnosticDirectory() {
    const char* temp = std::getenv("TEMP");
    if (!temp || !*temp) temp = std::getenv("TMP");
    if (!temp || !*temp)
        throw std::runtime_error("TEMP/TMP directory unavailable for diagnostics");
    // Windows getenv paths are in system-native encoding; use the native constructor.
    std::filesystem::path dir = std::filesystem::path(temp) / "InkpathDiagnostics";
    std::filesystem::create_directories(dir);
    return dir;
}

// Do OpenCV stuff
int cv_perform_processing(const char* image_path, Inkpath* data) {
    return cv_perform_processing_mode(image_path, data, 0, inkp::InkpathConfig{});
}

static int cv_perform_processing_mode(const char* image_path, Inkpath* data, int mode, const inkp::InkpathConfig& cfg) {
    // Decoding huge phone scans may still require substantial memory.
    // The cap below reduces subsequent processing memory and CPU usage.
    // Lua / GTK paths are UTF-8. On Windows cv::imread may treat a
    // narrow filename using the system ANSI code page and fail for Chinese
    // names. Open the path through std::filesystem instead, then decode
    // the JPEG/PNG/BMP bytes in memory.
    std::filesystem::path nativePath = std::filesystem::u8path(image_path);
    std::ifstream file(nativePath, std::ios::binary);
    if (!file) {
        throw std::runtime_error("Cannot open image file (check UTF-8 path and permissions)");
    }
    std::vector<unsigned char> bytes(
        (std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    if (file.bad() || bytes.empty()) {
        throw std::runtime_error("Image file is empty or could not be read");
    }
    cv::Mat img = cv::imdecode(bytes, cv::IMREAD_COLOR);
    if (img.empty()) {
        throw std::runtime_error("Could not decode image bytes (unsupported or damaged image)");
    }
    const int maxSide = cfg.maxSide;
    const int largest = std::max(img.cols, img.rows);
    if (largest > maxSide) {
        const double scale = static_cast<double>(maxSide) / largest;
        cv::Mat smaller;
        cv::resize(img, smaller, cv::Size(), scale, scale, cv::INTER_AREA);
        img = std::move(smaller);
    }

    // Normalize the input to dark strokes on a light background.
    // In forced modes, bypass the automatic heuristic entirely.
    bool invert = false;
    if (mode == 2) {
        invert = true;
        std::cout << "Inkpath: forced light-on-dark mode" << std::endl;
    } else if (mode == 1) {
        std::cout << "Inkpath: forced dark-on-light mode" << std::endl;
    } else {
        cv::Mat gray, mask;
        cv::cvtColor(img, gray, cv::COLOR_BGR2GRAY);
        cv::threshold(gray, mask, 0, 255, cv::THRESH_BINARY | cv::THRESH_OTSU);
        const double brightRatio = static_cast<double>(cv::countNonZero(mask)) /
                                   static_cast<double>(mask.total());
        invert = brightRatio < cfg.brightRatio;
        std::cout << "Inkpath: auto mode, bright ratio " << brightRatio
                  << ", invert=" << invert << std::endl;
    }
    if (invert) cv::bitwise_not(img, img);

    // Screenshots and very narrow text strips are already front-facing.
    // Quadrangle detection on such images can mistake glyph edges for a
    // "whiteboard", leading to cropping and slanted text. Skip correction.
    // Camera photos of larger pages retain the original two-pass behavior.
    const int shortSide = std::min(img.cols, img.rows);
    const int longSide = std::max(img.cols, img.rows);
    const bool isTextStrip = shortSide < cfg.textStripShortSide ||
                             static_cast<double>(longSide) / shortSide > cfg.textStripAspectRatio;
    cv::Mat prepared = img;
    if (isTextStrip || cfg.perspectivePasses == 0) {
        std::cout << "Inkpath: skipping perspective correction "
                  << img.cols << "x" << img.rows << std::endl;
    } else {
        for (int pass = 0; pass < cfg.perspectivePasses; ++pass) {
            prepared = getWhiteboard(prepared, "", &cfg);
            if (prepared.empty()) {
                throw std::runtime_error("Whiteboard correction returned an empty image");
            }
        }
    }
    cv::Mat binary = thresholdWithPrep(prepared, "", &cfg);
    if (binary.empty()) {
        throw std::runtime_error("Image thresholding returned an empty image");
    }
    // Preserve the pre-repair image only when diagnostics are enabled.
    cv::Mat beforeRepair;
    if (cfg.diagnosticEnabled == 1) beforeRepair = binary.clone();
    // 可选：在骨架提取前，对二值图中的深色笔画执行闭运算。
    // 输入：白底黑字（binary）；OpenCV 形态学运算的前景需为白色。
    // 默认关闭，避免意外连接相邻字符。
    if (cfg.bridgeEnabled == 1 && cfg.strokeMode == "CONTOUR") {
        cv::Mat foreground, joined;
        cv::bitwise_not(binary, foreground);
        const int shape = cfg.bridgeShape == "CROSS" ? cv::MORPH_CROSS :
                          cfg.bridgeShape == "RECT" ? cv::MORPH_RECT : cv::MORPH_ELLIPSE;
        cv::Mat element = cv::getStructuringElement(
            shape, cv::Size(cfg.bridgeKernel, cfg.bridgeKernel));
        cv::morphologyEx(foreground, joined, cv::MORPH_CLOSE,
                         element, cv::Point(-1, -1), cfg.bridgeIterations);
        cv::bitwise_not(joined, binary);
    }
    cv::Mat afterRepair;
    if (cfg.diagnosticEnabled == 1) afterRepair = binary.clone();
    cv::Mat skeleton = (cfg.strokeMode == "GRAPH" && cfg.thinMethod == "ZHANG_SUEN")
        ? inkp::thinZhangSuen(binary) : skeletonize(binary, "", &cfg);
    if (skeleton.empty()) {
        throw std::runtime_error("Skeletonization returned an empty image");
    }
    // Optional post-skeleton denoiser. The original skeleton diagnostic remains
    // available, so effects can be distinguished from prior repair stages.
    inkp::DenoiseStats denoiseStats;
    cv::Mat removedPixels;
    cv::Mat cleanedSkeleton = inkp::denoiseSkeleton(
        skeleton, cfg, &denoiseStats,
        cfg.diagnosticEnabled == 1 ? &removedPixels : nullptr);
    inkp::ReconstructionStats graphStats;
    cv::Mat graphAdded;
    std::vector<std::vector<cv::Point>> outputStrokes;
    if (cfg.strokeMode == "GRAPH") {
        outputStrokes = inkp::reconstructStrokes(cleanedSkeleton, cfg, &graphStats,
            cfg.diagnosticEnabled == 1 ? &graphAdded : nullptr);
    } else {
        outputStrokes = findStrokes(cleanedSkeleton, "", &cfg);
    }
    inkp::EdgeLineStats edgeLineStats;
    cv::Mat edgeLineRemoved;
    outputStrokes=inkp::removeCollinearEdgeGroups(
        outputStrokes, cleanedSkeleton.cols, cleanedSkeleton.rows, cfg,
        &edgeLineStats, cfg.diagnosticEnabled == 1 ? &edgeLineRemoved : nullptr);
    if (cfg.diagnosticEnabled == 1) {
        // Count pixels changed by repair; a separate changed-pixel image would
        // be useful later, but these three images capture the core stages.
        cv::Mat difference;
        cv::compare(beforeRepair, afterRepair, difference, cv::CMP_NE);
        const int changed = cv::countNonZero(difference);
        const int total = beforeRepair.rows * beforeRepair.cols;
        const double percent = total > 0 ? (100.0 * changed / total) : 0.0;

        const std::filesystem::path dir = diagnosticDirectory();
        saveDiagnosticPng(dir / "01_threshold.png", beforeRepair);
        saveDiagnosticPng(dir / "02_repaired.png", afterRepair);
        saveDiagnosticPng(dir / "03_skeleton.png", skeleton);
        saveDiagnosticPng(dir / "04_denoised.png", cleanedSkeleton);
        saveDiagnosticPng(dir / "05_removed_mask.png", removedPixels);
        if (cfg.edgeLinesEnabled == 1)
            saveDiagnosticPng(dir / "08_edge_lines_removed.png", edgeLineRemoved);
        if (cfg.strokeMode == "GRAPH") {
            saveDiagnosticPng(dir / "06_graph_added_mask.png", graphAdded);
            cv::Mat rendered(cleanedSkeleton.size(), CV_8UC1, cv::Scalar(255));
            for (const auto& path : outputStrokes)
                for (size_t i=1;i<path.size();++i)
                    cv::line(rendered,path[i-1],path[i],cv::Scalar(0),1,cv::LINE_8);
            saveDiagnosticPng(dir / "07_reconstructed.png", rendered);
        }
        std::ostringstream report;
        report << "Inkpath 图像处理诊断\n"
               << "笔迹提取模式：" << cfg.strokeMode << "\n"
               << "拓扑初始线段：" << graphStats.initialSegments << "\n"
               << "交点配对：" << graphStats.junctionPairs << "\n"
               << "断口连接：" << graphStats.gapJoins << "\n"
               << "边缘多段线过滤：" << (cfg.edgeLinesEnabled ? "开启" : "关闭") << "\n"
               << "候选近似直线段：" << edgeLineStats.candidatePaths << "\n"
               << "删除连续共线子组数：" << edgeLineStats.removedGroups << "\n"
               << "删除超长单条笔迹数：" << edgeLineStats.removedSingles << "\n"
               << "删除笔迹路径数：" << edgeLineStats.removedPaths << "\n"
               << "最终独立笔迹：" << outputStrokes.size() << "\n"
               << "旧闭运算修复：" << (cfg.strokeMode == "CONTOUR" && cfg.bridgeEnabled == 1 ? "开启" : "未使用") << "\n"
               << "邻域尺寸：" << cfg.bridgeKernel << "\n"
               << "重复次数：" << cfg.bridgeIterations << "\n"
               << "修复前后变化像素：" << changed << " / " << total << "\n"
               << "变化比例：" << std::fixed << std::setprecision(5) << percent << "%\n"
               << "去噪功能：" << (cfg.denoiseEnabled==1 ? "开启" : "关闭") << "\n"
               << "密度自适应：" << (cfg.denoiseDensityEnabled==1 ? "开启" : "关闭") << "\n"
               << "去噪前连通域：" << denoiseStats.componentsBefore << "\n"
               << "删除连通域：" << denoiseStats.componentsRemoved << "\n"
               << "删除像素：" << denoiseStats.pixelsRemoved << "\n"
               << "密度自适应额外删除：" << denoiseStats.densityComponentsRemoved << "\n"
               << "诊断图片保存于：\n" << dir.string() << "\n\n"
               << "01_threshold.png：修复前的二值图\n"
               << "02_repaired.png：修复后的二值图\n"
               << "03_skeleton.png：骨架提取结果\n"
               << "04_denoised.png：去噪后骨架\n"
               << "05_removed_mask.png：删除像素掩码（白色为删除）";
        if (cfg.strokeMode == "GRAPH") report << "\n06_graph_added_mask.png：新连接像素\n07_reconstructed.png：重建中心线";
        if(cfg.edgeLinesEnabled)report << "\n08_edge_lines_removed.png：过滤掉的近共线笔迹点";
        data->setDiagnosticReport(report.str());
    }
    data->set(std::move(outputStrokes));
    return 0;
}

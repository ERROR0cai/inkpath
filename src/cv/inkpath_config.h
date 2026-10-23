#pragma once
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
#include <sstream>
#include <cctype>

namespace inkp {
struct InkpathConfig {
    int maxSide=2600;
    double brightRatio=0.5;
    int textStripShortSide=250;
    double textStripAspectRatio=4.0;
    int perspectivePasses=2;
    double quadMinAreaRatio=0.25, quadMaxAreaRatio=0.90;
    int quadBorderWidth=10, quadMedianKernel=9;
    double cannyLow=10, cannyHigh=20;
    int cannyAperture=3;
    double quadApproxEpsilon=0.02, quadMinContourArea=1000, quadMaxAngleCosine=0.3;
    int quadThresholdLevels=2;
    int gaussianKernel=3, adaptiveBlockSize=31;
    int bridgeEnabled=0, bridgeKernel=3, bridgeIterations=1;
    std::string bridgeShape="ELLIPSE";
    double adaptiveC=5.0;
    int diagnosticEnabled=0;
    // Skeleton post-processing denoiser; density adaptation is opt-in.
    int denoiseEnabled=1;
    int denoiseMaxComponentPixels=6;
    int denoiseNeighborRadius=20;
    int denoiseAnchorMinPixels=15;
    int denoiseDensityEnabled=0;
    int denoiseDensityWindow=65;
    double denoiseLowDensityThreshold=0.01;
    int denoiseSparseMaxPixels=12;
    // Graph reconstruction (opt-in; legacy mode stays available).
    std::string strokeMode="GRAPH"; // GRAPH or CONTOUR
    std::string thinMethod="ZHANG_SUEN"; // ZHANG_SUEN or MORPH
    int graphGapEnabled=1, graphMaxGap=7, graphDirectionWindow=6;
    double graphSmoothAngle=40.0;
    int graphCornerEnabled=1, graphCornerMaxGap=3;
    double graphCornerAngle=105.0;
    int graphJunctionPairing=1;
    double graphJunctionAngle=40.0;
    int graphMinPoints=2;
    int graphGuardEnabled=1;
    double graphMaxGapRatio=0.18;
    // Remove groups of nearly collinear disconnected paths (off by default).
    int edgeLinesEnabled=0;
    int edgeLinesMargin=80;       // 0 = whole image; positive = border strip
    int edgeLinesMinSegments=3;
    int edgeLinesSingleEnabled=1;
    double edgeLinesSingleMinLength=300.0;
    double edgeLinesMinTotalLength=180.0;
    double edgeLinesMinSpan=180.0;
    double edgeLinesMaxGap=22.0;
    double edgeLinesMaxOffset=3.0;
    double edgeLinesMaxAngle=8.0;
    double edgeLinesMaxDeviation=2.5;
    int edgeLinesMinSegmentPoints=3;
    int skeletonKernel=3;
    std::string skeletonShape="CROSS";
    std::string retrievalMode="RETR_LIST", chainMode="CHAIN_APPROX_NONE";
};
inline std::string cfgTrim(std::string s) {
    const auto a=s.find_first_not_of(" \t\r\n");
    if (a==std::string::npos) return "";
    const auto b=s.find_last_not_of(" \t\r\n");
    return s.substr(a,b-a+1);
}
inline InkpathConfig loadInkpathConfig(const std::string& path) {
    InkpathConfig c;
    if (path.empty()) return c;
    std::ifstream in(std::filesystem::u8path(path));
    if (!in.is_open()) {
        if (std::filesystem::exists(std::filesystem::u8path(path)))
            throw std::runtime_error("Cannot read Inkpath config.ini");
        return c; // Missing file => previous defaults.
    }
    std::map<std::string,std::string> entries;
    std::string line,section;
    int lineNumber=0;
    while (std::getline(in,line)) {
        ++lineNumber;
        if (lineNumber==1 && line.size()>=3 &&
            static_cast<unsigned char>(line[0])==0xef &&
            static_cast<unsigned char>(line[1])==0xbb &&
            static_cast<unsigned char>(line[2])==0xbf) line.erase(0,3);
        line=cfgTrim(line);
        if (line.empty() || line[0]==';' || line[0]=='#') continue;
        if (line.front()=='[' && line.back()==']') { section=cfgTrim(line.substr(1,line.size()-2)); continue; }
        const auto pos=line.find('=');
        if (pos==std::string::npos) throw std::runtime_error("Invalid config.ini line "+std::to_string(lineNumber));
        auto name=cfgTrim(line.substr(0,pos));
        auto value=cfgTrim(line.substr(pos+1));
        const auto comment=value.find_first_of(";#");
        if (comment!=std::string::npos) value=cfgTrim(value.substr(0,comment));
        if (name.empty() || value.empty()) throw std::runtime_error("Empty config.ini key/value at line "+std::to_string(lineNumber));
        std::string key=section+"."+name;
        if (!entries.emplace(key,value).second) throw std::runtime_error("Duplicate config.ini key: "+key);
    }
    if (in.bad()) throw std::runtime_error("Error while reading config.ini");
    auto number=[&](const std::string& key, double def, double lo, double hi)->double {
        auto it=entries.find(key); if(it==entries.end())return def;
        size_t used=0;double v=0;
        try {v=std::stod(it->second,&used);} catch(...) {throw std::runtime_error("Invalid number for "+key);}
        if(used!=it->second.size() || !std::isfinite(v) || v<lo || v>hi) throw std::runtime_error("Out-of-range value for "+key);
        entries.erase(it);return v;
    };
    auto integer=[&](const std::string& key,int def,int lo,int hi)->int {
        double v=number(key,def,lo,hi);
        if(std::floor(v)!=v) throw std::runtime_error("Expected integer for "+key);
        return static_cast<int>(v);
    };
    auto odd=[&](const std::string& key,int def,int lo,int hi)->int {
        int v=integer(key,def,lo,hi);
        if(v%2!=1) throw std::runtime_error("Expected odd value for "+key);
        return v;
    };
    auto choice=[&](const std::string& key, std::string def, std::initializer_list<const char*> options)->std::string {
        auto it=entries.find(key);if(it==entries.end())return def;
        const std::string value=it->second;
        bool accepted=false;for(auto opt:options)if(value==opt)accepted=true;
        if(!accepted)throw std::runtime_error("Invalid option for "+key+": "+value);
        entries.erase(it);return value;
    };
    c.maxSide=integer("image.max_side",c.maxSide,128,16384);
    c.brightRatio=number("image.bright_ratio",c.brightRatio,0,1);
    c.textStripShortSide=integer("perspective.min_short_side",c.textStripShortSide,1,8192);
    c.textStripAspectRatio=number("perspective.max_aspect_ratio",c.textStripAspectRatio,1,100);
    c.perspectivePasses=integer("perspective.passes",c.perspectivePasses,0,2);
    c.quadMinAreaRatio=number("perspective.min_area_ratio",c.quadMinAreaRatio,0,1);
    c.quadMaxAreaRatio=number("perspective.max_area_ratio",c.quadMaxAreaRatio,0,1);
    if(c.quadMinAreaRatio>=c.quadMaxAreaRatio)throw std::runtime_error("min_area_ratio must be smaller than max_area_ratio");
    c.quadBorderWidth=integer("perspective.border_width",c.quadBorderWidth,0,100);
    c.quadMedianKernel=odd("perspective.median_kernel",c.quadMedianKernel,3,31);
    c.cannyLow=number("perspective.canny_low",c.cannyLow,0,1000);
    c.cannyHigh=number("perspective.canny_high",c.cannyHigh,0,1000);
    if(c.cannyLow>c.cannyHigh)throw std::runtime_error("canny_low must be <= canny_high");
    c.cannyAperture=odd("perspective.canny_aperture",c.cannyAperture,3,7);
    c.quadApproxEpsilon=number("perspective.approx_epsilon",c.quadApproxEpsilon,0.0001,0.5);
    c.quadMinContourArea=number("perspective.min_contour_area",c.quadMinContourArea,0,1e8);
    c.quadMaxAngleCosine=number("perspective.max_angle_cosine",c.quadMaxAngleCosine,0,1);
    c.quadThresholdLevels=integer("perspective.threshold_levels",c.quadThresholdLevels,2,16);
    c.gaussianKernel=odd("threshold.gaussian_kernel",c.gaussianKernel,1,31);
    c.adaptiveBlockSize=odd("threshold.block_size",c.adaptiveBlockSize,3,255);
    c.adaptiveC=number("threshold.c",c.adaptiveC,-255,255);
    c.diagnosticEnabled=integer("diagnostics.enabled",c.diagnosticEnabled,0,1);
    c.denoiseEnabled=integer("denoise.enabled",c.denoiseEnabled,0,1);
    c.denoiseMaxComponentPixels=integer("denoise.max_component_pixels",c.denoiseMaxComponentPixels,0,10000);
    c.denoiseNeighborRadius=integer("denoise.neighbor_radius",c.denoiseNeighborRadius,0,500);
    c.denoiseAnchorMinPixels=integer("denoise.anchor_min_pixels",c.denoiseAnchorMinPixels,1,100000);
    c.denoiseDensityEnabled=integer("denoise.density_enabled",c.denoiseDensityEnabled,0,1);
    c.denoiseDensityWindow=odd("denoise.density_window",c.denoiseDensityWindow,3,501);
    c.denoiseLowDensityThreshold=number("denoise.low_density_threshold",c.denoiseLowDensityThreshold,0,1);
    c.denoiseSparseMaxPixels=integer("denoise.sparse_max_pixels",c.denoiseSparseMaxPixels,0,10000);
    // The sparse cap is meaningful only when density adaptation is enabled.
    // In conservative-only mode it must not constrain normal denoising.
    if(c.denoiseDensityEnabled==1 &&
       c.denoiseSparseMaxPixels < c.denoiseMaxComponentPixels)
        throw std::runtime_error("denoise.sparse_max_pixels must be >= denoise.max_component_pixels when density_enabled=1");
    c.bridgeEnabled=integer("repair.enabled",c.bridgeEnabled,0,1);
    c.bridgeKernel=odd("repair.kernel_size",c.bridgeKernel,3,7);
    c.bridgeIterations=integer("repair.iterations",c.bridgeIterations,1,3);
    c.bridgeShape=choice("repair.kernel_shape",c.bridgeShape,{"CROSS","RECT","ELLIPSE"});
    c.strokeMode=choice("reconstruction.mode",c.strokeMode,{"GRAPH","CONTOUR"});
    c.thinMethod=choice("reconstruction.thinning",c.thinMethod,{"ZHANG_SUEN","MORPH"});
    c.graphGapEnabled=integer("reconstruction.gap_enabled",c.graphGapEnabled,0,1);
    c.graphMaxGap=integer("reconstruction.max_gap",c.graphMaxGap,0,50);
    c.graphDirectionWindow=integer("reconstruction.direction_window",c.graphDirectionWindow,2,40);
    c.graphSmoothAngle=number("reconstruction.smooth_angle",c.graphSmoothAngle,0,180);
    c.graphCornerEnabled=integer("reconstruction.corner_enabled",c.graphCornerEnabled,0,1);
    c.graphCornerMaxGap=integer("reconstruction.corner_max_gap",c.graphCornerMaxGap,0,50);
    c.graphCornerAngle=number("reconstruction.corner_angle",c.graphCornerAngle,0,180);
    c.graphJunctionPairing=integer("reconstruction.junction_pairing",c.graphJunctionPairing,0,1);
    c.graphJunctionAngle=number("reconstruction.junction_angle",c.graphJunctionAngle,0,90);
    c.graphMinPoints=integer("reconstruction.min_points",c.graphMinPoints,2,1000);
    c.graphGuardEnabled=integer("reconstruction.character_guard",c.graphGuardEnabled,0,1);
    c.graphMaxGapRatio=number("reconstruction.max_gap_ratio",c.graphMaxGapRatio,0.01,3);
    c.edgeLinesEnabled=integer("edge_lines.enabled",c.edgeLinesEnabled,0,1);
    c.edgeLinesMargin=integer("edge_lines.edge_margin",c.edgeLinesMargin,0,10000);
    c.edgeLinesMinSegments=integer("edge_lines.min_segments",c.edgeLinesMinSegments,2,1000);
    c.edgeLinesSingleEnabled=integer("edge_lines.single_enabled",c.edgeLinesSingleEnabled,0,1);
    c.edgeLinesSingleMinLength=number("edge_lines.single_min_length",c.edgeLinesSingleMinLength,1,100000);
    c.edgeLinesMinTotalLength=number("edge_lines.min_total_length",c.edgeLinesMinTotalLength,1,100000);
    c.edgeLinesMinSpan=number("edge_lines.min_span",c.edgeLinesMinSpan,1,100000);
    c.edgeLinesMaxGap=number("edge_lines.max_gap",c.edgeLinesMaxGap,0,10000);
    c.edgeLinesMaxOffset=number("edge_lines.max_offset",c.edgeLinesMaxOffset,0,1000);
    c.edgeLinesMaxAngle=number("edge_lines.max_angle",c.edgeLinesMaxAngle,0,45);
    c.edgeLinesMaxDeviation=number("edge_lines.max_deviation",c.edgeLinesMaxDeviation,0,100);
    c.edgeLinesMinSegmentPoints=integer("edge_lines.min_segment_points",c.edgeLinesMinSegmentPoints,2,10000);
    c.skeletonKernel=odd("skeleton.kernel_size",c.skeletonKernel,3,15);
    c.skeletonShape=choice("skeleton.kernel_shape",c.skeletonShape,{"CROSS","RECT","ELLIPSE"});
    c.retrievalMode=choice("contours.retrieval_mode",c.retrievalMode,{"RETR_LIST","RETR_EXTERNAL","RETR_TREE","RETR_CCOMP"});
    c.chainMode=choice("contours.approximation_mode",c.chainMode,{"CHAIN_APPROX_NONE","CHAIN_APPROX_SIMPLE"});
    // The Lua side owns [output], and can supply extra keys without breaking C++.
    for(const auto& entry:entries)if(entry.first.rfind("output.",0)!=0 && entry.first!="image.polarity_mode")
        std::cerr<<"Inkpath: ignored unknown config key: "<<entry.first<<'\n';
    return c;
}
}

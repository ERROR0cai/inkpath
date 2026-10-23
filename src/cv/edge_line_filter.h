#pragma once
#include "ipcv.h"
#include <algorithm>
#include <cmath>
#include <numeric>
#include <vector>

namespace inkp {
struct EdgeLineStats {
    int candidatePaths=0;
    int removedGroups=0;
    int removedSingles=0;
    int removedPaths=0;
};
namespace edge_line_detail {
struct Info {
    int index=-1;
    cv::Point2d center, dir;
    cv::Point2d p0,p1;
    double length=0, tmin=0, tmax=0;
};
inline double dot(cv::Point2d a,cv::Point2d b) {return a.x*b.x+a.y*b.y;}
inline cv::Point2d sub(cv::Point2d a,cv::Point2d b) {return {a.x-b.x,a.y-b.y};}
inline double cross(cv::Point2d a,cv::Point2d b) {return a.x*b.y-a.y*b.x;}
inline double distance(cv::Point2d a,cv::Point2d b) {return cv::norm(sub(a,b));}
inline double angleDeg(cv::Point2d a,cv::Point2d b) {
    double z=std::min(1.0,std::abs(dot(a,b)));
    return std::acos(z)*180.0/3.14159265358979323846;
}
inline int boundaryDistance(cv::Point p,int width,int height) {
    return std::min(std::min(p.x,width-1-p.x),std::min(p.y,height-1-p.y));
}
// For a fragmented line, treat a path as an approximately straight segment.
// Reject curves and zigzags before attempting any grouping.
inline bool fit(const std::vector<cv::Point>& path,int id,int w,int h,
                const InkpathConfig& cfg,Info& result) {
    if(int(path.size())<cfg.edgeLinesMinSegmentPoints)return false;
    const cv::Point2d first(path.front().x,path.front().y);
    const cv::Point2d last(path.back().x,path.back().y);
    cv::Point2d axis=sub(last,first);
    double chord=cv::norm(axis);
    if(chord<1.0)return false;
    axis.x/=chord;axis.y/=chord;
    double minProj=0,maxProj=chord,arc=0,maxPerp=0;
    for(size_t i=0;i<path.size();++i){
        const auto& q=path[i];
        if(cfg.edgeLinesMargin>0 && boundaryDistance(q,w,h)>cfg.edgeLinesMargin)return false;
        cv::Point2d v(q.x-first.x,q.y-first.y);
        minProj=std::min(minProj,dot(v,axis));
        maxProj=std::max(maxProj,dot(v,axis));
        maxPerp=std::max(maxPerp,std::abs(cross(v,axis)));
        if(i)arc+=cv::norm(path[i]-path[i-1]);
    }
    if(maxPerp>cfg.edgeLinesMaxDeviation)return false;
    // An almost-straight path cannot double back repeatedly.
    if(arc>1.25*std::max(1.0,maxProj-minProj))return false;
    result.index=id; result.center={(first.x+last.x)/2,(first.y+last.y)/2};
    result.dir=axis;result.p0=first;result.p1=last;
    result.length=arc;result.tmin=minProj;result.tmax=maxProj;
    return true;
}
inline bool compatible(const Info& a,const Info& b,const InkpathConfig& c) {
    if(angleDeg(a.dir,b.dir)>c.edgeLinesMaxAngle)return false;
    // Both endpoints of B must be close to A's infinite support line.
    if(std::abs(cross(sub(b.p0,a.p0),a.dir))>c.edgeLinesMaxOffset ||
       std::abs(cross(sub(b.p1,a.p0),a.dir))>c.edgeLinesMaxOffset)return false;
    // Use projected interval gap, not midpoint-to-midpoint distance:
    // short and long fragments can be connected on the same straight line.
    double b0=dot(sub(b.p0,a.p0),a.dir),b1=dot(sub(b.p1,a.p0),a.dir);
    if(b0>b1)std::swap(b0,b1);
    const double a0=a.tmin,a1=a.tmax;
    double gap=std::max({0.0,b0-a1,a0-b1});
    return gap<=c.edgeLinesMaxGap;
}
} // namespace edge_line_detail

inline std::vector<std::vector<cv::Point>> removeCollinearEdgeGroups(
    const std::vector<std::vector<cv::Point>>& paths,int width,int height,
    const InkpathConfig& cfg,EdgeLineStats* stats=nullptr,
    cv::Mat* removedMask=nullptr) {
    if(stats)*stats=EdgeLineStats{};
    if(removedMask)*removedMask=cv::Mat::zeros(height,width,CV_8UC1);
    if(!cfg.edgeLinesEnabled || width<=0 || height<=0)return paths;
    using namespace edge_line_detail;
    std::vector<Info> infos;
    for(int i=0;i<int(paths.size());++i){
        Info info;
        if(fit(paths[i],i,width,height,cfg,info))infos.push_back(info);
    }
    if(stats)stats->candidatePaths=int(infos.size());
    const int n=int(infos.size());
    // Independently evaluate grouped fragments and individual long strokes.
    // The thresholds must not affect each other's candidate sets.
    std::vector<unsigned char> groupRemove(paths.size(),0);
    std::vector<unsigned char> singleRemove(paths.size(),0);

    // Do NOT union all pairwise-compatible segments transitively. That can merge
    // two distinct parallel lines through one bad bridge and poison the whole group.
    // Instead, take each eligible segment as a fixed supporting-line hypothesis,
    // collect only fragments aligned to that SAME line, order by projection,
    // split into contiguous runs, and evaluate each run independently.
    struct Interval { int id; double begin,end; };
    for(int anchor=0;anchor<n;++anchor) {
        const Info& base=infos[anchor];
        std::vector<Interval> intervals;
        intervals.reserve(n);
        for(int j=0;j<n;++j) {
            const Info& v=infos[j];
            if(angleDeg(base.dir,v.dir)>cfg.edgeLinesMaxAngle)continue;
            const cv::Point2d d0=sub(v.p0,base.p0), d1=sub(v.p1,base.p0);
            if(std::abs(cross(d0,base.dir))>cfg.edgeLinesMaxOffset ||
               std::abs(cross(d1,base.dir))>cfg.edgeLinesMaxOffset)continue;
            double t0=dot(d0,base.dir),t1=dot(d1,base.dir);
            if(t0>t1)std::swap(t0,t1);
            intervals.push_back({j,t0,t1});
        }
        std::sort(intervals.begin(),intervals.end(),[](const Interval& a,const Interval& b){
            return a.begin<b.begin || (a.begin==b.begin && a.end<b.end);
        });
        // A run is maximal with respect to its maximum covered endpoint.
        // A distant unrelated member cannot invalidate earlier eligible runs.
        auto testRun=[&](size_t begin,size_t end) {
            if(end-begin<size_t(cfg.edgeLinesMinSegments))return;
            double total=0.0, low=intervals[begin].begin, high=intervals[begin].end;
            for(size_t k=begin;k<end;++k) {
                total+=infos[intervals[k].id].length;
                low=std::min(low,intervals[k].begin);
                high=std::max(high,intervals[k].end);
            }
            if(total<cfg.edgeLinesMinTotalLength || high-low<cfg.edgeLinesMinSpan)return;
            int newlyRemoved=0;
            for(size_t k=begin;k<end;++k){
                const int original=infos[intervals[k].id].index;
                if(!groupRemove[original]) {
                    groupRemove[original]=1;
                    ++newlyRemoved;
                }
            }
            if(newlyRemoved && stats)++stats->removedGroups;
        };
        if(intervals.empty())continue;
        size_t runBegin=0;
        double coveredEnd=intervals[0].end;
        for(size_t k=1;k<intervals.size();++k){
            if(intervals[k].begin-coveredEnd>cfg.edgeLinesMaxGap){
                testRun(runBegin,k);
                runBegin=k;
                coveredEnd=intervals[k].end;
            } else coveredEnd=std::max(coveredEnd,intervals[k].end);
        }
        testRun(runBegin,intervals.size());
    }
    // Evaluate single-line threshold against the ORIGINAL candidate list,
    // without removing a fragment before group recognition.
    if(cfg.edgeLinesSingleEnabled) {
        for(const auto& info:infos) {
            if(info.length>cfg.edgeLinesSingleMinLength) {
                singleRemove[info.index]=1;
                if(stats)++stats->removedSingles;
            }
        }
    }

    std::vector<std::vector<cv::Point>> kept;kept.reserve(paths.size());
    for(size_t i=0;i<paths.size();++i) {
        const bool drop=groupRemove[i] || singleRemove[i];
        if(!drop) { kept.push_back(paths[i]); continue; }
        if(stats)++stats->removedPaths; // union count; no double counting
        if(removedMask)for(const auto& q:paths[i])
            if(q.x>=0&&q.x<width&&q.y>=0&&q.y<height)
                removedMask->at<uchar>(q)=255;
    }
    return kept;
}
} // namespace inkp

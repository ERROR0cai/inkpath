#include "ipcv.h"
#include <opencv2/imgproc.hpp>
#include <algorithm>
#include <cstdint>
#include <cmath>
#include <limits>
#include <numeric>
#include <set>
#include <unordered_map>
#include <unordered_set>

namespace inkp {
namespace {
using Path = std::vector<cv::Point>;
constexpr double PI = 3.14159265358979323846;
int idx(int x,int y,int w) {return y*w+x;}
double dist(cv::Point a,cv::Point b){return cv::norm(a-b);}
cv::Point2d diff(cv::Point a,cv::Point b) {return cv::Point2d(double(a.x-b.x),double(a.y-b.y));}
double angle(cv::Point2d a,cv::Point2d b) {
    double na=cv::norm(a),nb=cv::norm(b);
    if(na<1e-6 || nb<1e-6)return 180.0;
    double v=(a.x*b.x+a.y*b.y)/(na*nb);
    return std::acos(std::clamp(v,-1.0,1.0))*180.0/PI;
}
uint64_t edgeKey(int a,int b){if(a>b)std::swap(a,b);return (uint64_t(uint32_t(a))<<32)|uint32_t(b);}
struct Seg {Path p; bool active=true;};
}

// Topology-preserving 2-subiteration thinning. Input/output: white background, black foreground.
cv::Mat thinZhangSuen(const cv::Mat& image) {
    if(image.empty()||image.type()!=CV_8UC1)throw std::runtime_error("thinning requires grayscale image");
    cv::Mat im=(image==0); // 0 or 255
    cv::Mat mark=cv::Mat::zeros(im.size(),CV_8UC1);
    bool changed;
    do {
        changed=false;
        for(int phase=0;phase<2;++phase){
            mark.setTo(0);
            for(int y=1;y<im.rows-1;++y)for(int x=1;x<im.cols-1;++x){
                if(!im.at<uchar>(y,x))continue;
                int p[8]={im.at<uchar>(y-1,x)!=0,im.at<uchar>(y-1,x+1)!=0,
                          im.at<uchar>(y,x+1)!=0,im.at<uchar>(y+1,x+1)!=0,
                          im.at<uchar>(y+1,x)!=0,im.at<uchar>(y+1,x-1)!=0,
                          im.at<uchar>(y,x-1)!=0,im.at<uchar>(y-1,x-1)!=0};
                int n=std::accumulate(p,p+8,0), transitions=0;
                for(int k=0;k<8;++k)if(p[k]==0&&p[(k+1)%8]==1)++transitions;
                if(n<2||n>6||transitions!=1)continue;
                if(phase==0){if(p[0]*p[2]*p[4]||p[2]*p[4]*p[6])continue;}
                else {if(p[0]*p[2]*p[6]||p[0]*p[4]*p[6])continue;}
                mark.at<uchar>(y,x)=255;
            }
            if(cv::countNonZero(mark)){im.setTo(0,mark);changed=true;}
        }
    }while(changed);
    // Border pixels remain untouched by iteration, preserving strokes at image edges.
    cv::Mat out;cv::bitwise_not(im,out);return out;
}

std::vector<std::vector<cv::Point>> reconstructStrokes(
    const cv::Mat& blackOnWhite,const InkpathConfig& cfg,
    ReconstructionStats* stats,cv::Mat* addedMask) {
    if(stats)*stats=ReconstructionStats{};
    if(addedMask)*addedMask=cv::Mat::zeros(blackOnWhite.size(),CV_8UC1);
    if(blackOnWhite.empty()||blackOnWhite.type()!=CV_8UC1)
        throw std::runtime_error("graph reconstruction requires grayscale skeleton");
    const int w=blackOnWhite.cols,h=blackOnWhite.rows;
    std::vector<int> vertices;vertices.reserve(w*h/25);
    std::vector<int> map(size_t(w)*h,-1);
    for(int y=0;y<h;++y){const uchar* row=blackOnWhite.ptr<uchar>(y);
        for(int x=0;x<w;++x)if(row[x]<128){int id=int(vertices.size());vertices.push_back(idx(x,y,w));map[idx(x,y,w)]=id;}}
    const int n=int(vertices.size());if(!n)return {};
    std::vector<cv::Point> xy(n);std::vector<std::vector<int>> adj(n);
    for(int i=0;i<n;++i){int k=vertices[i];xy[i]=cv::Point(k%w,k/w);}
    static const int dx[8]={1,1,0,-1,-1,-1,0,1},dy[8]={0,1,1,1,0,-1,-1,-1};
    for(int i=0;i<n;++i){auto p=xy[i];for(int t=0;t<8;++t){int xx=p.x+dx[t],yy=p.y+dy[t];
        if(xx<0||yy<0||xx>=w||yy>=h)continue;
        int v=map[idx(xx,yy,w)];if(v<=i)continue;
        // Avoid the diagonal shortcut across an L-shaped three-pixel corner.
        if(dx[t]&&dy[t]){
            int a=map[idx(p.x+dx[t],p.y,w)];int b=map[idx(p.x,p.y+dy[t],w)];
            if(a>=0||b>=0)continue;
        }
        adj[i].push_back(v);adj[v].push_back(i);
    }}
    std::unordered_set<uint64_t> used;used.reserve(size_t(n)*3);
    std::vector<Path> parts;parts.reserve(n/4+1);
    auto trace=[&](int start,int next){
        Path path;path.push_back(xy[start]);
        int prev=start,curr=next;
        used.insert(edgeKey(prev,curr));path.push_back(xy[curr]);
        while(adj[curr].size()==2){
            int following=(adj[curr][0]==prev?adj[curr][1]:adj[curr][0]);
            auto key=edgeKey(curr,following);if(used.count(key))break;
            used.insert(key);prev=curr;curr=following;path.push_back(xy[curr]);
            if(curr==start)break;
        }
        return path;
    };
    for(int i=0;i<n;++i)if(adj[i].size()!=2){
        if(adj[i].empty())continue; // isolated pixel cannot form an Xournal++ stroke
        for(int j:adj[i])if(!used.count(edgeKey(i,j)))parts.push_back(trace(i,j));
    }
    for(int i=0;i<n;++i)for(int j:adj[i])if(!used.count(edgeKey(i,j)))parts.push_back(trace(i,j));
    if(stats)stats->initialSegments=int(parts.size());
    // Rejoin graph edges at 3/4-way junctions by minimizing turning angle.
    // Every segment endpoint may be used once; this preserves separate objects at intersections.
    std::vector<Seg> segs;segs.reserve(parts.size());for(auto& p:parts)if(p.size()>1)segs.push_back({std::move(p),true});
    auto tangent=[&](const Path& p,bool atEnd){
        const int k=std::min<int>(cfg.graphDirectionWindow,int(p.size())-1);
        return atEnd ? diff(p.back(),p[p.size()-1-k]) : diff(p.front(),p[k]);
    }; // outward tangent from segment interior at its endpoint
    // Junction continuation is only considered if both ends occupy exactly the same graph node.
    if(cfg.graphJunctionPairing){
        for(int round=0;round<4;++round){
            struct Candidate{int a,b,ea,eb;double score;};
            std::vector<Candidate> candidates;
            std::unordered_map<int,std::vector<std::pair<int,int>>> ending;
            for(int i=0;i<int(segs.size());++i)if(segs[i].active){
                ending[idx(segs[i].p.front().x,segs[i].p.front().y,w)].push_back({i,0});
                ending[idx(segs[i].p.back().x,segs[i].p.back().y,w)].push_back({i,1});
            }
            for(auto& [pixel,ends]:ending)if(ends.size()>=2){
                for(size_t i=0;i<ends.size();++i)for(size_t j=i+1;j<ends.size();++j){
                    auto [a,ea]=ends[i];auto [b,eb]=ends[j];if(a==b)continue;
                    auto t1=tangent(segs[a].p,ea==1),t2=tangent(segs[b].p,eb==1);
                    double deviation=180.0-angle(t1,t2);
                    if(deviation<=cfg.graphJunctionAngle)candidates.push_back({a,b,ea,eb,deviation});
                }
            }
            std::sort(candidates.begin(),candidates.end(),[](auto& a,auto& b){return a.score<b.score;});
            std::set<int> taken;bool any=false;
            for(auto c:candidates){if(taken.count(c.a)||taken.count(c.b)||!segs[c.a].active||!segs[c.b].active)continue;
                Path a=segs[c.a].p,b=segs[c.b].p;
                if(c.ea==0)std::reverse(a.begin(),a.end());
                if(c.eb==1)std::reverse(b.begin(),b.end());
                if(a.back()!=b.front())continue;
                a.insert(a.end(),b.begin()+1,b.end());
                segs[c.a].p=std::move(a);segs[c.b].active=false;
                taken.insert(c.a);taken.insert(c.b);any=true;
                if(stats)++stats->junctionPairs;
            }
            if(!any)break;
        }
    }
    // Gap endpoint matching, separate from graph junction pairing.
    // Strong geometric and local-scale constraints; no OCR-based stroke count is claimed.
    if(cfg.graphGapEnabled && cfg.graphMaxGap>0){
        struct End {int segment,end;cv::Point pos;};
        std::vector<End> ends;
        for(int i=0;i<int(segs.size());++i)if(segs[i].active){
            if(segs[i].p.front()==segs[i].p.back())continue;
            ends.push_back({i,0,segs[i].p.front()});ends.push_back({i,1,segs[i].p.back()});
        }
        // Bucket search to avoid quadratic comparisons on large pages.
        const int step=std::max(1,cfg.graphMaxGap);
        std::unordered_map<uint64_t,std::vector<int>> buckets;
        auto key=[](int bx,int by){return (uint64_t(uint32_t(bx))<<32)|uint32_t(by);};
        for(int i=0;i<int(ends.size());++i)buckets[key(ends[i].pos.x/step,ends[i].pos.y/step)].push_back(i);
        struct Candidate{int i,j;double score;};std::vector<Candidate> options;
        for(int i=0;i<int(ends.size());++i){auto e=ends[i];int bx=e.pos.x/step,by=e.pos.y/step;
            for(int oy=-1;oy<=1;++oy)for(int ox=-1;ox<=1;++ox){auto found=buckets.find(key(bx+ox,by+oy));if(found==buckets.end())continue;
                for(int j:found->second){if(j<=i||ends[j].segment==e.segment)continue;
                    auto f=ends[j];double d=dist(e.pos,f.pos);if(d<1.01||d>cfg.graphMaxGap)continue;
                    auto ta=tangent(segs[e.segment].p,e.end==1);
                    auto tb=tangent(segs[f.segment].p,f.end==1);
                    auto toward=diff(f.pos,e.pos);
                    double aa=angle(ta,toward),bb=angle(tb,-toward);
                    // Endpoint outward direction must point toward the opposite endpoint.
                    double deviation=std::max(aa,bb);
                    bool smooth=deviation<=cfg.graphSmoothAngle;
                    bool corner=cfg.graphCornerEnabled&&d<=cfg.graphCornerMaxGap&&
                                deviation<=cfg.graphCornerAngle;
                    if(!smooth&&!corner)continue;
                    if(cfg.graphGuardEnabled){
                        auto& a=segs[e.segment].p;auto& b=segs[f.segment].p;
                        // Compute the segment bounding-box diagonal directly.
                        // This avoids cv::boundingRect, which is unavailable in
                        // some OpenCV 5 builds while preserving the scale guard.
                        auto boxDiagonal=[](const Path& path)->double {
                            int minX=path.front().x, maxX=minX;
                            int minY=path.front().y, maxY=minY;
                            for(const auto& p:path){
                                minX=std::min(minX,p.x);maxX=std::max(maxX,p.x);
                                minY=std::min(minY,p.y);maxY=std::max(maxY,p.y);
                            }
                            return std::hypot(double(maxX-minX+1),double(maxY-minY+1));
                        };
                        double scale=std::max(4.0,std::min(boxDiagonal(a),boxDiagonal(b)));
                        if(d>cfg.graphMaxGapRatio*scale)continue;
                    }
                    // Do not draw a new bridge across pre-existing strokes.
                    cv::LineIterator it(blackOnWhite,e.pos,f.pos,8);
                    bool crosses=false;for(int k=1;k<it.count-1;++k){++it;auto q=it.pos();
                        if(blackOnWhite.at<uchar>(q)<128){crosses=true;break;}}
                    if(crosses)continue;
                    options.push_back({i,j,d+deviation*0.1+(smooth?0:10)});
                }
            }
        }
        std::sort(options.begin(),options.end(),[](auto&a,auto&b){return a.score<b.score;});
        std::set<int> taken;
        for(auto c:options){auto a=ends[c.i],b=ends[c.j];
            if(taken.count(a.segment)||taken.count(b.segment)||!segs[a.segment].active||!segs[b.segment].active)continue;
            Path one=segs[a.segment].p,two=segs[b.segment].p;
            if(a.end==0)std::reverse(one.begin(),one.end());
            if(b.end==1)std::reverse(two.begin(),two.end());
            // Insert short bridge line pixels and store exact coordinates for editable output.
            cv::LineIterator iter(blackOnWhite,one.back(),two.front(),8);
            for(int k=1;k<iter.count-1;++k){++iter;auto q=iter.pos();one.push_back(q);
                if(addedMask)addedMask->at<uchar>(q)=255;
            }
            one.insert(one.end(),two.begin(),two.end());
            segs[a.segment].p=std::move(one);segs[b.segment].active=false;
            taken.insert(a.segment);taken.insert(b.segment);
            if(stats)++stats->gapJoins;
        }
    }
    std::vector<Path> result;result.reserve(segs.size());
    for(auto& s:segs)if(s.active&&int(s.p.size())>=cfg.graphMinPoints)result.push_back(std::move(s.p));
    if(stats)stats->outputPaths=int(result.size());
    return result;
}
} // namespace inkp

// SPDX-License-Identifier: GPL-3.0-only
#include "ScrollFrameAligner.h"
#include <QColorSpace>
#include <array>
#include <algorithm>
#include <cmath>
namespace NkShot::Imaging {
namespace {
constexpr int tolerance=8;
constexpr double acceptance=.94, minimumGap=.08;
struct Cell {int anchors=0, matched=0;};
struct Score {double value=-1;};
// Logical x crosses the scroll axis; logical y follows it. No image transpose.
struct AxisView {
    const QImage &image;
    bool horizontal;
    int width() const { return horizontal ? image.height() : image.width(); }
    int height() const { return horizontal ? image.width() : image.height(); }
};
const uchar *pixel(const AxisView &view,int x,int y){return view.horizontal ? view.image.constScanLine(x)+4*y : view.image.constScanLine(y)+4*x;}
bool matches(const uchar *a,const uchar *b){for(int c=0;c<4;++c)if(std::abs(int(a[c])-int(b[c]))>tolerance)return false;return true;}
bool textured(const AxisView &image,int x,int y) {
    const auto center=pixel(image,x,y);if(center[3]<248)return false;
    const auto horizontal=pixel(image,x+1<image.width()?x+1:x-1,y);
    const auto vertical=pixel(image,x,y+1<image.height()?y+1:y-1);
    for(int c=0;c<3;++c)if(std::abs(int(center[c])-int(horizontal[c]))>=12||std::abs(int(center[c])-int(vertical[c]))>=12)return true;
    return false;
}
bool stationaryHeader(const AxisView &a,const AxisView &b) {
    int supportedRows=0,minX=a.width(),maxX=-1;
    for(int y=0;y<std::max(4,a.height()/4);++y) {
        int matching=0,anchors=0,allAnchors=0,rowMin=a.width(),rowMax=-1;
        for(int x=0;x<a.width();++x) {
            const bool anchor=textured(a,x,y);if(anchor)++allAnchors;
            if(matches(pixel(a,x,y),pixel(b,x,y))) {
                ++matching;
                bool sameStructure=anchor;
                // A matching white pixel adjacent to an edge is not itself a
                // stationary feature. Compare its local texture neighborhood.
                if(anchor) for(int dy=-1;dy<=1&&sameStructure;++dy) for(int dx=-1;dx<=1;++dx) {
                    const int px=std::clamp(x+dx,0,a.width()-1),py=std::clamp(y+dy,0,a.height()-1);
                    if(!matches(pixel(a,px,py),pixel(b,px,py))){sameStructure=false;break;}
                }
                if(sameStructure){++anchors;rowMin=std::min(rowMin,x);rowMax=std::max(rowMax,x);}
            }
        }
        if(double(matching)/a.width()>=.97&&allAnchors>0&&double(anchors)/allAnchors>=.97&&anchors>=std::max(8,a.width()/32)) {
            ++supportedRows;minX=std::min(minX,rowMin);maxX=std::max(maxX,rowMax);
        }
    }
    return supportedRows>=3&&maxX-minX>=std::max(8,a.width()/20);
}
Score score(const AxisView &previous,const AxisView &next,int displacement,bool exhaustive) {
    const int overlap=previous.height()-std::abs(displacement);
    const int py=std::max(displacement,0),ny=std::max(-displacement,0);
    const int columns=exhaustive?previous.width():std::min(previous.width(),48);
    const int rows=exhaustive?overlap:std::min(overlap,24);
    qint64 total=0, matched=0, anchors=0, anchorMatched=0;std::array<Cell,48*24> footprint{};
    int minColumn=48,maxColumn=-1,minRow=24,maxRow=-1;
    for(int row=0;row<rows;++row) {
        const int y=exhaustive?row:int((qint64(2*row+1)*overlap)/(2*rows));
        for(int column=0;column<columns;++column) {
            const int x=exhaustive?column:int((qint64(2*column+1)*previous.width())/(2*columns));
            const bool equal=matches(pixel(previous,x,py+y),pixel(next,x,ny+y));++total;if(equal)++matched;
            if(textured(previous,x,py+y)) {
                ++anchors;if(equal)++anchorMatched;
                const int columnBin=std::min(47,x*48/previous.width()),rowBin=std::min(23,y*24/overlap);
                auto &cell=footprint[rowBin*48+columnBin];++cell.anchors;if(equal)++cell.matched;
                minColumn=std::min(minColumn,columnBin);maxColumn=std::max(maxColumn,columnBin);minRow=std::min(minRow,rowBin);maxRow=std::max(maxRow,rowBin);
            }
        }
    }
    if(anchors<24||maxColumn-minColumn<2||maxRow-minRow<5)return {};
    // Normalize spatial support to occupied texture, not the surrounding whitespace.
    // Still require at least three horizontal bins and six vertical bins of extent.
    std::array<Cell,9> cells{};
    for(int y=minRow;y<=maxRow;++y)for(int x=minColumn;x<=maxColumn;++x) {
        const auto &source=footprint[y*48+x];
        auto &target=cells[std::min(2,(y-minRow)*3/(maxRow-minRow+1))*3+std::min(2,(x-minColumn)*3/(maxColumn-minColumn+1))];
        target.anchors+=source.anchors;target.matched+=source.matched;
    }
    int covered=0;std::array<int,3> bands{},lanes{};
    for(int i=0;i<9;++i)if(cells[i].anchors>=4&&double(cells[i].matched)/cells[i].anchors>=.85){++covered;++bands[i/3];++lanes[i%3];}
    if(covered<6||std::any_of(bands.begin(),bands.end(),[](int n){return n==0;})||std::any_of(lanes.begin(),lanes.end(),[](int n){return n==0;}))return {};
    return {std::min(double(matched)/total,double(anchorMatched)/anchors)};
}
}
ScrollAlignment alignScrollFrames(const QImage &previous,const QImage &next,int minimumOverlap,ScrollDirection direction) {
    if(direction!=ScrollDirection::Vertical&&direction!=ScrollDirection::Horizontal)return {StitchError::InvalidInput};
    const bool horizontal=direction==ScrollDirection::Horizontal;
    const AxisView geometry{previous,horizontal};
    if(previous.isNull()||next.isNull()||minimumOverlap<24||geometry.width()<8||geometry.height()<=minimumOverlap)return {StitchError::InvalidInput};
    if(previous.size()!=next.size()||previous.devicePixelRatio()!=next.devicePixelRatio()||previous.colorSpace()!=next.colorSpace())return {StitchError::IncompatibleFrames};
    if(previous.width()>8192||previous.height()>8192||qint64(previous.width())*previous.height()>16000000)return {StitchError::TooLarge};
    const auto first=previous.convertToFormat(QImage::Format_RGBA8888),secondImage=next.convertToFormat(QImage::Format_RGBA8888);
    if(first.isNull()||secondImage.isNull())return {StitchError::TooLarge};
    const AxisView a{first,horizontal},b{secondImage,horizontal};
    double best=-1,second=-1;int bestDisplacement=0;
    QVector<QPair<int,double>> candidates;
    const int maximum=a.height()-minimumOverlap;
    for(int displacement=-maximum;displacement<=maximum;++displacement) {
        const auto candidate=score(a,b,displacement,false).value;
        if(candidate>=0)candidates.append({displacement,candidate});
        if(candidate>best){second=best;best=candidate;bestDisplacement=displacement;}else if(candidate>second)second=candidate;
    }
    if(best<acceptance)return {StitchError::NoOverlap,0,std::max(0.,best),0};
    double gap=best-std::max(0.,second);
    bool fullyScored=false;
    if(gap<minimumGap) {
        // Regular glyph grids can alias the bounded search lattice. Re-evaluate
        // only a small near-tie set with every overlap pixel, never relax the gap.
        QVector<int> near; qint64 work=0;
        for(const auto &candidate:candidates)if(candidate.second>=best-minimumGap) {
            near.append(candidate.first);work+=qint64(a.width())*(a.height()-std::abs(candidate.first));
        }
        if(near.size()>8||work>32000000)return {StitchError::AmbiguousOverlap,0,best,gap};
        best=second=-1;
        for(int displacement:near) {
            const double value=score(a,b,displacement,true).value;
            if(value>best){second=best;best=value;bestDisplacement=displacement;}else if(value>second)second=value;
        }
        fullyScored=true;gap=best-std::max(0.,second);
        if(best<acceptance)return {StitchError::NoOverlap,0,std::max(0.,best),gap};
        if(gap<minimumGap)return {StitchError::AmbiguousOverlap,0,best,gap};
    }
    if(bestDisplacement==0)return {StitchError::NoProgress,0,best,gap};
    if(bestDisplacement<0)return {StitchError::NoOverlap,0,best,gap};
    if(stationaryHeader(a,b))return {StitchError::NoOverlap,0,best,gap};
    const auto full=fullyScored?best:score(a,b,bestDisplacement,true).value;
    if(full<acceptance)return {StitchError::NoOverlap,0,std::max(0.,full),gap};
    return {StitchError::None,a.height()-bestDisplacement,full,gap};
}

} // namespace NkShot::Imaging

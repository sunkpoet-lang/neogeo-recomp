// SPDX-License-Identifier: GPL-3.0-only
// Actual recorder-rendered controls; provenance in assets/recorder-inputs.
#include "neogeo_input_display.h"
#include "recorder_input_art.h"
#include "../recomp-common/text.h"
#include <algorithm>
#include <array>
#include <vector>
#include <zlib.h>
namespace port {
namespace {
const auto& Art(){
    static const auto art=[](){std::array<std::vector<uint8_t>,17> images;
        for(size_t i=0;i<images.size();++i){const auto& e=recorder_art::entries[i];auto& raw=images[i];
            if(e.width<=0||e.height<=0||e.width>256||e.height>256||e.offset<0||e.bytes<0||size_t(e.offset+e.bytes)>sizeof(recorder_art::packed))continue;
            raw.resize(size_t(e.width)*e.height*4);uLongf size=raw.size();
            if(uncompress(raw.data(),&size,recorder_art::packed+e.offset,e.bytes)!=Z_OK||size!=raw.size())raw.clear();
        }return images;}();return art;
}
void Sprite(uint32_t* pixels,int width,int height,int which,int cx,int cy,int size,int left,int top,int right,int bottom){
    const auto& raw=Art()[which];if(raw.empty())return;const auto& e=recorder_art::entries[which];
    const int x=cx-size/2,y=cy-size/2;
    for(int dy=0;dy<size;++dy)for(int dx=0;dx<size;++dx){
        const int tx=x+dx,ty=y+dy;if(tx<left||tx>=right||ty<top||ty>=bottom||tx<0||tx>=width||ty<0||ty>=height)continue;
        const int sx=dx*(e.width-1)*256/std::max(size-1,1),sy=dy*(e.height-1)*256/std::max(size-1,1);
        const int x0=sx/256,y0=sy/256,x1=std::min(x0+1,e.width-1),y1=std::min(y0+1,e.height-1),fx=sx%256,fy=sy%256;
        unsigned rgba[4]={};
        for(int c=0;c<4;++c)rgba[c]=(raw[(size_t(y0)*e.width+x0)*4+c]*(256-fx)*(256-fy)+raw[(size_t(y0)*e.width+x1)*4+c]*fx*(256-fy)+raw[(size_t(y1)*e.width+x0)*4+c]*(256-fx)*fy+raw[(size_t(y1)*e.width+x1)*4+c]*fx*fy+32768)/65536;
        auto& target=pixels[size_t(ty)*width+tx];uint32_t value=0xff000000;
        for(int c=0;c<3;++c)value|=((rgba[c]*rgba[3]+((target>>(8*c))&255)*(255-rgba[3])+127)/255)<<(8*c);
        target=value;
    }
}
int PanelTop(int height,int requested){return std::clamp(requested<0?height-50:requested,0,std::max(0,height-24));}
void Panels(uint32_t* pixels,int width,int height,const uint16_t pads[2],bool /*training*/,int scale,int requested){
    const recomp::text::Canvas c{pixels,width,height};
    const int y=PanelTop(height/scale,requested)*scale;
    const auto reduced=[scale](int value){return (value*scale*3+2)/4;};
    const int panelWidth=reduced(90),panelHeight=reduced(32);
    constexpr int centresX[]={31,47,64,80},centresY[]={22,11,8,10};
    for(int p=0;p<2;++p){const int x=p?width-2*scale-panelWidth:2*scale;
        std::vector<uint32_t> background(size_t(panelWidth)*panelHeight);
        for(int row=0;row<panelHeight;++row)std::copy_n(pixels+size_t(y+row)*width+x,panelWidth,background.data()+size_t(row)*panelWidth);
        c.Fill(x,y,panelWidth,panelHeight,{201,166,90,255});c.Fill(x+scale,y+scale,panelWidth-2*scale,panelHeight-2*scale,{7,7,12,255});
        Sprite(pixels,width,height,8+NeoGeoInputDirection(pads[p])-1,x+reduced(13),y+reduced(16),reduced(24),x+scale,y+scale,x+panelWidth-scale,y+panelHeight-scale);
        for(int b=0;b<4;++b)Sprite(pixels,width,height,b*2+int(bool(pads[p]&(0x10<<b))),x+reduced(centresX[b]),y+reduced(centresY[b]),reduced((90*14+25)/50),x+scale,y+scale,x+panelWidth-scale,y+panelHeight-scale);
        // 224/255 opacity: about12% transparency, applied once to the whole panel.
        for(int row=0;row<panelHeight;++row)for(int col=0;col<panelWidth;++col){
            auto& pixel=pixels[size_t(y+row)*width+x+col];const auto below=background[size_t(row)*panelWidth+col];uint32_t blended=0xff000000;
            for(int channel=0;channel<3;++channel)blended|=((((pixel>>(8*channel))&255)*224+((below>>(8*channel))&255)*31+127)/255)<<(8*channel);
            pixel=blended;
        }
    }
}
}
void DrawNeoGeoInputPanels(uint32_t* pixels,int width,int height,const uint16_t pads[2],bool training,int panelTop){
    if(!pixels||!pads||width<240||height<80)return;
    Panels(pixels,width,height,pads,training,1,panelTop);
}
bool RecorderInputArtValid(){return std::all_of(Art().begin(),Art().end(),[](const auto& image){return !image.empty();});}
void BuildNeoGeoInputPresentation(const std::vector<uint32_t>& native,int width,int height,const uint16_t pads[2],bool training,std::vector<uint32_t>& out,const uint32_t* background,int panelTop){
    if(width<240||height<80||native.size()!=size_t(width)*height){out.clear();return;}
    out.resize(size_t(width)*height*9);
    for(int y=0;y<height*3;++y)for(int x=0;x<width*3;++x)out[size_t(y)*width*3+x]=native[size_t(y/3)*width+x/3];
    if(background){
        const int top=PanelTop(height,panelTop),panelWidth=(90*3+2)/4;
        for(int player=0;player<2;++player){const int left=player?width-2-panelWidth:2;
            for(int y=top*3;y<(top+24)*3;++y)for(int x=left*3;x<(left+panelWidth)*3;++x)
                out[size_t(y)*width*3+x]=background[size_t(y/3)*width+x/3];
        }
    }
    Panels(out.data(),width*3,height*3,pads,training,3,panelTop);
}
}

// SPDX-License-Identifier: GPL-3.0-only
#include "neogeo_input_display.h"
#include <vector>
#include <cstdlib>
void check(bool b){if(!b)std::abort();}
int main(){
    using port::NeoGeoInputDirection;
    check(NeoGeoInputDirection(0)==5&&NeoGeoInputDirection(1)==8&&NeoGeoInputDirection(2)==2);
    check(NeoGeoInputDirection(4)==4&&NeoGeoInputDirection(8)==6&&NeoGeoInputDirection(9)==9&&NeoGeoInputDirection(6)==1);
    check(NeoGeoInputDirection(3)==5&&NeoGeoInputDirection(12)==5&&NeoGeoInputDirection(0x100)==5);
    check(port::RecorderInputArtValid());
    constexpr uint32_t guard=0xaabbccdd;const int width=320,height=224;const size_t n=width*height;
    std::vector<uint32_t> idle(n+64,guard);uint16_t pads[2]={};
    port::DrawNeoGeoInputPanels(idle.data()+32,width,height,pads,false);
    for(int i=0;i<32;++i)check(idle[i]==guard&&idle[n+32+i]==guard);
    auto pressed=std::vector<uint32_t>(n+64,guard);pads[0]=0x19;
    port::DrawNeoGeoInputPanels(pressed.data()+32,width,height,pads,false);check(pressed!=idle);
    for(int y=0;y<height;++y)for(int x=0;x<width;++x){
        const auto at=32+size_t(y)*width+x;const bool p1=x>=2&&x<70&&y>=174&&y<198;
        if(!p1)check(pressed[at]==idle[at]);
    }
    pads[0]|=0x1500;auto operators=std::vector<uint32_t>(n+64,guard);
    port::DrawNeoGeoInputPanels(operators.data()+32,width,height,pads,false);check(operators==pressed);
    std::vector<uint32_t> training(n,guard);
    port::DrawNeoGeoInputPanels(training.data(),width,height,pads,true);
    for(int y=0;y<height;++y)for(int x=0;x<width;++x){
        const bool panel=(x>=2&&x<70)||(x>=250&&x<318);
        if(!panel||y<174||y>=198)check(training[size_t(y)*width+x]==guard);
    }
    std::vector<uint32_t> tiny(64,guard);port::DrawNeoGeoInputPanels(tiny.data(),8,8,pads,false);check(tiny==std::vector<uint32_t>(64,guard));
    const std::vector<uint32_t> native(n,guard);std::vector<uint32_t> detailed;
    port::BuildNeoGeoInputPresentation(native,width,height,pads,true,detailed);check(detailed.size()==n*9);
    for(int y=0;y<height*3;++y)for(int x=0;x<width*3;++x){
        const bool panel=(x>=6&&x<210)||(x>=750&&x<954);
        if(!panel||y<522||y>=594)check(detailed[size_t(y)*width*3+x]==guard);
    }
    check(training[174*width+2]==0xff66abcbu); // Gold border blended12% over known background.
    std::vector<uint32_t> fromPainted;port::BuildNeoGeoInputPresentation(training,width,height,pads,true,fromPainted,native.data());
    check(fromPainted==detailed); // No opaque native panel or doubled opacity underneath.
    port::BuildNeoGeoInputPresentation(tiny,width,height,pads,true,detailed);check(detailed.empty());
}

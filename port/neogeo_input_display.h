// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <cstdint>
#include <vector>
namespace port {
constexpr int NeoGeoInputDirection(uint16_t pad) {
    return 5+int(bool(pad&8))-int(bool(pad&4))+3*(int(bool(pad&1))-int(bool(pad&2)));
}
void DrawNeoGeoInputPanels(uint32_t* pixels,int width,int height,const uint16_t pads[2],bool training);
bool RecorderInputArtValid();
void BuildNeoGeoInputPresentation(const std::vector<uint32_t>& native,int width,int height,const uint16_t pads[2],bool training,std::vector<uint32_t>& out,const uint32_t* background=nullptr);
}

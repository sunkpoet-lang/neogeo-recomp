// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <cstdint>
namespace port {
// KOF95: an increased bout clock arms a marker; its first subsequent
// decrement starts the bout. Hit-stop/frozen clocks do not re-arm it.
struct ReplayRoundClock {
    uint16_t previous=0;
    bool armed=false,wasVersus=false;
    void Reset(uint16_t clock){previous=clock;armed=clock==0x6032;wasVersus=false;}
    bool Observe(uint16_t clock,bool versus){
        const bool valid=(clock>>8)<=0x99 && ((clock>>8)&15)<=9 && (clock&255)<=0x32;
        if(!versus||!valid){previous=clock;armed=false;wasVersus=false;return false;}
        if(!wasVersus&&clock==0x6032)armed=true;
        wasVersus=true;
        if(clock>previous)armed=true;
        const bool begin=armed&&clock<previous;
        if(begin)armed=false;
        previous=clock;return begin;
    }
};
}

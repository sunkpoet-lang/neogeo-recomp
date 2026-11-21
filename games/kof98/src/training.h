// SPDX-License-Identifier: GPL-3.0-only
// Independent native implementation. RAM facts investigated from peon2's kof98
// reference and verified native traces; no Lua implementation/assets imported.
#pragma once
#include "../../../runtime/neogeo_game.h"
#include "../../../runtime/state.h"
#include "../../../runtime/training_recordings.h"
#include "../../../machine/neogeo.h"
#include <array>
#include <cstdio>
#include "hitboxes.h"

namespace cps2recomp::kof98::training {
inline bool Active(){return !TheGame().online&&!TheGame().replayPlayback&&FeatureOn("training_session");}
inline bool Fighting(const NeoGeo& b){const auto phase=b.Peek8(0x10b094);return b.GameRunning()&&b.Peek8(0x10fdaf)==2&&b.Peek8(0x10fdb6)==1&&b.Peek8(0x10fdb7)==1&&(phase==1||phase==2);}
inline bool Ready(NeoGeo& b){return Fighting(b)&&b.Peek8(0x108239)>0&&b.Peek8(0x108239)<=0x68&&b.Peek8(0x108439)>0&&b.Peek8(0x108439)<=0x68;}
struct Press {uint16_t pad=0,count=0;};
struct State {
    bool seen=false;
    uint32_t entryTicks=0;
    std::array<std::array<Press,6>,2> history{};
    std::vector<uint8_t> checkpoint;
    cps2recomp::training::Recordings recordings;
};
inline State state;
inline void Clear(){auto kept=std::move(state.recordings);state={};state.recordings=std::move(kept);state.recordings.ResetInteraction();SetHud({});}
inline void Save(Archive& ar){
    uint32_t mark=0x4b393855;
    if(ar.Saving())ar.Mark(mark);
    else {ar.Pod(mark);ar.Require(mark==0x4b393854||mark==0x4b393855);if(!ar.Ok())return;}
    ar.Pod(state.seen);ar.Pod(state.entryTicks);ar.Pod(state.history);ar.BoundedVec(state.checkpoint,1024*1024);
    if(mark==0x4b393854)state.recordings={}; // first-slice save states contain no slots
    else state.recordings.Serialize(ar);
}
inline void Start(NeoGeo&){Clear();for(auto f:{"training_session","training_health","training_time","training_hud"})SetFeatureValue(f,1);}
inline void Stop(NeoGeo&){SetFeatureValue("training_session",0);Clear();}
inline void Reset(NeoGeo& b){
    if(!Active())return;
    state.recordings.ResetInteraction();
    if(state.checkpoint.empty())return;
    Archive check(state.checkpoint,false,true);b.Serialize(check);if(!check.Complete())return;
    Archive ar(state.checkpoint,false);b.Serialize(ar);
    state.history={};
}
inline void RecordSlot(NeoGeo&){if(Active())state.recordings.Record(FeatureValue("training_slot"));}
inline void PlaySlot(NeoGeo& b){if(Active()&&Ready(b))state.recordings.Play(FeatureValue("training_slot"),FeatureValue("training_playback"));}
inline void StopAction(NeoGeo&){if(Active())state.recordings.Stop();}
inline void ClearSlot(NeoGeo&){if(Active())state.recordings.Clear(FeatureValue("training_slot"));}
inline void Before(NeoGeo& b){
    if(!Active()) {if(state.seen)Clear();return;}
    state.seen=true;
    if(!b.GameRunning())return;
    if(!Ready(b)&&(state.recordings.Playing()||(state.recordings.Recording()&&state.recordings.Length(state.recordings.RecordingSlot()))))state.recordings.Stop();
    if(!Fighting(b)) {
        // Avoid phase writes. Let the MVS accept coins/start normally; selection
        // remains interactive, with P1's gameplay controls mirrored to P2.
        b.players[1]=b.players[0]&0xff;
        if((b.Peek8(0x10fdb6)!=1||b.Peek8(0x10fdb7)!=1)&&state.entryTicks<1800) {
            const auto at=state.entryTicks++%120;
            if(at<8){b.players[0]|=NeoGeo::kCoin;b.players[1]|=NeoGeo::kCoin;}
            if(at>=60&&at<68){b.players[0]|=NeoGeo::kStart;b.players[1]|=NeoGeo::kStart;}
        }
        return;
    }
    if(state.checkpoint.empty()&&Ready(b)){Archive ar(state.checkpoint,true);b.Serialize(ar);}
    const int dummy=FeatureValue("training_dummy");
    const bool facingLeft=b.Peek8(0x108331)==0;
    if(Ready(b)&&state.recordings.Recording()) {
        b.players[1]=state.recordings.Capture(b.players[0],facingLeft);b.players[0]=0;
    } else {
        auto recorded=Ready(b)?state.recordings.Next(facingLeft):std::optional<uint16_t>{};
        b.players[1]=recorded?*recorded:dummy==1?NeoGeo::kDown:dummy>=2?(facingLeft?NeoGeo::kRight:NeoGeo::kLeft)|(dummy==3?NeoGeo::kDown:0):0;
    }
    for(int p=0;p<2;++p){
        auto& h=state.history[p];const auto pad=uint16_t(b.players[p]&0xff);
        if(h[0].pad==pad&&h[0].count){if(h[0].count<65535)++h[0].count;}
        else {for(int i=5;i>0;--i)h[i]=h[i-1];h[0]={pad,1};}
    }
}
inline void After(NeoGeo& b){
    if(!Active())return;
    const bool fight=Fighting(b);
    if(Ready(b)){
        if(FeatureOn("training_health"))for(auto at:{0x108239u,0x108439u}){const auto hp=b.Peek8(at);if(hp>0&&hp<=0x68)b.Poke8(at,0x67);}
        if(FeatureOn("training_time"))b.Poke16(0x10a83a,0x6000);
    }
    if(!FeatureOn("training_hud")){SetHud({});return;}
    std::vector<HudLine> hud={{.5f,.15f,0,fight?"TRAINING - OFFLINE":"TRAINING - SELECT TEAMS (P1 BOTH)"}};
    char action[80];const auto& slots=state.recordings;
    if(slots.Recording())snprintf(action,sizeof(action),"REC S%d %zu/1800 - P1 TO P2",slots.RecordingSlot()+1,slots.Length(slots.RecordingSlot()));
    else if(slots.Playing())snprintf(action,sizeof(action),"PLAY S%d %u/%zu %s",slots.PlayingSlot()+1,slots.Cursor(),slots.Length(slots.PlayingSlot()),slots.PlaybackMode()==0?"ONCE":slots.PlaybackMode()==1?"LOOP":"RANDOM");
    else snprintf(action,sizeof(action),"S1 %zuf  S2 %zuf  S3 %zuf",slots.Length(0),slots.Length(1),slots.Length(2));
    hud.push_back({.5f,.22f,0,action});
    if(fight)for(int p=0;p<2;++p){
        char line[64];snprintf(line,sizeof(line),"P%d HP %u POW %u",p+1,b.Peek8(p?0x108439:0x108239),b.Peek8(p?0x1083e8:0x1081e8));
        const float x=p?.98f:.02f;const int align=p?1:-1;hud.push_back({x,.70f,align,line});
    }
    SetHud(std::move(hud));
}
inline void ShowBodyBoxes(NeoGeo&){if(Active())for(auto name:{"training_box_hurt","training_box_attack","training_box_guard","training_box_push"})SetFeatureValue(name,1);}
inline void HideBodyBoxes(NeoGeo&){for(auto name:{"training_box_hurt","training_box_attack","training_box_guard","training_box_push","training_box_axes","training_box_projectile"})SetFeatureValue(name,0);}
inline void Inspect(const NeoGeo& b){
    std::vector<HudBox> boxes;
    if(!Active()||!Fighting(b)){SetBoxes({});return;}
    const auto read=[&](uint32_t at){return b.Peek8(at);};
    const auto frame=FeatureOn("training_box_projectile")?collision::ReadScene(read):collision::ReadPlayers(read);
    for(const auto& box:frame.boxes){
        const auto kind=box.kind;const char* name=box.player<0?"training_box_projectile":kind==collision::Kind::Hurt?"training_box_hurt":kind==collision::Kind::Attack?"training_box_attack":kind==collision::Kind::Guard?"training_box_guard":"training_box_push";
        if(!FeatureOn(name))continue;
        constexpr std::array<std::array<uint8_t,3>,6> colours={{{119,119,255},{255,0,0},{204,204,255},{0,255,0},{0,255,255},{255,102,255}}};
        const auto colour=colours[unsigned(kind)];
        boxes.push_back({box.left,box.top,box.right-box.left+1,box.bottom-box.top+1,colour[0],colour[1],colour[2],uint8_t(kind==collision::Kind::Push?32:64)});
    }
    if(frame.valid&&FeatureOn("training_box_axes"))for(const auto axis:frame.axes){boxes.push_back({axis.x-6,axis.y,13,1,255,255,255,255});boxes.push_back({axis.x,axis.y-6,1,13,255,255,255,255});}
    SetBoxes(std::move(boxes));
}
inline void Register(){
    auto& g=TheGame();auto& h=TheNeoGeoGame();
    constexpr unsigned own=kFeatureOwn,aid=kFeatureOwn|kFeatureTraining;
    g.features.insert(g.features.end(),{
        {"training_session","Training","Training session","Offline practice; P1 chooses both teams.",own|kFeatureSession},
        {"training_health","Training","Refill health","Refills living fighters during verified battle.",aid},
        {"training_time","Training","Infinite time","Holds the verified KOF98 clock.",aid},
        {"training_dummy","Training","Dummy","Basic P2 behaviour; guard follows facing.",aid,{"Stand","Crouch","Guard","Low guard"}},
        {"training_hud","Training","Training HUD","Current-input panel, health and read-only meter.",aid},
        {"training_slot","Dummy recordings","Selected slot","Record replaces only this slot.",aid,{"Slot 1","Slot 2","Slot 3"}},
        {"training_playback","Dummy recordings","Playback mode","Random uses populated slots and a saved seed.",aid,{"Once","Loop selected","Random slots"}}});
    g.pages.push_back({"Training","","Offline practice; advanced drills are not implemented.",true});
    g.pages.push_back({"Dummy recordings","Training","P1 controls P2 while recording. Limit 1800 frames per slot.",false});
    g.commands.insert(g.commands.end(),{{"training_start","Training","Start training","Select teams using P1.",nullptr},
        {"training_reset","Training","Reset round","Return to the first verified battle checkpoint.",nullptr,kKeyRestart},
        {"training_stop","Training","End training","Stop practice effects and dummy input.",nullptr},
        {"dummy_record","Dummy recordings","Record selected slot","P1 controls the dummy; P1 fighter stays neutral.",nullptr},
        {"dummy_play","Dummy recordings","Play selected slot","Start during battle, using the chosen playback mode.",nullptr},
        {"dummy_stop","Dummy recordings","Stop dummy action","Finish recording or stop playback.",nullptr},
        {"dummy_clear","Dummy recordings","Clear selected slot","Erase only the selected input slot.",nullptr}});
    h.commands.insert(h.commands.end(),{Start,Reset,Stop,RecordSlot,PlaySlot,StopAction,ClearSlot});h.beforeFrame=Before;h.frame=After;g.state=Save;g.inTraining=Active;g.noRunAhead=Active;
    g.features.insert(g.features.end(),{
        {"training_box_hurt","Hitboxes","Hurt boxes","Player vulnerability, blue.",aid},
        {"training_box_attack","Hitboxes","Attack boxes","Player attacks, red; projectiles not included.",aid},
        {"training_box_guard","Hitboxes","Guard boxes","Guard detection, pale blue.",aid},
        {"training_box_push","Hitboxes","Push boxes","Player body separation, green.",aid},
        {"training_box_axes","Hitboxes","Player axes","Player position crosshairs.",aid},
        {"training_box_projectile","Hitboxes","Projectile boxes","Bounded projectile list; throws are not included.",aid}});
    g.pages.push_back({"Hitboxes","Training","Player and projectile boxes. Throw coverage pending.",false});
    g.commands.insert(g.commands.end(),{{"training_boxes_show","Hitboxes","Show body boxes","Enable hurt, attack, guard and push boxes.",nullptr},{"training_boxes_hide","Hitboxes","Hide all boxes","Disable box and axis overlays.",nullptr}});
    h.commands.insert(h.commands.end(),{ShowBodyBoxes,HideBodyBoxes});h.inspect=Inspect;
    TrainingAids({"training_health","training_time","training_dummy","training_hud","training_slot","training_playback","training_box_hurt","training_box_attack","training_box_guard","training_box_push","training_box_axes","training_box_projectile"});
}
}

// SPDX-License-Identifier: GPL-3.0-only
// Independent native implementation. RAM facts investigated from the original KOF2002
// reference and native original-set traces; no Lua implementation/assets imported.
#pragma once
#include "../../../runtime/neogeo_game.h"
#include "../../../runtime/state.h"
#include "../../../runtime/training_recordings.h"
#include "../../../machine/neogeo.h"
#include <array>
#include <cstdio>
#include "random_guard.h"
#include "recovery.h"
#include "hitboxes.h"

namespace cps2recomp::kof2002::training {
inline bool Active(){return !TheGame().online&&!TheGame().replayPlayback&&FeatureOn("training_session");}
inline bool Fighting(const NeoGeo& b){const auto phase=b.Peek8(0x10b056);return b.GameRunning()&&b.Peek8(0x10fdaf)==2&&b.Peek8(0x10fdb6)==1&&b.Peek8(0x10fdb7)==1&&(phase==1||phase==2);}
inline bool Ready(NeoGeo& b){return Fighting(b)&&b.Peek8(0x108239)>0&&b.Peek8(0x108239)<=0x66&&b.Peek8(0x108439)>0&&b.Peek8(0x108439)<=0x66;}
struct Press {uint16_t pad=0,count=0;};
struct State {
    bool seen=false;
    uint32_t entryTicks=0;
    std::array<std::array<Press,6>,2> history{};
    std::vector<uint8_t> checkpoint;
    cps2recomp::training::Recordings recordings;
    RandomGuard guard;
    RecoveryDetector recovery;
    bool responseOwned=false;
};
inline State state;
struct ThrowCapture {const NeoGeo* owner=nullptr;uint64_t frame=0,epoch=0;std::array<std::optional<collision::Box>,2> boxes{};};
inline ThrowCapture throws;
inline void CaptureThrow(const NeoGeo& b,uint32_t actor){
    if(!Active()||!FeatureOn("training_box_throw")||!Fighting(b))return;
    const auto box=collision::ReadThrow([&](uint32_t at){return b.Peek8(at);},actor&0xffffff);
    if(!box)return;
    throws.owner=&b;throws.frame=b.Frames();throws.epoch=b.InspectionEpoch();throws.boxes[box->player]=box;
}
inline void Clear(){throws={};auto kept=std::move(state.recordings);state={};state.recordings=std::move(kept);state.recordings.ResetInteraction();SetHud({});}
inline void Save(Archive& ar){
    // Previous original-set builds saved board state only.
    if(!ar.Saving()&&ar.Complete()){state={};return;}
    ar.Mark(0x4b303201);
    if(!ar.Ok())return;
    ar.Pod(state.seen);ar.Pod(state.entryTicks);ar.Pod(state.history);ar.BoundedVec(state.checkpoint,1024*1024);
    state.recordings.Serialize(ar);state.guard.Serialize(ar);state.recovery.Serialize(ar);
    uint8_t owned=state.responseOwned;ar.Pod(owned);
    ar.Require(owned<=1&&(!owned||(state.recordings.Playing()&&!state.recordings.Recording()&&state.recordings.PlaybackMode()==0)));
    if(ar.Ok()&&!ar.Saving())state.responseOwned=owned!=0;
}
inline void Start(NeoGeo&){Clear();for(auto f:{"training_session","training_health","training_time","training_hud"})SetFeatureValue(f,1);}
inline void Stop(NeoGeo&){SetFeatureValue("training_session",0);Clear();}
inline void Reset(NeoGeo& b){
    if(!Active())return;

    throws={};
    state.recordings.ResetInteraction();
    state.guard.ResetInteraction();
    state.recovery.ResetInteraction();
    state.responseOwned=false;
    if(state.checkpoint.empty())return;
    Archive check(state.checkpoint,false,true);b.Serialize(check);if(!check.Complete())return;
    Archive ar(state.checkpoint,false);b.Serialize(ar);
    state.history={};
}
inline void RecordSlot(NeoGeo&){if(Active()){state.responseOwned=false;state.recovery.ResetInteraction();state.recordings.Record(FeatureValue("training_slot"));}}
inline void PlaySlot(NeoGeo& b){if(Active()&&Ready(b)){state.responseOwned=false;state.recovery.ResetInteraction();state.recordings.Play(FeatureValue("training_slot"),FeatureValue("training_playback"));}}
inline void StopAction(NeoGeo&){if(Active()){state.responseOwned=false;state.recovery.ResetInteraction();state.recordings.Stop();}}
inline void ClearSlot(NeoGeo&){if(Active()){state.responseOwned=false;state.recovery.ResetInteraction();state.recordings.Clear(FeatureValue("training_slot"));}}
inline void Before(NeoGeo& b){
    throws={};
    b.ObserveRamWrite(0x10e7b5,Active()&&FeatureOn("training_box_throw")?CaptureThrow:nullptr);

    if(!Active()) {if(state.responseOwned)state.recordings.Stop();state.responseOwned=false;state.recovery.ResetInteraction();if(state.seen)Clear();return;}
    state.seen=true;
    if(!b.GameRunning()){if(state.responseOwned)state.recordings.Stop();state.responseOwned=false;state.recovery.ResetInteraction();return;}
    if(!Ready(b)){if(state.responseOwned)state.recordings.Stop();state.responseOwned=false;state.guard.ResetInteraction();state.recovery.ResetInteraction();}
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
    const bool randomGuard=dummy==4||dummy==5;
    const bool facingLeft=b.Peek8(0x108331)==0;
    if(Ready(b)&&state.recordings.Recording()) {
        state.responseOwned=false;
        state.recovery.ResetInteraction();
        state.guard.ResetInteraction();
        b.players[1]=state.recordings.Capture(b.players[0],facingLeft);b.players[0]=0;
    } else {
        const int response=FeatureValue("training_response");
        if(!response&&state.responseOwned){state.recordings.Stop();state.responseOwned=false;}
        if(!response||!Ready(b)||state.recordings.Playing())state.recovery.ResetInteraction();
        else {
            const auto event=state.recovery.Next([&](uint32_t at){return b.Peek8(at);});
            const bool match=event!=RecoveryDetector::None&&(response==4||(response==1&&event==RecoveryDetector::Block)||(response==2&&event==RecoveryDetector::Hit)||(response==3&&event==RecoveryDetector::AirborneHit));
            if(match)state.responseOwned=state.recordings.Play(FeatureValue("training_slot"),0);
        }
        auto recorded=Ready(b)?state.recordings.Next(facingLeft):std::optional<uint16_t>{};
        if(!state.recordings.Playing())state.responseOwned=false;
        if(recorded)state.recovery.ResetInteraction();
        if(recorded||!randomGuard||!Ready(b))state.guard.ResetInteraction();
        bool randomBack=false;
        if(!recorded&&randomGuard&&Ready(b)){
            const bool attack=(b.players[0]&0xf0)!=0; // No unqualified collision table.
            randomBack=state.guard.Next(attack);
        }
        const auto back=facingLeft?NeoGeo::kRight:NeoGeo::kLeft;
        b.players[1]=recorded?*recorded:randomGuard?(randomBack?(back|(dummy==5?NeoGeo::kDown:0)):0):dummy==1?NeoGeo::kDown:dummy>=2?back|(dummy==3?NeoGeo::kDown:0):0;
    }
    for(int p=0;p<2;++p){
        auto& h=state.history[p];const auto pad=uint16_t(b.players[p]&0xff);
        if(h[0].pad==pad&&h[0].count){if(h[0].count<65535)++h[0].count;}
        else {for(int i=5;i>0;--i)h[i]=h[i-1];h[0]={pad,1};}
    }
}
inline void RefreshHud(const NeoGeo& b){
    if(!Active()){SetHud({});return;}
    const bool fight=Fighting(b);
    if(!FeatureOn("training_hud")){SetHud({});return;}
    std::vector<HudLine> hud;
    const int status=FeatureValue("training_hud_status");
    if(status<2)hud.push_back({.5f,.15f,0,fight?"TRAINING - OFFLINE":"TRAINING - SELECT TEAMS (P1 BOTH)"});
    char action[80];const auto& slots=state.recordings;
    if(slots.Recording())snprintf(action,sizeof(action),"REC S%d %zu/1800 - P1 TO P2",slots.RecordingSlot()+1,slots.Length(slots.RecordingSlot()));
    else if(slots.Playing())snprintf(action,sizeof(action),"%s S%d %u/%zu %s",state.responseOwned?"RESP":"PLAY",slots.PlayingSlot()+1,slots.Cursor(),slots.Length(slots.PlayingSlot()),slots.PlaybackMode()==0?"ONCE":slots.PlaybackMode()==1?"LOOP":"RANDOM");
    else if(FeatureValue("training_dummy")==4||FeatureValue("training_dummy")==5)snprintf(action,sizeof(action),"RG %s | S1 %zuf S2 %zuf S3 %zuf",state.guard.Episode()?(state.guard.Blocking()?(FeatureValue("training_dummy")==5?"LOW":"GUARD"):"PASS"):"WAIT",slots.Length(0),slots.Length(1),slots.Length(2));
    else snprintf(action,sizeof(action),"S1 %zuf  S2 %zuf  S3 %zuf",slots.Length(0),slots.Length(1),slots.Length(2));
    if(status==0)hud.push_back({.5f,.22f,0,action});
    if(fight&&FeatureValue("training_hud_resources")==0)for(int p=0;p<2;++p){
        char line[64];snprintf(line,sizeof(line),"P%d HP %u POW %u",p+1,b.Peek8(p?0x108439:0x108239),(b.Peek8(p?0x1083e8:0x1081e8)+0x42u*b.Peek8(p?0x1084e3:0x1082e3)));
        const float x=p?.98f:.02f;const int align=p?1:-1;hud.push_back({x,.70f,align,line});
    }
    SetHud(std::move(hud));
}
inline void After(NeoGeo& b){
    if(!Active())return;
    if(Ready(b)){
        if(FeatureOn("training_health"))for(auto at:{0x108239u,0x108439u}){const auto hp=b.Peek8(at);if(hp>0&&hp<=0x66)b.Poke8(at,0x66);}
        if(FeatureOn("training_time"))b.Poke16(0x10a7d2,0x6000);
    }
    RefreshHud(b);
}
inline void RestoreHud(NeoGeo& b){
    if(!Active())return;
    for(auto name:{"training_hud_inputs","training_hud_position","training_hud_status","training_hud_resources"})SetFeatureValue(name,0);
    SetFeatureValue("training_hud",1);RefreshHud(b);
}
inline void ShowBodyBoxes(NeoGeo&){if(Active())for(auto name:{"training_box_hurt","training_box_attack","training_box_guard","training_box_push"})SetFeatureValue(name,1);}
inline void HideBodyBoxes(NeoGeo&){throws={};for(auto name:{"training_box_hurt","training_box_attack","training_box_guard","training_box_push","training_box_axes","training_box_throw","training_box_throwable","training_box_projectile"})SetFeatureValue(name,0);}
inline void Inspect(const NeoGeo& b){
    RefreshHud(b);std::vector<HudBox> boxes;
    if(!Active()||!Fighting(b)){throws={};SetBoxes({});return;}
    if(!FeatureOn("training_box_throw"))throws={};
    const auto read=[&](uint32_t at){return b.Peek8(at);};
    const auto frame=FeatureOn("training_box_projectile")?collision::ReadScene(read):collision::ReadPlayers(read);
    constexpr const char* names[]={"training_box_hurt","training_box_attack","training_box_guard","training_box_push"};
    constexpr std::array<std::array<uint8_t,3>,4> colours={{{119,119,255},{255,0,0},{204,204,255},{0,255,0}}};
    for(const auto& box:frame.boxes){const auto kind=unsigned(box.kind);const bool projectile=box.player<0;
        if(!FeatureOn(projectile?"training_box_projectile":names[kind]))continue;
        const auto colour=projectile?(box.kind==collision::Kind::ProjectileAttack?std::array<uint8_t,3>{255,102,255}:std::array<uint8_t,3>{0,255,255}):colours[kind];
        boxes.push_back({box.left,box.top,box.right-box.left+1,box.bottom-box.top+1,colour[0],colour[1],colour[2],uint8_t(box.kind==collision::Kind::Push?32:64)});
    }
    if(frame.valid&&FeatureOn("training_box_throwable"))for(unsigned player=0;player<2;++player){
        const auto box=collision::ReadThrowable([&](uint32_t at){return b.Peek8(at);},0x108100+player*0x200);
        if(box)boxes.push_back({box->left,box->top,box->right-box->left+1,box->bottom-box->top+1,240,240,240,32});
    }
    if(frame.valid&&FeatureOn("training_box_axes"))for(const auto axis:frame.axes){boxes.push_back({axis.x-6,axis.y,13,1,255,255,255,255});boxes.push_back({axis.x,axis.y-6,1,13,255,255,255,255});}
    if(throws.owner==&b&&throws.epoch==b.InspectionEpoch()&&throws.frame+1==b.Frames())for(const auto& box:throws.boxes)if(box)boxes.push_back({box->left,box->top,box->right-box->left+1,box->bottom-box->top+1,255,255,0,64});
    SetBoxes(std::move(boxes));
}
inline void Register(){
    auto& g=TheGame();auto& h=TheNeoGeoGame();
    constexpr unsigned own=kFeatureOwn,aid=kFeatureOwn|kFeatureTraining;
    g.features.insert(g.features.end(),{
        {"training_session","Session","Training session","Offline practice; P1 chooses both teams.",own|kFeatureSession},
        {"training_health","Resources","Refill health","Refills living fighters during verified battle.",aid},
        {"training_time","Resources","Infinite time","Holds this set's verified battle clock.",aid},
        {"training_dummy","Dummy","Behaviour","Attack-button episodes choose guard/pass; no automatic collision or attack-height detection.",aid,{"Stand","Crouch","Guard","Low guard","Random guard","Random low guard"}},
        {"training_response","Dummy","Recovery response","Play selected slot once after conservative recovery. Not first-frame reversal; airborne includes swept hits.",aid,{"Off","After block","After hit","After airborne hit","After any"}},
        {"training_hud","HUD","Training HUD","Current-input panel, health and read-only meter.",aid},
        {"training_slot","Recordings","Selected slot","Record replaces only this slot.",aid,{"Slot 1","Slot 2","Slot 3"}},
        {"training_playback","Recordings","Playback mode","Random uses populated slots and a saved seed.",aid,{"Once","Loop selected","Random slots"}}});
    g.pages.push_back({"Training","","Original KOF2002 practice; recorded responses are conservative, not first-frame reversals.",true});
    g.pages.push_back({"Session","Training","Start, reset or end offline practice.",false});
    g.pages.push_back({"Resources","Training","Verified health and time controls; meter is read-only.",false});
    g.pages.push_back({"Dummy","Training","Verified basic stance and side-relative guard.",false});
    g.pages.push_back({"Recordings","Training","P1 controls P2 while recording. Limit 1800 frames per slot.",false});
    g.commands.insert(g.commands.end(),{{"training_start","Session","Start training","Select teams using P1.",nullptr},
        {"training_reset","Session","Reset round","Return to the first verified battle checkpoint.",nullptr,kKeyRestart},
        {"training_stop","Session","End training","Stop practice effects and dummy input.",nullptr},
        {"dummy_record","Recordings","Record selected slot","P1 controls the dummy; P1 fighter stays neutral.",nullptr},
        {"dummy_play","Recordings","Play selected slot","Start during battle, using the chosen playback mode.",nullptr},
        {"dummy_stop","Recordings","Stop dummy action","Finish recording or stop playback.",nullptr},
        {"dummy_clear","Recordings","Clear selected slot","Erase only the selected input slot.",nullptr}});
    h.commands.insert(h.commands.end(),{Start,Reset,Stop,RecordSlot,PlaySlot,StopAction,ClearSlot});h.beforeFrame=Before;h.frame=After;g.state=Save;g.inTraining=Active;g.noRunAhead=Active;
    h.inspect=Inspect;
    g.pages.push_back({"Hitboxes","Training","Player boxes, bounded projectile areas, yellow attempts and light grey defender vulnerability.",false});
    g.features.insert(g.features.end(),{
        {"training_box_hurt","Hitboxes","Hurt boxes","Player vulnerability, blue.",aid},
        {"training_box_attack","Hitboxes","Attack boxes","Player attacks, red; not projectiles.",aid},
        {"training_box_guard","Hitboxes","Guard boxes","Guard detection, pale blue.",aid},
        {"training_box_push","Hitboxes","Push boxes","Player separation, green.",aid},
        {"training_box_axes","Hitboxes","Player axes","Position crosshairs.",aid},
        {"training_box_throw","Hitboxes","Throw attempts","Yellow event-time attempt area; not opponent throwability.",aid},
        {"training_box_projectile","Hitboxes","Projectile boxes","Bounded object list; ownership is not inferred.",aid},
        {"training_box_throwable","Hitboxes","Throw vulnerability","Light grey reference area; not guaranteed throw success.",aid}});
    g.commands.insert(g.commands.end(),{{"training_boxes_show","Hitboxes","Show body boxes","Enable player hurt, attack, guard and push boxes.",nullptr},{"training_boxes_hide","Hitboxes","Hide all boxes","Disable player boxes and axes.",nullptr}});
    h.commands.insert(h.commands.end(),{ShowBodyBoxes,HideBodyBoxes});
    g.pages.push_back({"HUD","Training","Current inputs and verified read-only resources.",false});
    g.features.insert(g.features.end(),{
        {"training_hud_inputs","HUD","Input panels","Override the global input display only in offline practice.",aid,{"Follow global","Show","Hide"}},
        {"training_hud_position","HUD","Panel position","Accepted size/opacity; fixed safe anchors.",aid,{"Above supers","Higher","Upper sides"}},
        {"training_hud_status","HUD","Status text","Reduce training/recording text without changing playback.",aid,{"Full","Session only","Hidden"}},
        {"training_hud_resources","HUD","Portrait resources","Show or hide the verified HP/POW readouts.",aid,{"Show","Hide"}}});
    g.commands.push_back({"training_hud_restore","HUD","Restore HUD defaults","Restore presentation only; leave drills and global inputs unchanged.",nullptr});h.commands.push_back(RestoreHud);
    TrainingAids({"training_health","training_time","training_dummy","training_response","training_hud","training_slot","training_playback","training_box_hurt","training_box_attack","training_box_guard","training_box_push","training_box_axes","training_box_throw","training_box_throwable","training_box_projectile"});
}
}

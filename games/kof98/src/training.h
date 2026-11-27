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
#include "random_guard.h"
#include "recovery.h"
#include "combo.h"

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
    RandomGuard guard;
    RecoveryDetector recovery;
    bool responseOwned=false;
    Combo combo; // (the dummy's side of a combo: the HUD and the after-combo refill)
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
    uint32_t mark=0x4b393859;
    if(ar.Saving())ar.Mark(mark);
    else {ar.Pod(mark);ar.Require(mark>=0x4b393854&&mark<=0x4b393859);if(!ar.Ok())return;}
    ar.Pod(state.seen);ar.Pod(state.entryTicks);ar.Pod(state.history);ar.BoundedVec(state.checkpoint,1024*1024);
    if(mark==0x4b393854)state.recordings={}; // first-slice save states contain no slots
    else state.recordings.Serialize(ar);
    if(mark>=0x4b393856)state.guard.Serialize(ar);else state.guard={};
    if(mark>=0x4b393857)state.recovery.Serialize(ar);else state.recovery={};
    if(mark>=0x4b393858){
        uint8_t owned=state.responseOwned;ar.Pod(owned);
        ar.Require(owned<=1&&(!owned||(state.recordings.Playing()&&!state.recordings.Recording()&&state.recordings.PlaybackMode()==0)));
        if(ar.Ok()&&!ar.Saving())state.responseOwned=owned!=0;
    }else state.responseOwned=false;
    if(mark>=0x4b393859)state.combo.Serialize(ar);else state.combo={};
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
    b.ObserveRamWrite(0x10ebdf,Active()&&FeatureOn("training_box_throw")?CaptureThrow:nullptr);
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
            const auto frame=collision::ReadPlayers([&](uint32_t at){return b.Peek8(at);});
            const bool attack=(b.players[0]&0xf0)||std::any_of(frame.boxes.begin(),frame.boxes.end(),[](const auto& box){return box.player==0&&box.kind==collision::Kind::Attack;});
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
        char line[64];snprintf(line,sizeof(line),"P%d HP %u POW %u",p+1,b.Peek8(p?0x108439:0x108239),b.Peek8(p?0x1083e8:0x1081e8));
        const float x=p?.98f:.02f;const int align=p?1:-1;hud.push_back({x,.70f,align,line});
    }
    if(fight&&FeatureValue("training_hud_combo")==0){
        char line[48];const auto& c=state.combo;
        snprintf(line,sizeof(line),"COMBO %u",unsigned(c.hits?c.hits:c.shown));hud.push_back({.5f,.30f,0,line});
        snprintf(line,sizeof(line),"DAMAGE %u  TOTAL %u",unsigned(c.lastDamage),unsigned(c.total));hud.push_back({.5f,.35f,0,line});
    }
    if(const int history=FeatureValue("training_hud_history"))for(int p=0;p<2;++p){
        if(history!=3&&history!=p+1)continue;
        for(int i=0;i<6;++i){
            const auto& e=state.history[p][i];if(!e.count)break;
            // (fighting notation: 6 is forward; the side's facing now, 0x108131/0x108331 at 0 when facing left)
            const bool left=b.Peek8(p?0x108331:0x108131)==0;
            const unsigned pad=e.pad;const int v=(pad&1)?0:(pad&2)?2:1,h=(pad&4)?(left?2:0):(pad&8)?(left?0:2):1; // up, down, left, right
            const char dir="789456123"[v*3+h];
            char line[32];snprintf(line,sizeof(line),"%c%s%s%s%s %u",dir,(pad&0x10)?"A":"",(pad&0x20)?"B":"",(pad&0x40)?"C":"",(pad&0x80)?"D":"",unsigned(e.count));
            hud.push_back({p?.98f:.02f,.30f+.05f*float(i),p?1:-1,line});
        }
    }
    SetHud(std::move(hud));
}
inline void After(NeoGeo& b){
    if(!Active())return;
    if(Ready(b)){
        state.combo.Next(b.Peek8(0x108439),b.Peek8(0x1084b0)!=0);
        const int health=FeatureValue("training_health");
        if(health==1)for(auto at:{0x108239u,0x108439u}){const auto hp=b.Peek8(at);if(hp>0&&hp<=0x68)b.Poke8(at,0x67);}
        // (after combo: a side not in hitstun, its attacker's combo counter on it at 0, fills up a few points a frame)
        if(health==2)for(int p=0;p<2;++p){const uint32_t at=p?0x108439u:0x108239u;const auto hp=b.Peek8(at);
            if(hp>0&&hp<0x67&&b.Peek8(p?0x1084b0u:0x1082b0u)==0)b.Poke8(at,uint8_t(hp+4>0x67?0x67:hp+4));}
        if(FeatureOn("training_meter"))for(auto at:{0x1081e8u,0x1083e8u})b.Poke8(at,0x80);
        if(FeatureOn("training_time"))b.Poke16(0x10a83a,0x6000);
    }
    RefreshHud(b);
}
inline void RestoreHud(NeoGeo& b){
    if(!Active())return;
    for(auto name:{"training_hud_inputs","training_hud_position","training_hud_status","training_hud_resources","training_hud_combo","training_hud_history"})SetFeatureValue(name,0);
    SetFeatureValue("training_hud",1);RefreshHud(b);
}
inline void ShowBodyBoxes(NeoGeo&){if(Active())for(auto name:{"training_box_hurt","training_box_attack","training_box_guard","training_box_push"})SetFeatureValue(name,1);}
inline void HideBodyBoxes(NeoGeo&){throws={};for(auto name:{"training_box_hurt","training_box_attack","training_box_guard","training_box_push","training_box_axes","training_box_projectile","training_box_throw"})SetFeatureValue(name,0);}
inline void Inspect(const NeoGeo& b){
    RefreshHud(b);
    std::vector<HudBox> boxes;
    if(!Active()||!Fighting(b)){throws={};SetBoxes({});return;}
    if(!FeatureOn("training_box_throw"))throws={};
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
    if(throws.owner==&b&&throws.epoch==b.InspectionEpoch()&&throws.frame+1==b.Frames())for(const auto& box:throws.boxes)if(box)boxes.push_back({box->left,box->top,box->right-box->left+1,box->bottom-box->top+1,255,255,0,64});
    SetBoxes(std::move(boxes));
}
inline void Register(){
    auto& g=TheGame();auto& h=TheNeoGeoGame();
    constexpr unsigned own=kFeatureOwn,aid=kFeatureOwn|kFeatureTraining;
    g.features.insert(g.features.end(),{
        {"training_session","Session","Training session","Offline practice; P1 chooses both teams.",own|kFeatureSession},
        {"training_health","Resources","Refill health","Always full, or filled up again once a combo ends (the side is out of hitstun).",aid,{"Off","Always full","After combo"}},
        {"training_meter","Resources","Meter always full","Keeps both power gauges full during battle.",aid},
        {"training_time","Resources","Infinite time","Holds the verified KOF98 clock.",aid},
        {"training_dummy","Dummy","Behaviour","Random modes choose one saved guard/pass intention; select standing or low explicitly.",aid,{"Stand","Crouch","Guard","Low guard","Random guard","Random low guard"}},
        {"training_response","Dummy","Recovery response","Play selected slot once after conservative recovery. Not first-frame reversal; airborne includes swept hits.",aid,{"Off","After block","After hit","After airborne hit","After any"}},
        {"training_hud","HUD","Training HUD","Current-input panel, health and read-only meter.",aid},
        {"training_slot","Recordings","Selected slot","Record replaces only this slot.",aid,{"Slot 1","Slot 2","Slot 3"}},
        {"training_playback","Recordings","Playback mode","Random uses populated slots and a saved seed.",aid,{"Once","Loop selected","Random slots"}}});
    g.pages.push_back({"Training","","Offline practice; advanced drills are not implemented.",true});
    g.pages.push_back({"Session","Training","Start, reset or end offline practice.",false});
    g.pages.push_back({"Resources","Training","Health, meter and time controls.",false});
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
    g.features.insert(g.features.end(),{
        {"training_box_hurt","Hitboxes","Hurt boxes","Player vulnerability, blue.",aid},
        {"training_box_attack","Hitboxes","Attack boxes","Player attacks, red; projectiles not included.",aid},
        {"training_box_guard","Hitboxes","Guard boxes","Guard detection, pale blue.",aid},
        {"training_box_push","Hitboxes","Push boxes","Player body separation, green.",aid},
        {"training_box_axes","Hitboxes","Player axes","Player position crosshairs.",aid},
        {"training_box_projectile","Hitboxes","Projectile boxes","Bounded projectile list; independent of throws.",aid},
        {"training_box_throw","Hitboxes","Throw attempts","Yellow: sampled event-time throw area; not throwability.",aid}});
    g.pages.push_back({"Hitboxes","Training","Read-only collision views. Throw roster coverage is partial.",false});
    g.pages.push_back({"HUD","Training","Show the approved input panels and verified portrait resources.",false});
    g.commands.insert(g.commands.end(),{{"training_boxes_show","Hitboxes","Show body boxes","Enable hurt, attack, guard and push boxes.",nullptr},{"training_boxes_hide","Hitboxes","Hide all boxes","Disable box and axis overlays.",nullptr}});
    h.commands.insert(h.commands.end(),{ShowBodyBoxes,HideBodyBoxes});h.inspect=Inspect;
    g.features.insert(g.features.end(),{
        {"training_hud_inputs","HUD","Input panels","Override the global input display only in offline practice.",aid,{"Follow global","Show","Hide"}},
        {"training_hud_position","HUD","Panel position","Accepted size/opacity; fixed safe anchors.",aid,{"Above supers","Higher","Upper sides"}},
        {"training_hud_status","HUD","Status text","Reduce training/recording text without changing playback.",aid,{"Full","Session only","Hidden"}},
        {"training_hud_resources","HUD","Portrait resources","Show or hide the verified HP/POW readouts.",aid,{"Show","Hide"}},
        {"training_hud_combo","HUD","Combo counter","The dummy's combo: hits, last hit's damage and the total.",aid,{"Show","Hide"}},
        {"training_hud_history","HUD","Input history","The last six inputs in fighting notation (6 forward), buttons and frames held.",aid,{"Hide","P1","P2","Both"}}});
    g.commands.push_back({"training_hud_restore","HUD","Restore HUD defaults","Restore presentation only; leave drills and global inputs unchanged.",nullptr});h.commands.push_back(RestoreHud);
    TrainingAids({"training_health","training_meter","training_hud_combo","training_hud_history","training_time","training_dummy","training_response","training_hud","training_slot","training_playback","training_box_hurt","training_box_attack","training_box_guard","training_box_push","training_box_axes","training_box_projectile","training_box_throw"});
}
}

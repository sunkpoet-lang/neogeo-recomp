// SPDX-License-Identifier: GPL-3.0-only
// Neo Geo adapter for the shared viewer and the CPS fork's format-2 codec.
#include "neogeo_replay.h"
#include "game_info.h"
#include "neogeo_port.h"
#include "fareplay.h"
#include "replay_round_clock.h"
#include "replay_identity.h"
#include "../runtime/neogeo_game.h"
#include "../runtime/state.h"
#include <algorithm>
#include <filesystem>
#include <cstdio>
#include <ctime>
#include <sstream>
#include <stdexcept>
namespace port {
using namespace cps2recomp;
using ReplayCommand = recomp::replay::Command;
struct NeoGeoReplay::Impl : recomp::replay::Board {
    NeoGeoPort& port;
    Replay record, replay;
    recomp::replay::Viewer viewer;
    bool recording=false,playing=false,failed=false;
    bool automatic=false,armed=true;
    uint32_t tail=0;
    std::string autoFolder;
    uint32_t at=0,recorded=0;
    uint64_t tick=0;
    std::string path,note,oldFeatures;
    bool oldOnline=false;
    bool oldPaused=false;
    int oldSlow=1;
    std::vector<uint8_t> before;
    std::map<uint32_t,std::vector<uint8_t>> keys;
    ReplayRoundClock rounds;
    explicit Impl(NeoGeoPort& p):port(p){}
    std::vector<uint8_t> Save() {
        std::vector<uint8_t> s;Archive ar(s,true);port.board->Serialize(ar);
        if(TheGame().state)TheGame().state(ar);
        return s;
    }
    bool Restore(std::vector<uint8_t> s) {
        // Structural pass uses the live schema without executing restore callbacks.
        Archive check(s,false,true);port.board->Serialize(check);
        if(TheGame().state)TheGame().state(check);
        if(!check.Complete()){note="Replay state layout does not match this game";return false;}
        Archive ar(s,false);port.board->Serialize(ar);if(TheGame().state)TheGame().state(ar);
        port.frame=port.board->Frames();return ar.Ok();
    }
    std::string Features() {
        std::string s;
        for(const auto& f:TheGame().features)s+=std::string(f.name)+"="+std::to_string(FeatureSetting(f.name))+",";
        return s;
    }
    bool SetFeatures(const std::string& text) {
        std::vector<std::pair<std::string,int>> parsed;
        std::istringstream in(text);std::string one;
        while(std::getline(in,one,',')) {
            const auto eq=one.find('=');if(eq==std::string::npos)return false;
            const auto name=one.substr(0,eq);char* end=nullptr;
            const auto value=strtol(one.c_str()+eq+1,&end,10);if(!end||*end)return false;
            auto f=std::find_if(TheGame().features.begin(),TheGame().features.end(),[&](const auto& f){return f.name==name;});
            if(f==TheGame().features.end()||value<0||value>=static_cast<long>(f->choices.empty()?2:f->choices.size()))return false;
            parsed.emplace_back(name,static_cast<int>(value));
        }
        if(parsed.size()!=TheGame().features.size())return false;
        for(size_t i=0;i<parsed.size();i++)for(size_t j=0;j<i;j++)if(parsed[i].first==parsed[j].first)return false;
        for(auto& [name,value]:parsed)SetFeatureValue(name.c_str(),value);
        return true;
    }
    uint32_t Frame() const override{return at;}
    uint32_t Length() const override{return static_cast<uint32_t>(replay.info.frames);}
    void RunFrame(bool,bool heard) override {
        if(failed)throw std::runtime_error(note);
        if(at>=Length())return;
        Replay::Pads pads{};auto it=replay.inputs.upper_bound(at);
        if(it!=replay.inputs.begin())pads=(--it)->second;
        port.RunFrame(pads.data());++at;
        const auto& hooks=TheNeoGeoGame();
        if(hooks.replayClock&&hooks.replayVersus&&rounds.Observe(hooks.replayClock(*port.board),hooks.replayVersus(*port.board))){viewer.AddMarker(at);fprintf(stderr,"replay: round marker %u\n",at);}
        if(!heard)port.board->Sound().made.clear();
        for(const auto& [frame,hash]:replay.checks)if(frame==at && hash!=port.board->StateHash()) {
            note="Replay checksum differs at frame "+std::to_string(at);failed=true;throw std::runtime_error(note);
        }
        if(at%600==0 && !keys.count(at)) {
            keys[at]=Save();
            if(keys.size()>32)keys.erase(std::next(keys.begin()));
        }
    }
    void Keep(std::vector<uint8_t>& state,bool) override{state=Save();}
    void PutBack(const std::vector<uint8_t>& state,uint32_t frame) override {
        if(Restore(state)){at=frame;if(TheNeoGeoGame().replayClock)rounds.Reset(TheNeoGeoGame().replayClock(*port.board));}else{failed=true;throw std::runtime_error(note);}
    }
    void GoBack(uint32_t frame) override {
        auto it=keys.upper_bound(frame);if(it!=keys.begin())--it;
        PutBack(it->second,it->first);
    }
};
NeoGeoReplay::NeoGeoReplay(NeoGeoPort& p):m(std::make_unique<Impl>(p)){}
NeoGeoReplay::~NeoGeoReplay(){StopRecord();Stop();}
bool NeoGeoReplay::Recording()const{return m->recording;}
bool NeoGeoReplay::Playing()const{return m->playing;}
const std::string& NeoGeoReplay::Note()const{return m->note;}
uint32_t NeoGeoReplay::Frame()const{return m->at;}
double NeoGeoReplay::AudioRate()const {
    if(!m->playing||!m->viewer.Heard())return 0;
    using S=recomp::replay::Speed;
    switch(m->viewer.CurrentSpeed()){case S::Quarter:return .25;case S::Half:return .5;case S::ThreeQuarters:return .75;default:return 1;}
}
bool NeoGeoReplay::Record(const std::string& path) {
    if(TheGame().inTraining&&TheGame().inTraining()){m->note="End training before recording a replay";return false;}
    if(m->playing||m->port.online){m->note="Manual recording requires offline play; online sessions record automatically";return false;}
    namespace fs=std::filesystem;std::error_code ec;
    auto rom=fs::weakly_canonical(fs::u8path(m->port.opt.rom),ec);
    if(!fs::is_directory(rom,ec))rom=rom.parent_path();
    auto output=fs::u8path(path).parent_path();if(output.empty())output=fs::current_path(ec);
    auto parent=fs::weakly_canonical(output,ec);
    for(auto current=parent;!current.empty();current=current.parent_path()) {
        if(fs::exists(current,ec)&&fs::equivalent(current,rom,ec)){m->note="Choose a replay folder outside the ROM folder";return false;}
        if(current==current.parent_path())break;
    }
    StopRecord();m->record=Replay{};auto& r=m->record;r.backend=0;
    r.info.game=GAME_ID;r.info.title=GAME_TITLE;r.info.backend="neogeo";
    r.info.identity=NEOGEO_REPLAY_IDENTITY;r.info.version=NEOGEO_RECOMP_VERSION;
    r.info.players={"Player 1","Player 2"};r.info.features=m->Features();
    char date[48]={};const auto now=std::time(nullptr);std::strftime(date,sizeof(date),"%Y-%m-%dT%H:%M:%S%z",std::localtime(&now));r.info.started=date;
    r.state=m->Save();m->path=path;m->recorded=0;m->automatic=false;m->recording=true;m->note="Recording replay";return true;
}
void NeoGeoReplay::Before(const uint16_t* pads) {
    if(m->playing)return;
    if(!m->autoFolder.empty()&&!m->port.online&&TheNeoGeoGame().replayVersus) {
        // (a training session is not a match: both players count as playing there)
        const bool versus=TheNeoGeoGame().replayVersus(*m->port.board)&&!(TheGame().inTraining&&TheGame().inTraining());
        if(!versus)m->armed=true;
        if(m->recording&&m->automatic&&versus&&m->tail)StopRecord();
        if(!m->recording&&versus&&m->armed) {
            m->armed=false;
            const auto path=std::filesystem::u8path(m->autoFolder)/(std::string(GAME_ID)+"_"+std::to_string(std::time(nullptr))+"_"+std::to_string(SDL_GetTicks())+".fareplay");
            if(Record(path.string())){m->automatic=true;m->armed=false;m->tail=0;}
        }
        if(m->recording&&m->automatic) {
            m->tail=versus?0:m->tail+1;
            if(m->tail>=120)StopRecord();
        }
    }
    if(!m->recording)return;
    if(m->Features()!=m->record.info.features){StopRecord();return;}
    const Replay::Pads p={pads[0],pads[1],0,0};
    if(m->record.inputs.empty()||m->record.inputs.rbegin()->second!=p)m->record.inputs[m->recorded]=p;
}
void NeoGeoReplay::After() {
    if(!m->recording||m->playing)return;
    ++m->recorded;if(m->recorded%60==0)m->record.checks.emplace_back(m->recorded,m->port.board->StateHash());
    if(m->recorded>=72000)StopRecord();
}
void NeoGeoReplay::AutoFolder(const std::string& path){m->autoFolder=path;m->armed=true;}
std::unique_ptr<Replay> NeoGeoReplay::NewOnline() {
    if(m->autoFolder.empty()||m->playing)return {};
    auto r=std::make_unique<Replay>();r->backend=0;
    auto& h=r->info;h.game=GAME_ID;h.title=GAME_TITLE;h.backend="neogeo";
    h.identity=NEOGEO_REPLAY_IDENTITY;h.version=NEOGEO_RECOMP_VERSION;
    h.features=m->Features();h.mode="online";h.online=true;
    const auto now=std::time(nullptr);char date[48]={};
    std::strftime(date,sizeof(date),"%Y-%m-%dT%H:%M:%S%z",std::localtime(&now));h.started=date;
    r->state=m->Save();return r;
}
bool NeoGeoReplay::SaveOnline(const Replay& replay) {
    // Reuse the same ROM-folder guard as manual recording without starting one.
    const auto path=(std::filesystem::u8path(m->autoFolder)/(std::string(GAME_ID)+"_online_"+std::to_string(std::time(nullptr))+"_"+std::to_string(SDL_GetTicks())+".fareplay")).string();
    namespace fs=std::filesystem;std::error_code ec;
    auto rom=fs::weakly_canonical(fs::u8path(m->port.opt.rom),ec);
    if(!fs::is_directory(rom,ec))rom=rom.parent_path();
    auto parent=fs::weakly_canonical(fs::u8path(path).parent_path(),ec);
    for(auto current=parent;!current.empty();current=current.parent_path()) {
        if(fs::exists(current,ec)&&fs::equivalent(current,rom,ec)){m->note="Choose a replay folder outside the ROM folder";return false;}
        if(current==current.parent_path())break;
    }
    fs::create_directories(fs::u8path(path).parent_path(),ec);
    std::string error;const bool saved=replay.Save(path,error);
    m->note=saved?"Online replay saved":error;
    fprintf(stderr,"online replay: %s (%llu frames)\n",saved?path.c_str():error.c_str(),static_cast<unsigned long long>(replay.info.frames));
    return saved;
}
void NeoGeoReplay::StopRecord() {
    if(!m->recording)return;
    m->recording=false;
    if(!m->recorded){m->note="No replay frames recorded";return;}
    m->record.info.frames=m->recorded;
    if(m->record.checks.empty()||m->record.checks.back().first!=m->recorded)m->record.checks.emplace_back(m->recorded,m->port.board->StateHash());
    std::error_code ec;const auto parent=std::filesystem::u8path(m->path).parent_path();if(!parent.empty())std::filesystem::create_directories(parent,ec);
    std::string error;if(!m->record.Save(m->path,error))m->note=error;else m->note="Replay saved";
}
bool NeoGeoReplay::Play(const std::string& path) {
    if(m->port.online){m->note="Stop online play before opening a replay";return false;}
    StopRecord();Stop();Replay candidate;std::string error;
    if(!candidate.Load(path,error)){m->note=error;return false;}
    if(candidate.info.format!=2||candidate.backend!=0||candidate.info.backend!="neogeo"||candidate.info.game!=GAME_ID||candidate.info.identity!=NEOGEO_REPLAY_IDENTITY) {
        m->note="Replay game/backend/gameplay identity differs from this build";return false;
    }
    if(candidate.players!=2||candidate.info.inputPlayers!=2||candidate.info.inputBytes!=2||!candidate.info.frames||candidate.info.frames>72000||candidate.inputs.empty()||candidate.inputs.begin()->first!=0) {
        m->note="Replay inputs or duration are invalid";return false;
    }
    uint64_t prev=0;for(const auto& [f,h]:candidate.checks){(void)h;if(!f||f<=prev||f>candidate.info.frames){m->note="Replay checksum positions are invalid";return false;}prev=f;}
    if(candidate.checks.size()!=(candidate.info.frames+59)/60){m->note="Replay checksum cadence is invalid";return false;}
    for(size_t i=0;i<candidate.checks.size();i++)if(candidate.checks[i].first!=std::min<uint64_t>((i+1)*60,candidate.info.frames)){m->note="Replay checksum cadence is invalid";return false;}
    if(candidate.checks.empty()||prev!=candidate.info.frames){m->note="Replay final checksum is missing";return false;}
    if(candidate.inputs.rbegin()->first>=candidate.info.frames){m->note="Replay input extends beyond its end";return false;}
    m->before=m->Save();m->oldFeatures=m->Features();m->oldOnline=TheGame().online;m->oldPaused=m->port.paused;m->oldSlow=m->port.menu.SlowBy();
    if(!m->SetFeatures(candidate.info.features)){m->note="Replay feature configuration differs";return false;}
    if(!m->Restore(candidate.state)){m->SetFeatures(m->oldFeatures);return false;}
    TheGame().online=candidate.info.online;m->replay=std::move(candidate);
    m->at=0;m->tick=0;m->failed=false;m->keys.clear();m->keys[0]=m->replay.state;
    m->playing=true;TheGame().replayPlayback=true;m->port.paused=false;m->port.menu.SetSlowBy(1);m->viewer.Begin(m->Length());m->viewer.AddMarker(0);
    if(TheNeoGeoGame().replayClock)m->rounds.Reset(TheNeoGeoGame().replayClock(*m->port.board));
    m->note.clear();return true;
}
void NeoGeoReplay::Stop(){if(!m->playing)return;m->playing=false;TheGame().replayPlayback=false;m->viewer.End();m->SetFeatures(m->oldFeatures);TheGame().online=m->oldOnline;m->Restore(m->before);m->port.paused=m->oldPaused;m->port.menu.SetSlowBy(m->oldSlow);m->keys.clear();}
bool NeoGeoReplay::Tick(){if(!m->playing)return false;++m->port.tick;try{return m->viewer.Tick(*m,++m->tick);}catch(const std::exception& e){const std::string note=e.what();Stop();m->note=note;return false;}}
void NeoGeoReplay::Command(ReplayCommand c){if(m->playing)m->viewer.Do(c,m->tick);}
void NeoGeoReplay::Seek(uint32_t f){if(m->playing)m->viewer.SeekTo(f,m->tick);}
void NeoGeoReplay::Script(uint64_t tick,const std::string& script) {
    std::istringstream in(script);std::string one;
    while(std::getline(in,one,';')) {
        const auto colon=one.find(':');if(colon==std::string::npos||strtoull(one.c_str(),nullptr,10)!=tick)continue;
        const auto name=one.substr(colon+1);
        if(name.rfind("seek ",0)==0){Seek(static_cast<uint32_t>(strtoul(name.c_str()+5,nullptr,10)));continue;}
        const std::pair<const char*,ReplayCommand> commands[]={{"pause",ReplayCommand::Pause},{"play",ReplayCommand::Play},{"faster",ReplayCommand::Faster},{"rewind",ReplayCommand::Rewind},{"slower",ReplayCommand::Slower},{"normal",ReplayCommand::Normal},{"back",ReplayCommand::StepBack},{"forward",ReplayCommand::StepForward},{"start",ReplayCommand::ToStart},{"end",ReplayCommand::ToEnd},{"jumpback",ReplayCommand::JumpBack},{"jumpforward",ReplayCommand::JumpForward},{"a",ReplayCommand::MarkA},{"b",ReplayCommand::MarkB},{"clear",ReplayCommand::ClearLoop},{"hud",ReplayCommand::ToggleHud}};
        for(const auto& [label,command]:commands)if(name==label)Command(command);
    }
}
void NeoGeoReplay::Draw(uint32_t* pixels,int w,int h){
    if(!m->playing)return;
    const recomp::text::Canvas canvas{pixels,w,h};
    m->viewer.DrawHud(canvas,m->at,m->Length(),m->tick);
    const auto name=[&](int p){return m->replay.info.players.size()>static_cast<size_t>(p)?m->replay.info.players[p].substr(0,12):"Player "+std::to_string(p+1);};
    const std::string left=name(0)+(m->replay.info.result?" "+std::to_string(m->replay.info.wins[0]):"");
    const std::string right=(m->replay.info.result?std::to_string(m->replay.info.wins[1])+" ":"")+name(1);
    canvas.Fill(0,0,w,10,{16,18,26,190});
    canvas.SmallText(2,1,{110,170,255,255},left.c_str());
    canvas.SmallText((w-recomp::text::SmallTextWidth("VS"))/2,1,{190,190,190,255},"VS");
    canvas.SmallText(w-recomp::text::SmallTextWidth(right.c_str())-2,1,{255,125,125,255},right.c_str());
}
bool NeoGeoReplay::Click(float x,float y) {
    if(!m->playing)return false;
    const auto hit=m->viewer.HitAt(x,y,NeoGeoVideo::kWidth,NeoGeoVideo::kHeight,m->Length());
    using H=recomp::replay::Viewer::Hit;
    if(hit.kind==H::Do)Command(hit.command);else if(hit.kind==H::Seek)Seek(hit.frame);
    return hit.kind!=H::None;
}
bool NeoGeoReplay::Event(const SDL_Event& e) {
    if(!m->playing)return false;
    using C=ReplayCommand;
    if(e.type==SDL_EVENT_MOUSE_MOTION){m->viewer.Touched(m->tick);return false;}
    if(e.type==SDL_EVENT_KEY_DOWN && !e.key.repeat){
        C c;switch(e.key.scancode){
        case SDL_SCANCODE_SPACE:case SDL_SCANCODE_DOWN:case SDL_SCANCODE_P:c=C::PlayPause;break;
        case SDL_SCANCODE_LEFT:c=C::Rewind;break;case SDL_SCANCODE_RIGHT:c=C::Faster;break;
        case SDL_SCANCODE_UP:c=C::Slower;break;case SDL_SCANCODE_COMMA:c=C::StepBack;break;
        case SDL_SCANCODE_PERIOD:case SDL_SCANCODE_O:c=C::StepForward;break;case SDL_SCANCODE_LEFTBRACKET:c=C::JumpBack;break;
        case SDL_SCANCODE_RIGHTBRACKET:c=C::JumpForward;break;case SDL_SCANCODE_HOME:c=C::ToStart;break;
        case SDL_SCANCODE_END:c=C::ToEnd;break;case SDL_SCANCODE_A:c=C::MarkA;break;
        case SDL_SCANCODE_B:c=C::MarkB;break;case SDL_SCANCODE_C:c=C::ClearLoop;break;
        case SDL_SCANCODE_F:c=C::FrameCounter;break;case SDL_SCANCODE_V:c=C::ToggleHud;break;
        case SDL_SCANCODE_PAGEUP:c=C::PreviousMarker;break;case SDL_SCANCODE_PAGEDOWN:c=C::NextMarker;break;
        default:return false;}Command(c);return true;
    }
    if(e.type==SDL_EVENT_GAMEPAD_BUTTON_DOWN){
        C c;switch(e.gbutton.button){case SDL_GAMEPAD_BUTTON_SOUTH:case SDL_GAMEPAD_BUTTON_DPAD_DOWN:c=C::PlayPause;break;
        case SDL_GAMEPAD_BUTTON_DPAD_LEFT:c=C::Rewind;break;case SDL_GAMEPAD_BUTTON_DPAD_RIGHT:c=C::Faster;break;
        case SDL_GAMEPAD_BUTTON_DPAD_UP:c=C::Slower;break;case SDL_GAMEPAD_BUTTON_LEFT_SHOULDER:c=C::JumpBack;break;
        case SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER:c=C::JumpForward;break;case SDL_GAMEPAD_BUTTON_BACK:c=C::ToggleHud;break;
        case SDL_GAMEPAD_BUTTON_WEST:c=C::StepBack;break;case SDL_GAMEPAD_BUTTON_EAST:c=C::StepForward;break;
        case SDL_GAMEPAD_BUTTON_NORTH:c=C::NextMarker;break;case SDL_GAMEPAD_BUTTON_LEFT_STICK:c=C::PreviousMarker;break;
        case SDL_GAMEPAD_BUTTON_RIGHT_STICK:c=C::FrameCounter;break;case SDL_GAMEPAD_BUTTON_START:c=C::Normal;break;
        default:return false;}Command(c);return true;
    }
    return false;
}
}

// SPDX-License-Identifier: GPL-3.0-only
// Adapted from the ForkAde CPS controller; GPL-3.0-only, same contributors.
#pragma once
#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>
namespace port {
// Pure same-peer reset barrier. The caller validates peer address and room nonce.
class Recovery {
public:
    struct Action {
        bool accepted=false, reset=false, ready=false, expired=false, left=false;
        char reply=0;
        uint64_t epoch=0;
    };
    enum class Stage { Running, Offer, Ready, Request, Ack, Ended };
    void Reset(bool host, uint64_t now) { host_=host;stage_=Stage::Running;epoch_=1;target_=deadline_=0;next_=now+250; }
    uint64_t Epoch() const { return epoch_; }
    bool Waiting() const { return stage_!=Stage::Running && stage_!=Stage::Ended; }
    uint64_t Deadline() const { return deadline_; }
    void Complete() { if(stage_==Stage::Running)deadline_=0; }
    Action Begin(uint64_t now) {
        if(stage_==Stage::Ended)return {};
        if(Waiting())return {};
        if(epoch_==std::numeric_limits<uint64_t>::max()){stage_=Stage::Ended;Action a;a.expired=true;return a;}
        if(!deadline_)deadline_=now+60000;
        stage_=host_?Stage::Offer:Stage::Request;target_=host_?epoch_+1:0;
        return Reply(host_?'R':'N',host_?target_:epoch_,now);
    }
    Action Tick(uint64_t now) {
        if(auto a=Expire(now);a.expired)return a;
        if(stage_==Stage::Ended || now<next_)return {};
        switch(stage_) {
        case Stage::Offer:return Reply('R',target_,now);
        case Stage::Ready:return Reply('S',epoch_,now);
        case Stage::Request:return Reply('N',epoch_,now);
        case Stage::Ack:return Reply('K',target_,now);
        default:return Reply('P',epoch_,now);
        }
    }
    Action Receive(char kind,uint64_t epoch,uint64_t now) {
        if(auto a=Expire(now);a.expired)return a;
        if(stage_==Stage::Ended)return {};
        if(kind=='L' && (epoch==epoch_ || (Waiting()&&target_&&epoch==target_))) {
            stage_=Stage::Ended;Action a;a.accepted=a.left=true;return a;
        }
        if(kind=='P' && epoch==epoch_){Action a;a.accepted=true;return a;}
        if(host_) {
            if(kind=='N' && epoch==epoch_) {
                if(stage_==Stage::Running)return Begin(now);
                if(stage_==Stage::Offer)return Reply('R',target_,now);
                if(stage_==Stage::Ready){stage_=Stage::Running;return Begin(now);}
            }
            if(kind=='K' && stage_==Stage::Offer && epoch==target_) {
                epoch_=target_;stage_=Stage::Ready;auto a=Reply('S',epoch_,now);a.reset=true;return a;
            }
            if(kind=='K' && stage_==Stage::Ready && epoch==epoch_)return Reply('S',epoch_,now);
            if(kind=='T' && stage_==Stage::Ready && epoch==epoch_) {
                stage_=Stage::Running;next_=now+250;Action a;a.accepted=a.ready=true;return a;
            }
        } else {
            if(kind=='R' && epoch_!=std::numeric_limits<uint64_t>::max() && epoch==epoch_+1 &&
               (stage_==Stage::Running || stage_==Stage::Request || stage_==Stage::Ack)) {
                if(!deadline_)deadline_=now+60000;
                target_=epoch;stage_=Stage::Ack;return Reply('K',target_,now);
            }
            if(kind=='R' && epoch==epoch_ && stage_==Stage::Running)return Reply('T',epoch_,now);
            if(kind=='S' && stage_==Stage::Ack && epoch==target_) {
                epoch_=target_;stage_=Stage::Running;
                auto a=Reply('T',epoch_,now);a.reset=a.ready=true;return a;
            }
            if(kind=='S' && stage_==Stage::Running && epoch==epoch_)return Reply('T',epoch_,now);
        }
        return {};
    }
    static void Put(std::vector<uint8_t>& bytes,uint64_t value) { for(int n=7;n>=0;--n)bytes.push_back(static_cast<uint8_t>(value>>(n*8))); }
    static uint64_t Get(const uint8_t* bytes) { uint64_t n=0;for(int i=0;i<8;++i)n=(n<<8)|bytes[i];return n; }
private:
    bool host_=false;
    Stage stage_=Stage::Running;
    uint64_t epoch_=1,target_=0,deadline_=0,next_=0;
    Action Reply(char kind,uint64_t epoch,uint64_t now) { next_=now+250;Action a;a.accepted=true;a.reply=kind;a.epoch=epoch;return a; }
    Action Expire(uint64_t now) {
        if(deadline_ && now>=deadline_){stage_=Stage::Ended;deadline_=0;Action a;a.expired=true;return a;}
        return {};
    }
};
}

// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "../recomp-common/replay_viewer.h"
#include <memory>
#include <string>
#include <SDL3/SDL.h>
namespace port {
struct NeoGeoPort;
struct Replay;
class NeoGeoReplay {
public:
    explicit NeoGeoReplay(NeoGeoPort& port);
    ~NeoGeoReplay();
    bool Record(const std::string& path);
    bool Play(const std::string& path);
    void StopRecord();
    void Stop();
    bool Recording() const;
    bool Playing() const;
    void Before(const uint16_t* inputs);
    void After();
    bool Tick();
    bool Event(const SDL_Event& event);
    bool Click(float x,float y);
    void Draw(uint32_t* pixels,int width,int height);
    void Command(recomp::replay::Command command);
    void Seek(uint32_t frame);
    void Script(uint64_t tick, const std::string& script);
    void AutoFolder(const std::string& path);
    uint32_t Frame() const;
    double AudioRate() const;
    const std::string& Note() const;
    std::unique_ptr<Replay> NewOnline();
    bool SaveOnline(const Replay& replay);
private:
    struct Impl;
    std::unique_ptr<Impl> m;
};
}

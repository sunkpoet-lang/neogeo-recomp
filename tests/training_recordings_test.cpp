// SPDX-License-Identifier: GPL-3.0-only
#include "training_recordings.h"
#include <cstdlib>
using cps2recomp::Archive;
using Slots=cps2recomp::training::Recordings;
void check(bool b){if(!b)std::abort();}
int main(){
    Slots a;check(!a.Play(0,Slots::Once)&&!a.Play(0,Slots::Random));
    check(a.Record(0));check(a.Capture(0x1514,true)==0x14);a.Capture(0x26,true);a.Stop();
    check(a.Length(0)==2&&a.Play(0,Slots::Once));
    check(a.Next(false)==0x18);check(a.Next(true)==0x26);check(!a.Playing()&&!a.Next(false));
    check(!a.Play(1,Slots::Loop)&&a.Length(0)==2);
    a.Record(2);a.Capture(0x40,false);a.Stop();check(a.Play(0,Slots::Loop));
    for(int i=0;i<100;++i)check(a.Next(false)==(i%2?0x2a:0x18));
    a.Stop();a.Record(0);a.Capture(0x10,false);a.Stop();check(a.Length(0)==1&&a.Length(2)==1);
    check(a.Play(1,Slots::Random));bool one=false,three=false;
    for(int i=0;i<100;++i){auto pad=a.Next(false);check(pad==0x10||pad==0x40);one|=pad==0x10;three|=pad==0x40;}
    check(one&&three);
    std::vector<uint8_t> bytes;Archive save(bytes,true);a.Serialize(save);check(save.Ok());
    Slots b;Archive pre(bytes,false,true);b.Serialize(pre);check(pre.Complete()&&b.Length(0)==0);
    Archive load(bytes,false);b.Serialize(load);check(load.Complete());
    for(int i=0;i<300;++i)check(a.Next(i%7==0)==b.Next(i%7==0));check(a.RandomState()==b.RandomState());
    a.ResetInteraction();check(!a.Playing()&&!a.Recording()&&a.Length(2)==1&&a.RandomState()==Slots::Seed);
    a.Record(1);for(size_t i=0;i<Slots::Limit+5;++i)a.Capture(0xffff,false);
    check(!a.Recording()&&a.Length(1)==Slots::Limit);check(a.Play(1,Slots::Once));
    for(size_t i=0;i<Slots::Limit;++i)check(a.Next(false)==0xff);
    check(!a.Playing());a.Clear(1);check(a.Length(1)==0&&a.Length(2)==1);
    // Every truncation must fail bounded structural preflight without allocating.
    for(size_t n=0;n<bytes.size();++n){std::vector<uint8_t> cut(bytes.begin(),bytes.begin()+n);Slots c;Archive ar(cut,false,true);c.Serialize(ar);check(!ar.Complete()&&c.Length(0)==0);}
}

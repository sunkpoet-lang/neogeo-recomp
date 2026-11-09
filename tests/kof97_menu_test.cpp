// SPDX-License-Identifier: GPL-3.0-only
#include "../port/menu.h"
#include "../runtime/game.h"
#include <cassert>
namespace cps2recomp {
Game& TheGame(){static Game game;return game;}
int FeatureSetting(const char*){return 0;}
void SetFeatureValue(const char*,int){}
int SlowMotion(){return 1;}
}
int main(){
 port::Menu menu;menu.forkade=true;
 assert(menu.SelectOnline(0)==port::Menu::Action::Host);menu.Close();
 assert(menu.SelectOnline(1)==port::Menu::Action::CopyCode);menu.Close();
 assert(menu.SelectOnline(2)==port::Menu::Action::HideNames);menu.Close();
 assert(menu.SelectOnline(2)==port::Menu::Action::HideNames);menu.Close();
 assert(menu.SelectOnline(3)==port::Menu::Action::HideStreaks);menu.Close();
 assert(menu.SelectOnline(0)==port::Menu::Action::Host);
}

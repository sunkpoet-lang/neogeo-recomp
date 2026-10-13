// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of recomp-common: https://github.com/tmcleroy/recomp-common
// The shared in-game menu (menu.h) for hosts written in C. The same menu, the same rules; see menu.h.
// The host links menu.cpp and menu_c.cpp (C++17) with the rest of its C.
#ifndef RECOMP_COMMON_MENU_C_H
#define RECOMP_COMMON_MENU_C_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct RMenu RMenu;

// The menu's own buttons (rmenu_buttons)
enum {
  RMENU_UP = 1,
  RMENU_DOWN = 2,
  RMENU_LEFT = 4,
  RMENU_RIGHT = 8,
  RMENU_CONFIRM = 16,  // chooses a row, changes it, goes into a page
  RMENU_BACK = 32,     // leaves a page; on the first page, closes the menu
};

typedef enum {
  RMENU_ROW_SAVE,    // "Save state": rmenu_buttons returns RMENU_SAVE
  RMENU_ROW_LOAD,    // "Load state": RMENU_LOAD
  RMENU_ROW_SLOT,    // the slot (1-9) those two use
  RMENU_ROW_SPEED,   // one of the config's speeds (left out when there are none)
  RMENU_ROW_PAGES,   // the commands of no category, then a row for each category's page
  RMENU_ROW_ACTION,  // the host's own: RMENU_ACTION with its id
  RMENU_ROW_QUIT,    // "Quit game": RMENU_QUIT
  RMENU_ROW_RESUME,  // closes the menu
  RMENU_ROW_MUTE,    // "Sound": on or muted, through the config's muted and setMuted (left out without them)
} RMenuRowKind;

// A row of the first page. Rows with the same `group` are together; a line is drawn where it changes.
typedef struct {
  RMenuRowKind kind;
  int group;
  int id;                   // RMENU_ROW_ACTION: handed back when chosen
  const char* title;        // RMENU_ROW_ACTION
  const char* description;  // RMENU_ROW_ACTION
} RMenuRow;

typedef struct {
  const char* category;        // the page it is on
  const char* title;
  const char* description;
  const char* const* choices;  // NULL: on (1) or off (0). Else names ended by NULL, the first meaning off.
} RMenuFeature;

typedef struct {
  const char* category;  // the page it is on ("": the first page)
  const char* title;
  const char* description;
} RMenuCommand;

// Everything is copied by rmenu_configure; only the two functions and `user` are kept.
typedef struct {
  const RMenuRow* rows;
  int rowCount;
  const RMenuFeature* features;
  int featureCount;
  const RMenuCommand* commands;
  int commandCount;
  // a feature's value now (by its place in `features`), and changing it
  int (*value)(void* user, int feature);
  void (*setValue)(void* user, int feature, int value);
  void* user;
  // the Speed row: the speeds' names (the first being full speed) and the row's description
  const char* const* speeds;
  int speedCount;
  const char* speedDescription;
  // what the menu's buttons are called on this system, for the hint line (NULL: "a button", "Esc")
  const char* confirmName;
  const char* closeName;
  // the Mute row: whether the game's sound is off now (not 0), and switching it off and on
  int (*muted)(void* user);
  void (*setMuted)(void* user, int muted);
} RMenuConfig;

typedef enum { RMENU_NONE, RMENU_SAVE, RMENU_LOAD, RMENU_QUIT, RMENU_ACTION, RMENU_COMMAND } RMenuChosen;

RMenu* rmenu_new(void);
void rmenu_free(RMenu* menu);
void rmenu_configure(RMenu* menu, const RMenuConfig* config);
void rmenu_open(RMenu* menu);
void rmenu_close(RMenu* menu);
bool rmenu_isOpen(const RMenu* menu);
// Buttons newly pressed (RMENU_... bits). Returns what the host is to do; *id (may be NULL) is the row's id
// for RMENU_ACTION and the command's place for RMENU_COMMAND. The host closes the menu if that went well.
RMenuChosen rmenu_buttons(RMenu* menu, unsigned pressed, int* id);
int rmenu_slot(const RMenu* menu);
void rmenu_setSlot(RMenu* menu, int slot);
// the speed chosen, by its place in the config's speeds (0: full speed)
int rmenu_speed(const RMenu* menu);
void rmenu_setSpeed(RMenu* menu, int speed);
// a line to tell the player (e.g. why not), until the next press; copied
void rmenu_note(RMenu* menu, const char* text);
// over a picture of 32-bit pixels (red in the low byte)
void rmenu_draw(const RMenu* menu, uint32_t* pixels, int width, int height);

#ifdef __cplusplus
}
#endif

#endif

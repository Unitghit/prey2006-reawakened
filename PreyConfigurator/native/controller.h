#pragma once
#include "settings.h"

// Controller bindings live in the game's own configuration (userdata/base/prey06.cfg)
// as engine key names, e.g. bind "JOY_BTN_SOUTH" "_moveup", so the launcher and
// in-game rebinding share one set. Keys map to commands; a command may have several keys.
using PadBindings = std::map<std::string, std::string>;

// The layout the game applies when a configuration has no controller bindings.
PadBindings DefaultPadBindings();
// Controller bindings from the configuration, or the default layout when it has none.
PadBindings ReadPadBindings(const fs::path& root, bool* isDefault = nullptr);
// Replaces every controller binding in the configuration, keeping all other lines.
void WritePadBindings(const fs::path& root, const PadBindings& bindings);

// Modal controller window: shows the connected controller and lets each action be
// assigned by pressing a button on it. Returns true when bindings were saved.
bool RunControllerDialog(HINSTANCE instance, HWND owner, HFONT font, const fs::path& root);
// Self-test for --verify (configuration editing only; no controller needed).
void VerifyControllerBindings(const fs::path& output);

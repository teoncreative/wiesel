//
//   Copyright 2026 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//

#pragma once

// Codicon icons - codepoints from codicon-mappings.json
// Generated with: python compute_utf8.py
#define CODICON_ADD "\xee\xa9\xa0"              // 60000 add
#define CODICON_SEARCH "\xee\xa9\xad"           // 60013 search
#define CODICON_EYE "\xee\xa9\xb0"              // 60016 eye
#define CODICON_EDIT "\xee\xa9\xb3"             // 60019 edit
#define CODICON_INFO "\xee\xa9\xb4"             // 60020 info
#define CODICON_CLOSE "\xee\xa9\xb6"            // 60022 close
#define CODICON_FILE "\xee\xa9\xbb"             // 60027 file
#define CODICON_ELLIPSIS "\xee\xa9\xbc"         // 60028 ellipsis
#define CODICON_REPLY "\xee\xa9\xbd"            // 60029 reply
#define CODICON_TRASH "\xee\xaa\x81"            // 60033 trash
#define CODICON_HISTORY "\xee\xaa\x82"          // 60034 history
#define CODICON_TERMINAL "\xee\xaa\x85"         // 60037 terminal
#define CODICON_SYMBOL_RULER "\xee\xaa\x96"     // 60054 symbol-ruler
#define CODICON_BROWSER "\xee\xaa\xae"          // 60078 browser
#define CODICON_DASHBOARD "\xee\xab\x8d"        // 60109 dashboard
#define CODICON_PAUSE "\xee\xab\x91"            // 60113 debug-pause
#define CODICON_STOP "\xee\xab\x97"             // 60119 debug-stop
#define CODICON_CAMERA_VIDEO "\xee\xab\x99"     // 60121 device-camera-video
#define CODICON_DISCARD "\xee\xab\xa2"          // 60130 discard
#define CODICON_EYE_CLOSED "\xee\xab\xa7"       // 60135 eye-closed
#define CODICON_FOLDER_OPENED "\xee\xab\xb7"    // 60151 folder-opened
#define CODICON_GLOBE "\xee\xac\x81"            // 60161 globe
#define CODICON_HOME "\xee\xac\x86"             // 60166 home
#define CODICON_MOVE "\xee\xac\xa2"             // 60194 move
#define CODICON_PLAY "\xee\xac\xac"             // 60204 play
#define CODICON_PREVIEW "\xee\xac\xaf"          // 60207 preview
#define CODICON_REFRESH "\xee\xac\xb7"          // 60215 refresh
#define CODICON_SAVE_ALL "\xee\xad\x89"         // 60233 save-all
#define CODICON_SAVE "\xee\xad\x8b"             // 60235 save
#define CODICON_SETTINGS_GEAR "\xee\xad\x91"    // 60241 settings-gear
#define CODICON_SYMBOL_PROPERTY "\xee\xad\xa5"  // 60261 symbol-property
#define CODICON_REDO "\xee\xae\xb0"             // 60336 redo
#define CODICON_INSPECT "\xee\xaf\x91"          // 60369 inspect
#define CODICON_LAYOUT "\xee\xaf\xab"           // 60395 layout
#define CODICON_ARROW_BOTH "\xee\xaa\x99"       // 60057 arrow-both

namespace Wiesel::Editor {

// Initialize the editor icon font system. Call once in EditorLayer::OnAttach
// after all other fonts have been added.
void InitEditorIcons();

}  // namespace Wiesel::Editor

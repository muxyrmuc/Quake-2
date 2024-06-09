/*
Copyright (C) 1997-2001 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/
// sys_win.h

#include "../qcommon/qcommon.h"
#include "winquake.h"
#include <errno.h>
#include <float.h>
#include <fcntl.h>
#include <stdio.h>
#include <thread>

#include "../game/game.h"

#include <SDL_messagebox.h>
#include <SDL_clipboard.h>
#include <SDL_events.h>

#define MINIMUM_WIN_MEMORY 0x0a00000
#define MAXIMUM_WIN_MEMORY 0x1000000

int starttime;
bool ActiveApp;
bool Minimized;

unsigned sys_msg_time;
unsigned sys_frame_time;

#define MAX_NUM_ARGVS 128
int argc;
char** argv;

/*
===============================================================================

SYSTEM IO

===============================================================================
*/

void Sys_Error(const char* error, ...) {
    va_list argptr;
    char text[1024];

    CL_Shutdown();
    Qcommon_Shutdown();

    va_start(argptr, error);
    vsprintf(text, error, argptr);
    va_end(argptr);

    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", text, nullptr);

    exit(1);
}

void Sys_Quit(void) {
    CL_Shutdown();
    Qcommon_Shutdown();

    exit(0);
}

//================================================================

/*
================
Sys_Init
================
*/
void Sys_Init(void) {
}

static char console_text[256];
static int console_textlen;

/*
================
Sys_ConsoleInput
================
*/
char* Sys_ConsoleInput(void) {
    return NULL;
}

/*
================
Sys_ConsoleOutput

Print text to the dedicated console
================
*/
void Sys_ConsoleOutput(char* string) {
}

/*
================
Sys_SendKeyEvents

Send Key_Event calls
================
*/
void Sys_SendKeyEvents(void) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT) {
            Sys_Quit();
        }
        sys_msg_time = event.common.timestamp;

        MainWndProc();
    }

    // grab frame time
    sys_frame_time = Sys_NowMilliseconds();  // FIXME: should this be at start?
}

/*
================
Sys_GetClipboardData

================
*/
char* Sys_GetClipboardData(void) {
    char* data = nullptr;

    if (!SDL_HasClipboardText()) {

    }

    char* cliptext = SDL_GetClipboardText();
    if (cliptext[0] != '\0') {
        data = static_cast<char*>(malloc(strlen(cliptext) + 1));
        strcpy(data, cliptext);
    }
    SDL_free(cliptext);

    return data;
}

/*
==============================================================================

 WINDOWS CRAP

==============================================================================
*/

/*
=================
Sys_AppActivate
=================
*/
void Sys_AppActivate(void) {
    SDL_ShowWindow(cl_hwnd);
    SDL_RaiseWindow(cl_hwnd);
}

/*
========================================================================

GAME DLL

========================================================================
*/

/*
=================
Sys_UnloadGame
=================
*/
void Sys_UnloadGame(void) {
    // don't think we need FreeLibrary anymore
}

/*
=================
Sys_GetGameAPI

Loads the game dll
=================
*/
void* Sys_GetGameAPI(void* parms) {
    // don't think we need LoadLibrary anymore
    return GetGameApi(static_cast<game_import_t*>(parms));
}

//=======================================================================

/*
==================
WinMain

==================
*/
HINSTANCE global_hInstance;

int main(int ac, char** av /*HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow*/) {
    MSG msg;
    int time, oldtime, newtime;

    global_hInstance = hInstance;

    argc = ac;
    argv = av;

    Qcommon_Init(argc, argv);
    oldtime = Sys_Milliseconds();

    /* main window message loop */
    while (1) {
        // if at a full screen console, don't update unless needed
        if (Minimized) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        while (PeekMessage(&msg, NULL, 0, 0, PM_NOREMOVE)) {
            if (!GetMessage(&msg, NULL, 0, 0))
                Com_Quit();
            sys_msg_time = msg.time;
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        do {
            newtime = Sys_Milliseconds();
            time = newtime - oldtime;
        } while (time < 1);
        //			Con_Printf ("time:%5.2f - %5.2f = %5.2f\n", newtime, oldtime, time);

        //	_controlfp( ~( _EM_ZERODIVIDE /*| _EM_INVALID*/ ), _MCW_EM );

        // TODO: commented because _MCW_PC is not supported for x86-64
        // _controlfp( _PC_24, _MCW_PC );
        Qcommon_Frame(time);

        oldtime = newtime;
    }

    // never gets here
    return 0;
}

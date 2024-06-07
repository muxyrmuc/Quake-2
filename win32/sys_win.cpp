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

#include <SDL_messagebox.h>
#include <SDL_clipboard.h>

#define MINIMUM_WIN_MEMORY 0x0a00000
#define MAXIMUM_WIN_MEMORY 0x1000000

int starttime;
qboolean ActiveApp;
qboolean Minimized;

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
    MSG msg;

    while (PeekMessage(&msg, NULL, 0, 0, PM_NOREMOVE)) {
        if (!GetMessage(&msg, NULL, 0, 0))
            Sys_Quit();
        sys_msg_time = msg.time;
        TranslateMessage(&msg);
        DispatchMessage(&msg);
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
    ShowWindow(cl_hwnd, SW_RESTORE);
    SetForegroundWindow(cl_hwnd);
}

/*
========================================================================

GAME DLL

========================================================================
*/

static HINSTANCE game_library;

/*
=================
Sys_UnloadGame
=================
*/
void Sys_UnloadGame(void) {
    if (!FreeLibrary(game_library))
        Com_Error(ERR_FATAL, "FreeLibrary failed for game library");
    game_library = NULL;
}

/*
=================
Sys_GetGameAPI

Loads the game dll
=================
*/
void* Sys_GetGameAPI(void* parms) {
    void* (*GetGameAPI)(void*);
    char name[MAX_OSPATH];
    char* path;
    char cwd[MAX_OSPATH];

    const char* gamename = "game.dll";

#ifdef NDEBUG
    const char* debugdir = "release";
#else
    const char* debugdir = "debug";
#endif

    if (game_library)
        Com_Error(ERR_FATAL, "Sys_GetGameAPI without Sys_UnloadingGame");

    // check the current debug directory first for development purposes
    _getcwd(cwd, sizeof(cwd));
    Com_sprintf(name, sizeof(name), "%s/%s/%s", cwd, debugdir, gamename);
    game_library = LoadLibrary(name);
    if (game_library) {
        Com_DPrintf("LoadLibrary (%s)\n", name);
    } else {
        // check the current directory for other development purposes
        Com_sprintf(name, sizeof(name), "%s/%s", cwd, gamename);
        game_library = LoadLibrary(name);
        if (game_library) {
            Com_DPrintf("LoadLibrary (%s)\n", name);
        } else {
            // now run through the search paths
            path = NULL;
            while (1) {
                path = FS_NextPath(path);
                if (!path)
                    return NULL;  // couldn't find one anywhere
                Com_sprintf(name, sizeof(name), "%s/%s", path, gamename);
                game_library = LoadLibrary(name);
                if (game_library) {
                    Com_DPrintf("LoadLibrary (%s)\n", name);
                    break;
                }
            }
        }
    }

    GetGameAPI = (decltype(GetGameAPI))GetProcAddress(game_library, "GetGameAPI");
    if (!GetGameAPI) {
        Sys_UnloadGame();
        return NULL;
    }

    return GetGameAPI(parms);
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

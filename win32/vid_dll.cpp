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
// Main windowed and fullscreen graphics interface module. This module
// is used for both the software and OpenGL rendering versions of the
// Quake refresh engine.
#include <assert.h>
#include <float.h>

#include <SDL_messagebox.h>
#include <SDL_loadso.h>
#include <SDL_events.h>

#include "../client/client.h"
#include "winquake.h"
//#include "zmouse.h"

// Structure containing functions exported from refresh DLL
refexport_t re;

// Console variables that we need to access from this module
cvar_t* vid_gamma;
cvar_t* vid_ref;   // Name of Refresh DLL loaded
cvar_t* vid_xpos;  // X coordinate of window position
cvar_t* vid_ypos;  // Y coordinate of window position
cvar_t* vid_fullscreen;

// Global variables used internally by this module
viddef_t viddef;           // global video state; used by other modules
void* reflib_library;  // Handle to refresh DLL
qboolean reflib_active = kFalse;

SDL_Window* cl_hwnd;  // Main window handle for life of program; used to be HWND

#define VID_NUM_MODES (sizeof(vid_modes) / sizeof(vid_modes[0]))

void MainWndProc(const SDL_Event& event);

extern unsigned sys_msg_time;

/*
==========================================================================

DLL GLUE

==========================================================================
*/

#define MAXPRINTMSG 4096
void VID_Printf(int print_level, const char* fmt, ...) {
    va_list argptr;
    char msg[MAXPRINTMSG];

    va_start(argptr, fmt);
    vsprintf(msg, fmt, argptr);
    va_end(argptr);

    if (print_level == PRINT_ALL) {
        Com_Printf("%s", msg);
    } else if (print_level == PRINT_DEVELOPER) {
        Com_DPrintf("%s", msg);
    } else if (print_level == PRINT_ALERT) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_WARNING, "PRINT_ALERT", msg, nullptr);
    }
}

void VID_Error(int err_level, const char* fmt, ...) {
    va_list argptr;
    char msg[MAXPRINTMSG];

    va_start(argptr, fmt);
    vsprintf(msg, fmt, argptr);
    va_end(argptr);

    Com_Error(err_level, "%s", msg);
}

//==========================================================================

byte scantokey[128] =
    {
        //  0           1       2       3       4       5       6       7
        //  8           9       A       B       C       D       E       F
        0, 27, '1', '2', '3', '4', '5', '6',
        '7', '8', '9', '0', '-', '=', K_BACKSPACE, 9,  // 0
        'q', 'w', 'e', 'r', 't', 'y', 'u', 'i',
        'o', 'p', '[', ']', 13, K_CTRL, 'a', 's',  // 1
        'd', 'f', 'g', 'h', 'j', 'k', 'l', ';',
        '\'', '`', K_SHIFT, '\\', 'z', 'x', 'c', 'v',  // 2
        'b', 'n', 'm', ',', '.', '/', K_SHIFT, '*',
        K_ALT, ' ', 0, K_F1, K_F2, K_F3, K_F4, K_F5,  // 3
        K_F6, K_F7, K_F8, K_F9, K_F10, K_PAUSE, 0, K_HOME,
        K_UPARROW, K_PGUP, K_KP_MINUS, K_LEFTARROW, K_KP_5, K_RIGHTARROW, K_KP_PLUS, K_END,  // 4
        K_DOWNARROW, K_PGDN, K_INS, K_DEL, 0, 0, 0, K_F11,
        K_F12, 0, 0, 0, 0, 0, 0, 0,  // 5
        0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0,  // 6
        0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0  // 7
};

/*
=======
MapKey

Map from windows to quake keynums
=======
*/
int MapKey(int key) {
    int result;
    int modified = (key >> 16) & 255;
    qboolean is_extended = kFalse;

    if (modified > 127)
        return 0;

    if (key & (1 << 24))
        is_extended = kTrue;

    result = scantokey[modified];

    if (!is_extended) {
        switch (result) {
            case K_HOME:
                return K_KP_HOME;
            case K_UPARROW:
                return K_KP_UPARROW;
            case K_PGUP:
                return K_KP_PGUP;
            case K_LEFTARROW:
                return K_KP_LEFTARROW;
            case K_RIGHTARROW:
                return K_KP_RIGHTARROW;
            case K_END:
                return K_KP_END;
            case K_DOWNARROW:
                return K_KP_DOWNARROW;
            case K_PGDN:
                return K_KP_PGDN;
            case K_INS:
                return K_KP_INS;
            case K_DEL:
                return K_KP_DEL;
            default:
                return result;
        }
    } else {
        switch (result) {
            case 0x0D:
                return K_KP_ENTER;
            case 0x2F:
                return K_KP_SLASH;
            case 0xAF:
                return K_KP_PLUS;
        }
        return result;
    }
}

void AppActivate(bool fActive, bool minimize) {
    Minimized = minimize ? true : false;

    Key_ClearStates();

    // we don't want to act like we're active if we're minimized
    if (fActive && !Minimized)
        ActiveApp = true;
    else
        ActiveApp = false;

    // minimize/restore mouse-capture on demand
    if (false == ActiveApp) {
        IN_Activate(false);
        CDAudio_Activate(kFalse);
        S_Activate(kFalse);
    } else {
        IN_Activate(true);
        CDAudio_Activate(kTrue);
        S_Activate(kTrue);
    }
}

/*
====================
MainWndProc

main window procedure
====================
*/
void MainWndProc(const SDL_Event& event) {
    switch (event.type) {
        case SDL_MOUSEWHEEL:
            break;

    }




    LONG lRet = 0;

    if (uMsg == MSH_MOUSEWHEEL) {
        if (((int)wParam) > 0) {
            Key_Event(K_MWHEELUP, kTrue, sys_msg_time);
            Key_Event(K_MWHEELUP, kFalse, sys_msg_time);
        } else {
            Key_Event(K_MWHEELDOWN, kTrue, sys_msg_time);
            Key_Event(K_MWHEELDOWN, kFalse, sys_msg_time);
        }
        return DefWindowProc(hWnd, uMsg, wParam, lParam);
    }

    switch (uMsg) {
        case WM_MOUSEWHEEL:
            /*
            ** this chunk of code theoretically only works under NT4 and Win98
            ** since this message doesn't exist under Win95
            */
            if ((short)HIWORD(wParam) > 0) {
                Key_Event(K_MWHEELUP, kTrue, sys_msg_time);
                Key_Event(K_MWHEELUP, kFalse, sys_msg_time);
            } else {
                Key_Event(K_MWHEELDOWN, kTrue, sys_msg_time);
                Key_Event(K_MWHEELDOWN, kFalse, sys_msg_time);
            }
            break;

        case WM_HOTKEY:
            return 0;

        case WM_CREATE:
            cl_hwnd = hWnd;

            MSH_MOUSEWHEEL = RegisterWindowMessage("MSWHEEL_ROLLMSG");
            return DefWindowProc(hWnd, uMsg, wParam, lParam);

        case WM_PAINT:
            SCR_DirtyScreen();  // force entire screen to update next frame
            return DefWindowProc(hWnd, uMsg, wParam, lParam);

        case WM_DESTROY:
            // let sound and input know about this?
            cl_hwnd = NULL;
            return DefWindowProc(hWnd, uMsg, wParam, lParam);

        case WM_ACTIVATE: {
            int fActive, fMinimized;

            // KJB: Watch this for problems in fullscreen modes with Alt-tabbing.
            fActive = LOWORD(wParam);
            fMinimized = (BOOL)HIWORD(wParam);

            AppActivate(fActive != WA_INACTIVE, fMinimized);

            if (reflib_active)
                re.AppActivate((fActive != WA_INACTIVE) ? kTrue : kFalse);
        }
            return DefWindowProc(hWnd, uMsg, wParam, lParam);

        case WM_MOVE: {
            int xPos, yPos;
            RECT r;
            int style;

            if (!vid_fullscreen->value) {
                xPos = (short)LOWORD(lParam);  // horizontal position
                yPos = (short)HIWORD(lParam);  // vertical position

                r.left = 0;
                r.top = 0;
                r.right = 1;
                r.bottom = 1;

                style = GetWindowLong(hWnd, GWL_STYLE);
                AdjustWindowRect(&r, style, FALSE);

                Cvar_SetValue("vid_xpos", xPos + r.left);
                Cvar_SetValue("vid_ypos", yPos + r.top);
                vid_xpos->modified = kFalse;
                vid_ypos->modified = kFalse;
                if (false != ActiveApp)
                    IN_Activate(true);
            }
        }
            return DefWindowProc(hWnd, uMsg, wParam, lParam);

            // this is complicated because Win32 seems to pack multiple mouse events into
            // one update sometimes, so we always check all states and look for events
        case WM_LBUTTONDOWN:
        case WM_LBUTTONUP:
        case WM_RBUTTONDOWN:
        case WM_RBUTTONUP:
        case WM_MBUTTONDOWN:
        case WM_MBUTTONUP:
        case WM_MOUSEMOVE: {
            int temp;

            temp = 0;

            if (wParam & MK_LBUTTON)
                temp |= 1;

            if (wParam & MK_RBUTTON)
                temp |= 2;

            if (wParam & MK_MBUTTON)
                temp |= 4;

            IN_MouseEvent(temp);
        } break;

        case WM_SYSCOMMAND:
            if (wParam == SC_SCREENSAVE)
                return 0;
            return DefWindowProc(hWnd, uMsg, wParam, lParam);
        case WM_SYSKEYDOWN:
            if (wParam == 13) {
                if (vid_fullscreen) {
                    Cvar_SetValue("vid_fullscreen", !vid_fullscreen->value);
                }
                return 0;
            }
            // fall through
        case WM_KEYDOWN:
            Key_Event(MapKey(lParam), kTrue, sys_msg_time);
            break;

        case WM_SYSKEYUP:
        case WM_KEYUP:
            Key_Event(MapKey(lParam), kFalse, sys_msg_time);
            break;

        default:  // pass all unhandled messages to DefWindowProc
            return DefWindowProc(hWnd, uMsg, wParam, lParam);
    }

    /* return 0 if handled message, 1 if not */
    return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

/*
============
VID_Restart_f

Console command to re-start the video mode and refresh DLL. We do this
simply by setting the modified flag for the vid_ref variable, which will
cause the entire video mode and refresh DLL to be reset on the next frame.
============
*/
void VID_Restart_f(void) {
    vid_ref->modified = kTrue;
}

void VID_Front_f(void) {
    SetWindowLong(cl_hwnd, GWL_EXSTYLE, WS_EX_TOPMOST);
    SetForegroundWindow(cl_hwnd);
}

/*
** VID_GetModeInfo
*/
typedef struct vidmode_s {
    const char* description;
    int width, height;
    int mode;
} vidmode_t;

vidmode_t vid_modes[] =
    {
        {"Mode 0: 320x240", 320, 240, 0},
        {"Mode 1: 400x300", 400, 300, 1},
        {"Mode 2: 512x384", 512, 384, 2},
        {"Mode 3: 640x480", 640, 480, 3},
        {"Mode 4: 800x600", 800, 600, 4},
        {"Mode 5: 960x720", 960, 720, 5},
        {"Mode 6: 1024x768", 1024, 768, 6},
        {"Mode 7: 1152x864", 1152, 864, 7},
        {"Mode 8: 1280x960", 1280, 960, 8},
        {"Mode 9: 1600x1200", 1600, 1200, 9}};

qboolean VID_GetModeInfo(int* width, int* height, int mode) {
    if (mode < 0 || mode >= VID_NUM_MODES)
        return kFalse;

    *width = vid_modes[mode].width;
    *height = vid_modes[mode].height;

    return kTrue;
}

/*
** VID_UpdateWindowPosAndSize
*/
void VID_UpdateWindowPosAndSize(int x, int y) {
    RECT r;
    int style;
    int w, h;

    r.left = 0;
    r.top = 0;
    r.right = viddef.width;
    r.bottom = viddef.height;

    style = GetWindowLong(cl_hwnd, GWL_STYLE);
    AdjustWindowRect(&r, style, FALSE);

    w = r.right - r.left;
    h = r.bottom - r.top;

    MoveWindow(cl_hwnd, vid_xpos->value, vid_ypos->value, w, h, TRUE);
}

/*
** VID_NewWindow
*/
void VID_NewWindow(int width, int height) {
    viddef.width = width;
    viddef.height = height;

    cl.force_refdef = kTrue;  // can't use a paused refdef
}

void VID_FreeReflib(void) {
    SDL_UnloadObject(reflib_library);
    memset(&re, 0, sizeof(re));
    reflib_library = NULL;
    reflib_active = kFalse;
}

/*
==============
VID_LoadRefresh
==============
*/
qboolean VID_LoadRefresh(char* name) {
    refimport_t ri;
    GetRefAPI_t GetRefAPI;

    if (reflib_active) {
        re.Shutdown();
        VID_FreeReflib();
    }

    Com_Printf("------- Loading %s -------\n", name);

    if ((reflib_library = SDL_LoadObject(name)) == nullptr) {
        Com_Printf("SDL_LoadObject(\"%s\") failed\n", name);

        return kFalse;
    }

    ri.Cmd_AddCommand = Cmd_AddCommand;
    ri.Cmd_RemoveCommand = Cmd_RemoveCommand;
    ri.Cmd_Argc = Cmd_Argc;
    ri.Cmd_Argv = Cmd_Argv;
    ri.Cmd_ExecuteText = Cbuf_ExecuteText;
    ri.Con_Printf = VID_Printf;
    ri.Sys_Error = VID_Error;
    ri.FS_LoadFile = FS_LoadFile;
    ri.FS_FreeFile = FS_FreeFile;
    ri.FS_Gamedir = FS_Gamedir;
    ri.Cvar_Get = Cvar_Get;
    ri.Cvar_Set = Cvar_Set;
    ri.Cvar_SetValue = Cvar_SetValue;
    ri.Vid_GetModeInfo = VID_GetModeInfo;
    ri.Vid_MenuInit = VID_MenuInit;
    ri.Vid_NewWindow = VID_NewWindow;

    if ((GetRefAPI = (GetRefAPI_t)SDL_LoadFunction(reflib_library, "GetRefAPI")) == nullptr)
        Com_Error(ERR_FATAL, "GetProcAddress failed on %s", name);

    re = GetRefAPI(ri);

    if (re.api_version != API_VERSION) {
        VID_FreeReflib();
        Com_Error(ERR_FATAL, "%s has incompatible api_version", name);
    }

    if (re.Init(MainWndProc) == -1) {
        re.Shutdown();
        VID_FreeReflib();
        return kFalse;
    }

    Com_Printf("------------------------------------\n");
    reflib_active = kTrue;

    //======
    // PGM
    vidref_val = VIDREF_OTHER;
    if (vid_ref) {
        if (!strcmp(vid_ref->string, "gl"))
            vidref_val = VIDREF_GL;
        else if (!strcmp(vid_ref->string, "soft"))
            vidref_val = VIDREF_SOFT;
    }
    // PGM
    //======

    return kTrue;
}

/*
============
VID_CheckChanges

This function gets called once just before drawing each frame, and it's sole purpose in life
is to check to see if any of the video mode parameters have changed, and if they have to
update the rendering DLL and/or video mode to match.
============
*/
void VID_CheckChanges(void) {
    char name[100];

    if (vid_ref->modified) {
        cl.force_refdef = kTrue;  // can't use a paused refdef
        S_StopAllSounds();
    }
    while (vid_ref->modified) {
        /*
        ** refresh has changed
        */
        vid_ref->modified = kFalse;
        vid_fullscreen->modified = kTrue;
        cl.refresh_prepped = kFalse;
        cls.disable_screen = kTrue;

        Com_sprintf(name, sizeof(name), "ref_%s.dll", vid_ref->string);
        if (!VID_LoadRefresh(name)) {
            if (strcmp(vid_ref->string, "soft") == 0)
                Com_Error(ERR_FATAL, "Couldn't fall back to software refresh!");
            Cvar_Set("vid_ref", "soft");

            /*
            ** drop the console if we fail to load a refresh
            */
            if (cls.key_dest != key_console) {
                Con_ToggleConsole_f();
            }
        }
        cls.disable_screen = kFalse;
    }

    /*
    ** update our window position
    */
    if (vid_xpos->modified || vid_ypos->modified) {
        if (!vid_fullscreen->value)
            VID_UpdateWindowPosAndSize(vid_xpos->value, vid_ypos->value);

        vid_xpos->modified = kFalse;
        vid_ypos->modified = kFalse;
    }
}

/*
============
VID_Init
============
*/
void VID_Init(void) {
    /* Create the video variables so we know how to start the graphics drivers */
    vid_ref = Cvar_Get("vid_ref", "gl", CVAR_ARCHIVE);
    vid_xpos = Cvar_Get("vid_xpos", "3", CVAR_ARCHIVE);
    vid_ypos = Cvar_Get("vid_ypos", "22", CVAR_ARCHIVE);
    vid_fullscreen = Cvar_Get("vid_fullscreen", "0", CVAR_ARCHIVE);
    vid_gamma = Cvar_Get("vid_gamma", "1", CVAR_ARCHIVE);

    /* Add some console commands that we want to handle */
    Cmd_AddCommand("vid_restart", VID_Restart_f);
    Cmd_AddCommand("vid_front", VID_Front_f);

    /* Start the graphics mode and load refresh DLL */
    VID_CheckChanges();
}

/*
============
VID_Shutdown
============
*/
void VID_Shutdown(void) {
    if (reflib_active) {
        re.Shutdown();
        VID_FreeReflib();
    }
}

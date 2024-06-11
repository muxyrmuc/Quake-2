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
/*
** GLW_IMP.C
**
** This file contains ALL Win32 specific stuff having to do with the
** OpenGL refresh.  When a port is being made the following functions
** must be implemented by the port:
**
** GLimp_EndFrame
** GLimp_Init
** GLimp_Shutdown
**
*/
#include <assert.h>
#include "../ref_gl/gl_local.h"
#include "glw_win.h"
#include "winquake.h"
#include <cstdint>

qboolean GLimp_InitGL(void);

glwstate_t glw_state;

extern cvar_t* vid_fullscreen;
extern cvar_t* vid_ref;

/*
** VID_CreateWindow
*/

qboolean VID_CreateWindow(int width, int height, qboolean fullscreen) {
    int x = 0;
    int y = 0;

    std::uint32_t window_flags = SDL_WINDOW_OPENGL;
    if (fullscreen) {
        window_flags |= SDL_WINDOW_FULLSCREEN;
    } else {
        cvar_t* vid_xpos = ri.Cvar_Get("vid_xpos", "0", 0);
        cvar_t* vid_ypos = ri.Cvar_Get("vid_ypos", "0", 0);
        x = vid_xpos->value;
        y = vid_ypos->value;
    }

    glw_state.hWnd = SDL_CreateWindow("Quake 2", x, y, width, height, window_flags);

    if (!glw_state.hWnd) {
        ri.Sys_Error(ERR_FATAL, "Couldn't create window");
    }

    // init all the gl stuff for the window
    if (!GLimp_InitGL()) {
        ri.Con_Printf(PRINT_ALL, "VID_CreateWindow() - GLimp_InitGL failed\n");
        return kFalse;
    }

    // let the sound and input subsystems know about the new window
    ri.Vid_NewWindow(width, height);

    return kTrue;
}

/*
** GLimp_SetMode
*/
rserr_t GLimp_SetMode(int* pwidth, int* pheight, int mode, qboolean fullscreen) {
    int width, height;
    const char* win_fs[] = {"W", "FS"};

    ri.Con_Printf(PRINT_ALL, "Initializing OpenGL display\n");

    ri.Con_Printf(PRINT_ALL, "...setting mode %d:", mode);

    if (!ri.Vid_GetModeInfo(&width, &height, mode)) {
        ri.Con_Printf(PRINT_ALL, " invalid mode\n");
        return rserr_invalid_mode;
    }

    ri.Con_Printf(PRINT_ALL, " %d %d %s\n", width, height, win_fs[fullscreen]);

    // destroy the existing window
    if (glw_state.hWnd) {
        GLimp_Shutdown();
    }

    if (fullscreen) {
        ri.Con_Printf(PRINT_ALL, "...setting fullscreen mode\n");

        *pwidth = width;
        *pheight = height;
        // TODO: handle dual-monitor configurations somewhere here
        if (!VID_CreateWindow(width, height, kTrue))
            return rserr_invalid_mode;

        ri.Con_Printf(PRINT_ALL, "ok\n");
        gl_state.fullscreen = kTrue;
        return rserr_ok;
    } else {
        ri.Con_Printf(PRINT_ALL, "...setting windowed mode\n");

        *pwidth = width;
        *pheight = height;
        gl_state.fullscreen = kFalse;
        if (!VID_CreateWindow(width, height, kFalse))
            return rserr_invalid_mode;
    }

    return rserr_ok;
}

/*
** GLimp_Shutdown
**
** This routine does all OS specific shutdown procedures for the OpenGL
** subsystem.  Under OpenGL this means NULLing out the current DC and
** HGLRC, deleting the rendering context, and releasing the DC acquired
** for the window.  The state structure is also nulled out.
**
*/
void GLimp_Shutdown(void) {
    if (SDL_GL_MakeCurrent(nullptr, nullptr) == 0)
        ri.Con_Printf(PRINT_ALL, "ref_gl::R_Shutdown() - SDL_GL_MakeCurrent failed\n");
    if (glw_state.hGLRC) {
        SDL_GL_DeleteContext(glw_state.hGLRC);
        glw_state.hGLRC = NULL;
    }
    if (glw_state.hWnd) {
        SDL_DestroyWindow(glw_state.hWnd);
        glw_state.hWnd = NULL;
    }

    if (glw_state.log_fp) {
        fclose(glw_state.log_fp);
        glw_state.log_fp = 0;
    }

    // TODO: do I actually have to do it after the SDL_SetWindowFullscreen() call?
    /* if (gl_state.fullscreen) {
        ChangeDisplaySettings(0, 0);
        gl_state.fullscreen = kFalse;
    } */
}

/*
** GLimp_Init
**
** This routine is responsible for initializing the OS specific portions
** of OpenGL.  Under Win32 this means dealing with the pixelformats and
** doing the wgl interface stuff.
*/
qboolean GLimp_Init(void* hinstance, void* wndproc) {
    // TODO: assuming true but do we actually need this option at all?
    glw_state.allowdisplaydepthchange = kTrue;
    glw_state.wndproc = wndproc;

    return kTrue;
}

qboolean GLimp_InitGL(void) {
    PIXELFORMATDESCRIPTOR pfd =
        {
            sizeof(PIXELFORMATDESCRIPTOR),  // size of this pfd
            1,                              // version number
            PFD_DRAW_TO_WINDOW |            // support window
                PFD_SUPPORT_OPENGL |        // support OpenGL
                PFD_DOUBLEBUFFER,           // double buffered
            PFD_TYPE_RGBA,                  // RGBA type
            24,                             // 24-bit color depth
            0, 0, 0, 0, 0, 0,               // color bits ignored
            0,                              // no alpha buffer
            0,                              // shift bit ignored
            0,                              // no accumulation buffer
            0, 0, 0, 0,                     // accum bits ignored
            32,                             // 32-bit z-buffer
            0,                              // no stencil buffer
            0,                              // no auxiliary buffer
            PFD_MAIN_PLANE,                 // main layer
            0,                              // reserved
            0, 0, 0                         // layer masks ignored
        };
    int pixelformat;

    gl_state.stereo_enabled = kFalse;

    /*
    ** Get a DC for the specified window
    */
    if (glw_state.hDC != NULL)
        ri.Con_Printf(PRINT_ALL, "GLimp_Init() - non-NULL DC exists\n");

    if ((glw_state.hDC = GetDC(glw_state.hWnd)) == NULL) {
        ri.Con_Printf(PRINT_ALL, "GLimp_Init() - GetDC failed\n");
        return kFalse;
    }

    if ((pixelformat = ChoosePixelFormat(glw_state.hDC, &pfd)) == 0) {
        ri.Con_Printf(PRINT_ALL, "GLimp_Init() - ChoosePixelFormat failed\n");
        return kFalse;
    }
    if (SetPixelFormat(glw_state.hDC, pixelformat, &pfd) == FALSE) {
        ri.Con_Printf(PRINT_ALL, "GLimp_Init() - SetPixelFormat failed\n");
        return kFalse;
    }
    DescribePixelFormat(glw_state.hDC, pixelformat, sizeof(pfd), &pfd);

    /*
    ** startup the OpenGL subsystem by creating a context and making
    ** it current
    */
    if ((glw_state.hGLRC = qwglCreateContext(glw_state.hDC)) == 0) {
        ri.Con_Printf(PRINT_ALL, "GLimp_Init() - qwglCreateContext failed\n");

        goto fail;
    }

    if (!qwglMakeCurrent(glw_state.hDC, glw_state.hGLRC)) {
        ri.Con_Printf(PRINT_ALL, "GLimp_Init() - qwglMakeCurrent failed\n");

        goto fail;
    }

    /*
    ** print out PFD specifics
    */
    ri.Con_Printf(PRINT_ALL, "GL PFD: color(%d-bits) Z(%d-bit)\n", (int)pfd.cColorBits, (int)pfd.cDepthBits);

    return kTrue;

fail:
    if (glw_state.hGLRC) {
        SDL_GL_DeleteContext(glw_state.hGLRC);
        glw_state.hGLRC = NULL;
    }

    return kFalse;
}

/*
** GLimp_BeginFrame
*/
void GLimp_BeginFrame(float camera_separation) {
    if (gl_bitdepth->modified) {
        if (gl_bitdepth->value != 0 && !glw_state.allowdisplaydepthchange) {
            ri.Cvar_SetValue("gl_bitdepth", 0);
            ri.Con_Printf(PRINT_ALL, "gl_bitdepth requires Win95 OSR2.x or WinNT 4.x\n");
        }
        gl_bitdepth->modified = kFalse;
    }

    if (camera_separation < 0 && gl_state.stereo_enabled) {
        qglDrawBuffer(GL_BACK_LEFT);
    } else if (camera_separation > 0 && gl_state.stereo_enabled) {
        qglDrawBuffer(GL_BACK_RIGHT);
    } else {
        qglDrawBuffer(GL_BACK);
    }
}

/*
** GLimp_EndFrame
**
** Responsible for doing a swapbuffers and possibly for other stuff
** as yet to be determined.  Probably better not to make this a GLimp
** function and instead do a call to GLimp_SwapBuffers.
*/
void GLimp_EndFrame(void) {
    int err;

    err = qglGetError();
    assert(err == GL_NO_ERROR);

    if (Q_stricmp(gl_drawbuffer->string, "GL_BACK") == 0) {
        SDL_GL_SwapWindow(glw_state.hWnd);
    }
}

/*
** GLimp_AppActivate
*/
void GLimp_AppActivate(qboolean active) {
    if (active) {
        // TODO: what's the order?
        SDL_RestoreWindow(glw_state.hWnd);
        SDL_RaiseWindow(glw_state.hWnd);
    } else {
        if (vid_fullscreen->value)
            SDL_MinimizeWindow(glw_state.hWnd);
    }
}

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

#include "../qcommon/qcommon.h"
#include "winquake.h"
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <direct.h>
#include <io.h>
#include <conio.h>

//===============================================================================

int hunkcount;

byte* membase;
int hunkmaxsize;
int cursize;

#define VIRTUAL_ALLOC

void* Hunk_Begin(int maxsize) {
    // reserve a huge chunk of memory, but don't commit any yet
    cursize = 0;
    hunkmaxsize = maxsize;
#ifdef VIRTUAL_ALLOC
    membase = static_cast<byte*>(VirtualAlloc(NULL, maxsize, MEM_RESERVE, PAGE_NOACCESS));
#else
    membase = malloc(maxsize);
    memset(membase, 0, maxsize);
#endif
    if (!membase)
        Sys_Error("VirtualAlloc reserve failed");
    return (void*)membase;
}

void* Hunk_Alloc(int size) {
    void* buf;

    // round to cacheline
    size = (size + 31) & ~31;

#ifdef VIRTUAL_ALLOC
    // commit pages as needed
    //	buf = VirtualAlloc (membase+cursize, size, MEM_COMMIT, PAGE_READWRITE);
    buf = VirtualAlloc(membase, cursize + size, MEM_COMMIT, PAGE_READWRITE);
    if (!buf) {
        FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM, NULL, GetLastError(), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPTSTR)&buf, 0, NULL);
        Sys_Error("VirtualAlloc commit failed.\n%s", buf);
    }
#endif
    cursize += size;
    if (cursize > hunkmaxsize)
        Sys_Error("Hunk_Alloc overflow");

    return (void*)(membase + cursize - size);
}

int Hunk_End(void) {
    // free the remaining unused virtual memory
#if 0
	void	*buf;

	// write protect it
	buf = VirtualAlloc (membase, cursize, MEM_COMMIT, PAGE_READONLY);
	if (!buf)
		Sys_Error ("VirtualAlloc commit failed");
#endif

    hunkcount++;
    // Com_Printf ("hunkcount: %i\n", hunkcount);
    return cursize;
}

void Hunk_Free(void* base) {
    if (base)
#ifdef VIRTUAL_ALLOC
        VirtualFree(base, 0, MEM_RELEASE);
#else
        free(base);
#endif

    hunkcount--;
}

//===============================================================================

/*
================
Sys_Milliseconds
================
*/
int curtime;
int Sys_Milliseconds(void) {
    static int base;
    static qboolean initialized = kFalse;

    if (!initialized) {  // let base retain 16 bits of effectively random data
        base = timeGetTime() & 0xffff0000;
        initialized = kTrue;
    }
    curtime = timeGetTime() - base;

    return curtime;
}

//============================================

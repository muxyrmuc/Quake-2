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
#include <chrono>
#include <filesystem>

//===============================================================================

byte* membase;
int hunkmaxsize;
int cursize;

void* Hunk_Begin(int maxsize) {
    // reserve a huge chunk of memory, but don't commit any yet
    cursize = 0;
    hunkmaxsize = maxsize;

    membase = static_cast<byte*>(malloc(maxsize));
    if (!membase) {
        Sys_Error("Hunk_Begin reserve failed");
    }

    memset(membase, 0, maxsize);

    return static_cast<void*>(membase);
}

void* Hunk_Alloc(int size) {
    // round to cacheline
    size = (size + 31) & ~31;

    cursize += size;
    if (cursize > hunkmaxsize) {
        Sys_Error("Hunk_Alloc overflow");
    }

    return static_cast<void*>(membase + cursize - size);
}

int Hunk_End(void) {
    void* new_membase = realloc(membase, cursize);
    if (new_membase != membase) {
        Sys_Error("Hunk_End realloc failed");
    }

    return cursize;
}

void Hunk_Free(void* base) {
    if (base) {
        free(base);
    }
}

//===============================================================================

int Sys_NowMilliseconds() {
    static_assert(double(std::chrono::steady_clock::period::num) / std::chrono::steady_clock::period::den <= double(1) / 1000,
                      "steady_clock precision is too low");
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

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
        base = Sys_NowMilliseconds() & 0xffff0000;
        initialized = kTrue;
    }
    curtime = Sys_NowMilliseconds() - base;

    return curtime;
}

void Sys_Mkdir(char* path) {
    std::filesystem::create_directory(std::filesystem::path(path));
}

//============================================

char findbase[MAX_OSPATH];
char findpath[MAX_OSPATH];

namespace {

std::filesystem::directory_iterator findhandle;

}

static bool CompareAttributes(const std::filesystem::directory_entry& found, unsigned musthave, unsigned canthave) {
    const bool is_directory = found.is_directory();

    if (is_directory && (canthave & SFF_SUBDIR))
        return false;

    if ((musthave & SFF_SUBDIR) && !is_directory)
        return false;

    return true;
}

// TODO: does it work as expected? Seems like we're stopping the search in the middle
// in case there is a file with bad flags in the middle of the loop
char* Sys_FindFirst(char* path, unsigned musthave, unsigned canthave) {
    if (findhandle != std::filesystem::directory_iterator())
        Sys_Error("Sys_BeginFind without close");

    COM_FilePath(path, findbase);
    findhandle = std::filesystem::directory_iterator(path);
    if (findhandle == std::filesystem::directory_iterator())
        return NULL;
    if (!CompareAttributes(*findhandle, musthave, canthave))
        return NULL;
    Com_sprintf(findpath, sizeof(findpath), "%s/%s", findbase, findhandle->path().string().c_str());
    return findpath;
}

char* Sys_FindNext(unsigned musthave, unsigned canthave) {
    if (findhandle == std::filesystem::directory_iterator()) // default-constructed iterator is the end iterator
        return NULL;
    findhandle++;
    if (++findhandle == std::filesystem::directory_iterator())
        return NULL;

    if (!CompareAttributes(*findhandle, musthave, canthave))
        return NULL;

    Com_sprintf(findpath, sizeof(findpath), "%s/%s", findbase, findhandle->path().string().c_str());
    return findpath;
}

void Sys_FindClose(void) {
    findhandle = std::filesystem::directory_iterator();
}

//============================================

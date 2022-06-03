cmake_minimum_required(VERSION 2.6)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

project(quake2)

add_library(ref_gl SHARED
    ref_gl/gl_draw.cpp
    ref_gl/gl_image.cpp
    ref_gl/gl_light.cpp
    ref_gl/gl_mesh.cpp
    ref_gl/gl_model.cpp
    ref_gl/gl_rmain.cpp
    ref_gl/gl_rmisc.cpp
    ref_gl/gl_rsurf.cpp
    ref_gl/gl_warp.cpp
    win32/glw_imp.cpp
    win32/q_shwin.cpp
    win32/qgl_win.cpp
    game/q_shared.cpp)

add_library(game SHARED
    game/g_ai.cpp
    game/g_chase.cpp
    game/g_cmds.cpp
    game/g_combat.cpp
    game/g_func.cpp
    game/g_items.cpp
    game/g_main.cpp
    game/g_misc.cpp
    game/g_monster.cpp
    game/g_phys.cpp
    game/g_save.cpp
    game/g_spawn.cpp
    game/g_svcmds.cpp
    game/g_target.cpp
    game/g_trigger.cpp
    game/g_turret.cpp
    game/g_utils.cpp
    game/g_weapon.cpp
    game/m_actor.cpp
    game/m_berserk.cpp
    game/m_boss2.cpp
    game/m_boss31.cpp
    game/m_boss32.cpp
    game/m_boss3.cpp
    game/m_brain.cpp
    game/m_chick.cpp
    game/m_flash.cpp
    game/m_flipper.cpp
    game/m_float.cpp
    game/m_flyer.cpp
    game/m_gladiator.cpp
    game/m_gunner.cpp
    game/m_hover.cpp
    game/m_infantry.cpp
    game/m_insane.cpp
    game/m_medic.cpp
    game/m_move.cpp
    game/m_mutant.cpp
    game/m_parasite.cpp
    game/m_soldier.cpp
    game/m_supertank.cpp
    game/m_tank.cpp
    game/p_client.cpp
    game/p_hud.cpp
    game/p_trail.cpp
    game/p_view.cpp
    game/p_weapon.cpp
    game/q_shared.cpp
)

# WIN32 means WinMain()
add_executable(quake2 WIN32
    client/cl_view.cpp
    client/cl_pred.cpp
    client/cl_input.cpp
    client/cl_ents.cpp
    client/cl_scrn.cpp
    client/cl_parse.cpp
    client/cl_main.cpp
    client/cl_inv.cpp
    client/cl_fx.cpp
    client/cl_newfx.cpp
    client/cl_tent.cpp
    client/cl_cin.cpp
    client/console.cpp
    client/keys.cpp
    client/menu.cpp
    client/qmenu.cpp
    client/snd_mix.cpp
    client/snd_dma.cpp
    client/snd_mem.cpp
    
    server/sv_user.cpp
    server/sv_ents.cpp
    server/sv_send.cpp
    server/sv_world.cpp
    server/sv_main.cpp
    server/sv_game.cpp
    server/sv_ccmds.cpp
    server/sv_init.cpp

    qcommon/cmd.cpp
    qcommon/cmodel.cpp
    qcommon/common.cpp
    qcommon/crc.cpp
    qcommon/cvar.cpp
    qcommon/files.cpp
    qcommon/md4.cpp
    qcommon/net_chan.cpp
    qcommon/pmove.cpp

    game/m_flash.cpp
    game/q_shared.cpp

    win32/cd_win.cpp
    win32/net_wins.cpp
    win32/q_shwin.cpp
    win32/snd_win.cpp
    win32/vid_dll.cpp
    win32/vid_menu.cpp
    win32/sys_win.cpp
    win32/in_win.cpp
)

# Windows-only, required by timeGetTime
target_link_libraries(ref_gl winmm.lib)

# Windows-only, required by mciSendCommand, waveOutOpen and others
target_link_libraries(quake2 winmm.lib)

# Windows-only, required by Winsock-related stuff
target_link_libraries(quake2 ws2_32.lib)

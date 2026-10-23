/*
===========================================================================

Return to Castle Wolfenstein single player GPL Source Code
Copyright (C) 1999-2010 id Software LLC, a ZeniMax Media company. 

This file is part of the Return to Castle Wolfenstein single player GPL Source Code (RTCW SP Source Code).  

RTCW SP Source Code is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

RTCW SP Source Code is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with RTCW SP Source Code.  If not, see <http://www.gnu.org/licenses/>.

In addition, the RTCW SP Source Code is also subject to certain additional terms. You should have received a copy of these additional terms immediately following the terms and conditions of the GNU General Public License which accompanied the RTCW SP Source Code.  If not, please request a copy in writing from id Software at the address below.

If you have questions concerning this license or the applicable additional terms, you may contact in writing id Software LLC, c/o ZeniMax Media Inc., Suite 120, Rockville, Maryland 20850 USA.

===========================================================================
*/

//===========================================================================
//
// Name:			_files.cpp
// Function:
// Programmer:		Mr Elusive
// Last update:		1999-12-02
// Tab Size:		4
//===========================================================================

/*

aas_areamerging.cpp			//AAS area merging
aas_cfg.cpp					//AAS configuration for different games
aas_create.cpp				//AAS creating
aas_edgemelting.cpp			//AAS edge melting
aas_facemerging.cpp			//AAS face merging
aas_file.cpp					//AAS file writing
aas_gsubdiv.cpp				//AAS gravitational and ladder subdivision
aas_map.cpp					//AAS map brush creation
aas_prunenodes.cpp			//AAS node pruning
aas_store.cpp					//AAS file storing

map.cpp						//map file loading and writing
map_hl.cpp					//Half-Life map loading
map_q1.cpp					//Quake1 map loading
map_q2.cpp					//Quake2 map loading
map_q3.cpp					//Quake3 map loading
map_sin.cpp					//Sin map loading
tree.cpp						//BSP tree management + node pruning			(*)
brushbsp.cpp					//brush bsp creation							(*)
portals.cpp					//BSP portal creation and leaf filling			(*)
csg.cpp						//Constructive Solid Geometry brush chopping	(*)
leakfile.cpp					//leak file writing								(*)
textures.cpp					//Quake2 BSP textures							(*)

l_bsp_ent.cpp					//BSP entity parsing
l_bsp_hl.cpp					//Half-Life BSP loading and writing
l_bsp_q1.cpp					//Quake1 BSP loading and writing
l_bsp_q2.cpp					//Quake2 BSP loading and writing
l_bsp_q3.cpp					//Quake2 BSP loading and writing
l_bsp_sin.cpp					//Sin BSP loading and writing
l_cmd.cpp						//cmd library
l_log.cpp						//log file library
l_math.cpp					//math library
l_mem.cpp						//memory management library
l_poly.cpp					//polygon (winding) library
l_script.cpp					//script file parsing library
l_threads.cpp					//multi-threading library
l_utils.cpp					//utility library
l_qfiles.cpp					//loading of quake files

gldraw.cpp					//GL drawing									(*)
glfile.cpp					//GL file writing								(*)
nodraw.cpp					//no draw module								(*)

bspc.cpp						//BSPC Win32 console version
winbspc.cpp					//WinBSPC Win32 GUI version
win32_terminal.cpp			//Win32 terminal output
win32_qfiles.cpp				//Win32 game file management (also .pak .sin)
win32_font.cpp				//Win32 fonts
win32_folder.cpp				//Win32 folder dialogs

*/

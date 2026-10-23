--[[						MIT License

						Copyright (c) 2025 Krispy

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE. ]]

include(".premake/modules.lua")
require(".premake/vscode")

newoption {
   trigger     = "dedicated",
   description = "Build the dedicated server"
}

newoption {
   trigger     = "dll",
   description = "override the built-in game code with any custom gamex86.dll"
}

workspace("Doom3")
	configurations({"Debug", "Release", "Retail"})
	platforms({"Win32", "x64"})
	location("../build")
	--warnings( "Extra" )
	--floatingpoint( "Fast" )
	cppdialect("C++17")
	characterset( "ASCII" )

	includedirs({".", "external", "idlib"})
	defines({"__DOOM__", "ID_PC"})
	if _OPTIONS["dll"] then
		defines({"__DOOM_DLL__"})
	end

	filter("platforms:Win32")
		architecture("x86")
		objdir("../build/out/x86")
	filter({})

	filter("platforms:x64")
		architecture("x86_64")
		objdir("../build/out/x86_64")
	filter({})

	-- _Common.props
	filter("system:Windows")
		system("Windows")
		defines({"_CRT_SECURE_NO_DEPRECATE", "_CRT_NONSTDC_NO_DEPRECATE"})
		warnings("Extra")
		multiprocessorcompile("On")

		filter("platforms:Win32")
			defines({"_USE_32BIT_TIME_T"})
		filter({})

		filter("kind:SharedLib")
			-- We don't want manifests for DLLs
			manifest("Off")
		filter({})

		-- _Debug.props
		filter("configurations:Debug")
			optimize("Off")
			defines("_DEBUG")
			buildoptions({"/RTC1"})
			linktimeoptimizationex("Off")
			mapfile("On")
			staticruntime("On")
			runtime("Debug")
			symbols("On")
			rtti("On")
			editandcontinue("On")
			--sanitize({"Address"})
		filter({}) -- Debug Scope

		-- _Release.props
		filter("configurations:Release")
			optimize("Speed")
			defines("NDEBUG")
			stringpooling("On")
			buildoptions({"/Ob2", "/Gy"})
			linktimeoptimizationex("On")
			intrinsics("On")
			staticruntime("On")
			runtime("Release")
			buffersecuritycheck("Off")
			rtti("On")
			editandcontinue("On")
			extraoptimization()
		filter({}) -- Release Scope

		-- _Retail.props
		filter("configurations:Retail")
			optimize("Full")
			defines({"NDEBUG", "ID_RETAIL"})
			stringpooling("On")
			buildoptions({"/Ob2", "/Gy"})
			linktimeoptimizationex("On")
			intrinsics("On")
			mapfile("Off")
			staticruntime("On")
			runtime("Release")
			buffersecuritycheck("On")
			symbols("Off")
			rtti("On")
			editandcontinue("On")
			extraoptimization()
		filter({}) -- Release Scope

		-- _DoomEXE.props
		adressstacksize(16777216,16777216)
		filter("kind:not StaticLib")
			largeaddressaware("On")
		filter({})

		-- _PCLibs.props
		filter("kind:not StaticLib")
			links({"dbghelp", "glu32", "iphlpapi", "odbc32", "odbccp32", "opengl32", "winmm", "wsock32"}) -- Windows Libs
			links({"xinput", "dinput8", "dsound", "dxguid"}) -- DirectX Libs
		filter({})
	filter({}) -- Windows Scope

	include("external/external.lua")
	include("idlib/idlib.lua")
	include("d3xp/d3xp.lua")

	-- DoomEXE
	group("exes")

	project("DoomEXE")
		targetname("Doom3BFG")
		targetdir("../bin")
		kind("WindowedApp")
		language("C++")
		links({"external", "idLib"})
		if not _OPTIONS["dll"] then
			links({"Game-D3XP"})

			filter("toolset:msc*")
				linkoptions({"/WHOLEARCHIVE:gamex86"})
			filter({})
		end

		pchsource("framework/precompiled.cpp")
		pchheader("")

		files({"premake5.lua", "cm/**", "framework/**", "renderer/**", "sound/**", "swf/**", "sys/**", "tools/**", "ui/**" } )
		removefiles({"framework/Session.cpp", "tools/compilers/roqvq/roq.m", "ui/GameWindow.cpp", "sys/win32/win_stats.*", "sys/win32/win_snd.cpp", "sys/win32/win_gamma.cpp", "renderer/BoundsTrack.cpp"})

		filter("files:renderer/jobs/**.cpp")
			enablepch("Off")
		filter({})

		filter("files:**.c")
			enablepch("Off")
		filter({})
	group("")

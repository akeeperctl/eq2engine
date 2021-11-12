-- premake5.lua

require ".premake_modules/usage"
require ".premake_modules/android_studio"

IS_ANDROID = (_ACTION == "android-studio")

-- you can redefine dependencies
DependencyPath = {
	["zlib"] = os.getenv("ZLIB_DIR") or "src_dependency/zlib", 
	["libjpeg"] = os.getenv("JPEG_DIR") or "src_dependency/libjpeg", 
	["libogg"] = os.getenv("OGG_DIR") or "src_dependency/libogg", 
	["libvorbis"] = os.getenv("VORBIS_DIR") or "src_dependency/libvorbis", 
	["libsdl"] = os.getenv("SDL2_DIR") or "src_dependency/SDL2",
	["openal"] = os.getenv("OPENAL_DIR") or "src_dependency/openal-soft",
	
	["AndroidSDK"] = os.getenv("ANDROID_HOME") or "F:/Dev/AndroidSDK",
	["AndroidNDK"] = os.getenv("ANDROID_NDK_PATH") or "F:/Dev/android-ndk-r17c",
	["Android_libsdl"] = os.getenv("SDL2_DIR") or "src_dependency_android/SDL2",
	["Android_openal"] = os.getenv("OPENAL_DIR") or "src_dependency_android/openal-soft",
}

-- default configuration capabilities
Groups = {
    core = "Framework",
    
    engine1 = "Equilibrium 1 port",
    component1 = "Equilibrium 1 port/Components",

    engine2 = "Equilibrium 2",
    tools = "Tools",

    game = "Game",
}

-- folders for framework, libraries and tools
Folders = {
    public =  "./public/",
    matsystem1 = "./materialsystem1/",
    shared_engine = "./shared_engine/",
    shared_game = "./shared_game/",
    dependency = "./src_dependency/",
    game = "./game/",
}

-- Main workspace
workspace "Equilibrium2"
    language "C++"
	cppdialect "C++17"	-- required for sol2
    configurations { "Debug", "Release" }
	linkgroups 'On'
	
	--characterset "ASCII"
	objdir "build"
	targetdir "bin/%{cfg.platform}/%{cfg.buildcfg}"
	location "project_%{_ACTION}"
	
	if not IS_ANDROID then
		platforms { "x86", "x64" }
	end
	
	if IS_ANDROID then
	
		system "android"
	
		flags { "NoImportLib" }
		
		disablewarnings {
			-- disable warnings which are emitted by my stupid code
			"c++11-narrowing",
			"writable-strings",
			"logical-op-parentheses",
			"parentheses",
			"register",
			"unused-local-typedef",
			
		}
		
		--filter "files:**.c"
		--	buildoptions { "-fgnu89-inline" }
		
		buildoptions {
			
			"-fpermissive",

			"-fexceptions",
			"-pthread",
		}
		
		linkoptions {
			"--no-undefined",
			"-fexceptions",
			"-pthread",
			
			"-mfloat-abi=softfp",	-- force NEON to be used
			"-mfpu=neon"
		}
	
		androidabis { 
			"armeabi-v7a", "arm64-v8a" --"x86", "x86_64" 
		}
		
		androiddependencies
		{
			--"com.android.support:support-v4:27.1.0",
		}
		
		androidrepositories
		{
			"jcenter()",
			"google()",
			"mavenCentral()"
		}
		
		gradlewrapper {
			"distributionUrl=https://services.gradle.org/distributions/gradle-4.10.2-all.zip"
		}
		
		gradleversion "com.android.tools.build:gradle:3.2.0"
		--gradleversion "com.android.tools.build:gradle:7.0.0"
		androidsdkversion "26"
		androidminsdkversion "16"
		--androidndkpath(DependencyPath.AndroidNDK)
	end
	
    filter "system:linux"
        buildoptions {
            "-Wno-narrowing",
            "-fpermissive",
			"-fexceptions",
			"-fpic"
        }
		links { "pthread" }

	filter "system:Windows"
		disablewarnings { "4996", "4554", "4244", "4101", "4838", "4309" }
		defines { 
			"NOMINMAX", 
			"_CRT_SECURE_NO_WARNINGS", "_CRT_SECURE_NO_DEPRECATE"
		}

    filter "configurations:Debug"
        defines { 
            "DEBUG"
        }
        symbols "On"
		optimize "On"

    filter "configurations:Release"
        defines {
            "NDEBUG",
        }
		optimize "On"

	filter "system:Windows or system:Linux or system:Android"
		defines { 
			"EQ_USE_SDL"
		}
		
group "Dependencies"
		
-- dependencies are in separate configuration
include "src_dependency/premake5.lua"
include "src_dependency_android/premake5.lua"

group "Core"

-- eqCore essentials
project "corelib"
    kind "StaticLib"
	
    files {
		Folders.public.. "/core/*.cpp",
		Folders.public.. "/core/**.h"
	}
	includedirs {
		Folders.public
	}
	
usage "corelib"
    includedirs {
		Folders.public
	}
	links {"coreLib"}
	
-- little framework
project "frameworkLib"
    kind "StaticLib"

	uses { "corelib", "libjpeg" }

    files {
        Folders.public.. "/utils/*.cpp",
        Folders.public.. "/math/*.cpp",
        Folders.public.. "/imaging/*.cpp",
		Folders.public.. "/utils/*.h",
        Folders.public.. "/math/*.h", 
		Folders.public.. "/math/*.inl",
        Folders.public.. "/imaging/*.h"
	}

usage "frameworkLib"
    includedirs { Folders.public }
	
	links { "frameworkLib" }

	filter "system:Android"
		links { "log" }
	
	filter "system:Windows"
		links { "User32" }
	
----------------------------------------------
-- e2Core

project "e2Core"
    kind "SharedLib"
	
    files {
        "core/**.cpp",
        "core/minizip/**.c",
        "core/**.h",
        Folders.public.. "/core/**.h"
	}
	
	defines { "CORE_INTERFACE_EXPORT", "COREDLL_EXPORT" }
	
    uses { "zlib", "corelib", "frameworkLib" }
	
	filter "system:Windows"
		linkoptions { "-IGNORE:4217,4286" }	-- disable few linker warnings

    filter "system:android"
        files {
			"core/android_libc/**.c", 
			"core/android_libc/**.h"
		}

    filter "system:Windows"
        links {"User32", "DbgHelp", "Advapi32"}
		
usage "e2Core"
	links "e2Core"

----------------------------------------------
-- Sub usages

group "Components"

project "sysLib"
	kind "StaticLib"
	uses { 
		"corelib", "frameworkLib",
		"SDL2"	
	}
    files {
		Folders.shared_engine.. "sys/**.cpp",
		Folders.shared_engine.. "sys/**.h",
		Folders.shared_engine.. "input/**.cpp",
		Folders.shared_engine.. "input/**.h"
	}
    includedirs {
		Folders.shared_engine
	}
    

project "equiLib"
	kind "StaticLib"
	uses { 
		"corelib", "frameworkLib"
	}
    files {
		Folders.shared_engine.. "equi/**.cpp",
		Folders.shared_engine.. "equi/**.h"
	}
    includedirs {
		Folders.shared_engine
	}
	

project "networkLib"
	kind "StaticLib"
	uses {
		"corelib", "frameworkLib",
		"zlib" 
	}
    files {
		Folders.shared_engine.. "network/**.cpp",
		Folders.shared_engine.. "network/**.h"
	}
	-- this one is temporary. Once rewrite done it will be removed
	excludes {
		Folders.shared_engine.. "network/NETThread.cpp",
	}
    includedirs {
		Folders.shared_engine
	}
	
	filter "system:Windows"
		links { "wsock32" }

project "soundSystemLib"
	kind "StaticLib"
	uses { 
		"corelib", "frameworkLib",
		"libvorbis", "libogg", "openal-soft"
	}
    files {
		Folders.shared_engine.. "audio/**.cpp",
		Folders.shared_engine.. "audio/**.h",
		Folders.public.. "audio/**.h"
	}
    includedirs {
		Folders.public
	}

project "physicsLib"
	kind "StaticLib"
	uses { 
		"corelib", "frameworkLib",
		"bullet2"
	}
    files {
		Folders.shared_engine.. "physics/**.cpp"
	}
    includedirs {
		Folders.shared_engine
	}
	
group ""

include "premake5-eq1.lua"
include "game/DriverSyndicate/premake5.lua"

-- only build tools for big machines
if (os.target() == "windows" or os.target() == "linux") and not IS_ANDROID then
    include "utils/premake5.lua"
end

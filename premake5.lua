workspace "gatery-challenge"
    configurations { "Debug", "Release" }
    architecture "x64"
    symbols "On"
    flags { "MultiProcessorCompile" }
    cppdialect "C++latest"
    startproject "gatery-challenge"

    targetdir "%{wks.location}/bin/%{cfg.system}-%{cfg.architecture}-%{cfg.longname}"
    objdir "%{wks.location}/obj/%{cfg.system}-%{cfg.architecture}-%{cfg.longname}"

    defines "NOMINMAX"

    filter "configurations:Debug"
        runtime "Debug"

    filter "configurations:Release"
        runtime "Release"
        optimize "On"

    filter "system:linux"
        buildoptions { "-std=c++2a", "-fcoroutines"}
        includedirs {
            "/usr/local/vcpkg/installed/x64-linux/include/"
        }
        libdirs {
            "/usr/local/vcpkg/installed/x64-linux/lib/"
        }

    project "gatery-challenge"
        kind "ConsoleApp"
        links { "gatery"}
        files { "source/**" }
        includedirs { 
            "%{prj.location}/libs/gatery/source",
        }

include "libs/gatery/source/premake5.lua"
-- Enable to also build unit tests of gatery
-- include "libs/gatery/tests/premake5.lua"


    project "*"
        GateryWorkspaceDefaults()

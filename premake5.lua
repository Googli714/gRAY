-- premake5.lua
workspace "gRAY"
   architecture "x64"
   configurations { "Debug", "Release", "Dist" }
   startproject "gRAY"

outputdir = "%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}"
include "Walnut/WalnutExternal.lua"

include "gRAY"
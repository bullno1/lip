local function add_flags(flags)
	buildoptions(flags)
	linkoptions(flags)
end

local function declare_project(name)
	project(name)
		language "C"
		includedirs { "include", "deps" }
		flags {
			"ExtraWarnings",
			"FatalWarnings"
		}
		configuration "linux"
			buildoptions {
				"-Wno-missing-field-initializers"
			}

		files { "src/"..name.."/**" }
end

local function declare_library(name)
	declare_project(name)
		language "C"
		targetname("lip-"..name)

		configuration "*Dynamic"
			kind "SharedLib"

		configuration "*Static"
			kind "StaticLib"

		configuration "linux"
			buildoptions { "-pedantic" }

		configuration {"linux", "*Dynamic"}
			buildoptions { "-fvisibility=hidden" }

		configuration "Debug*"
			if _OPTIONS['with-coverage'] then
				add_flags { "--coverage" }
			end

		configuration {}

		defines { "LIP_" .. name:upper() .. "_BUILDING" }

		files { "include/lip/"..name..".h", }
end

local function declare_app(name)
	declare_project(name)
		kind "ConsoleApp"
end

-- Override generator to use $ORIGIN as rpath
local ninja = premake.ninja
function ninja.cpp.linker(prj, cfg, objfiles, tool)
	local all_ldflags = ninja.list(table.join(tool.getlibdirflags(cfg), tool.getldflags(cfg), cfg.linkoptions))
	local lddeps      = ninja.list(premake.getlinks(cfg, "siblings", "fullpath"))
	local libs

	local flags = {}
	local has_so = false
	for _, value in ipairs(premake.getlinks(cfg, "siblings", "fullpath")) do
		if path.getextension(value) == ".so" then
			table.insert(flags, "-l:" .. path.getbasename(value) .. ".so")
			has_so = true
		else
			table.insert(flags, value)
		end
	end

	libs = ninja.list(flags) .. " " .. ninja.list(tool.getlinkflags(cfg))

	if has_so then
		all_ldflags = all_ldflags .. " -Wl,-rpath='$$ORIGIN'"
	end

	local function writevars()
		_p(1, "all_ldflags = " .. all_ldflags)
		_p(1, "libs        = " .. libs)
		_p(1, "all_outputfiles = " .. table.concat(objfiles, " "))
	end

	if cfg.kind == "StaticLib" then
		local ar_flags = ninja.list(tool.getarchiveflags(cfg, cfg, false))
		_p("# link static lib")
		_p("build " .. cfg:getoutputfilename() .. ": ar " .. table.concat(objfiles, " ") .. " | " .. lddeps)
		_p(1, "flags = " .. ninja.list(tool.getarchiveflags(cfg, cfg, false)))
		_p(1, "all_outputfiles = " .. table.concat(objfiles, " "))
	elseif cfg.kind == "SharedLib" then
		local output = cfg:getoutputfilename()
		_p("# link shared lib")
		_p("build " .. output .. ": link " .. table.concat(objfiles, " ") .. " | " .. lddeps)
		writevars()
	elseif (cfg.kind == "ConsoleApp") or (cfg.kind == "WindowedApp") then
		_p("# link executable")
		_p("build " .. cfg:getoutputfilename() .. ": link " .. table.concat(objfiles, " ") .. " | " .. lddeps)
		writevars()
	else
		p.error("ninja action doesn't support this kind of target " .. cfg.kind)
	end
end

newaction {
	trigger = "gen-config.h",
	description = "Generate config.h",
	execute = function()
		local version = os.outputof("git describe --tags"):gsub("%s+", "")
		os.mkdir("include/lip/gen")
		local file = io.open("include/lip/gen/config.h", "w")
		file:write("#define LIP_VERSION \""..version.."\"\n")
		file:close()
	end
}

solution "lip"
	location "build"
	configurations { "ReleaseStatic", "DebugStatic", "ReleaseDynamic", "DebugDynamic" }
	platforms { "native", "universal", "arm" }
	debugdir "."
	targetdir "bin"
	startproject "repl"

	newoption {
		trigger = "with-config.h",
		description = "Generate config.h"
	}

	if _OPTIONS["with-config.h"] then
		prebuildcommands {
			_PREMAKE_COMMAND .. " gen-config.h"
		}

		defines { "LIP_HAVE_CONFIG_H" }
	end

	flags {
		"StaticRuntime",
		"Symbols",
		"NoEditAndContinue",
		"NoNativeWChar",
		"NoManifest"
	}

	defines { "_CRT_SECURE_NO_WARNINGS" }

	configuration "linux"
		newoption {
			trigger = "with-coverage",
			description = "Compile with coverage"
		}

		newoption {
			trigger = "with-asan",
			description = "Compile with AddressSanitizer"
		}

		newoption {
			trigger = "with-ubsan",
			description = "Compile with UndefinedBehaviorSanitizer"
		}

		newoption {
			trigger = "with-lto",
			description = "Compile with Link-time Optimization"
		}

		newoption {
			trigger = "with-clang",
			description = "Compile with Clang"
		}

		newoption {
			trigger = "with-gprof",
			description = "Compile for gprof"
		}

		add_flags { "-pthread" }

	configuration "Release*"
		flags { "OptimizeSpeed" }

	configuration "*Dynamic"
		defines { "LIP_DYNAMIC=1" }

	configuration { "Release*", "linux" }
		if _OPTIONS["with-lto"] then
			add_flags { add_flags("-flto") }
		end

	configuration "linux"
		if _OPTIONS["with-asan"] then
			add_flags { "-fsanitize=address", "-fPIC" }
		end

		if _OPTIONS["with-ubsan"] then
			add_flags {
				"-fsanitize=undefined",
				"-fno-sanitize-recover=undefined"
			}
		end

		if _OPTIONS["with-gprof"] then
			add_flags { "-pg" }
		end

		if _OPTIONS["with-clang"] then
			premake.gcc.cc = "clang"
			premake.gcc.cxx = "clang++"
			premake.gcc.llvm = true
		end

	declare_library "core"
		files { "include/lip/core/*.h" }

	declare_library "std"
		links { "core" }

	declare_library "dbg"
		links { "core", "cmp" }

		configuration "linux"
			buildoptions { "-Wno-unused-function" }
			linkoptions { "-Wl,--exclude-libs=ALL" }

	declare_app "repl"
		targetname "lip"
		links { "core", "std", "dbg", "linenoise" }

		configuration { "linux", "*Static" }
			links { "cmp" }

	declare_app "compiler"
		targetname "lipc"
		links { "core", "std" }

	project "cmp"
		language "C"
		kind "StaticLib"
		files {
			"deps/cmp/cmp.h",
			"deps/cmp/cmp.c",
		}

	project "linenoise"
		language "C"
		kind "StaticLib"
		files {
			"deps/linenoise/**.h",
			"deps/linenoise/**.c"
		}
		excludes {
			"deps/linenoise/exaple.c"
		}

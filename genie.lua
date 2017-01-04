solution "lip"
	location(_ACTION)
	configurations {"ReleaseLib", "DebugLib", "ReleaseDLL", "DebugDLL"}
	platforms {"x64"}
	debugdir "."

	flags {
		"StaticRuntime",
		"Symbols",
		"NoEditAndContinue",
		"NoNativeWChar"
	}

	defines {
		"_CRT_SECURE_NO_WARNINGS"
	}

	configuration "Release*"
	    flags { "OptimizeSpeed" }

	configuration "*DLL"
		defines {
			"LIP_DYNAMIC=1"
		}

	configuration "ReleaseLib"
		targetdir "bin/ReleaseLib"

	configuration "ReleaseDLL"
		targetdir "bin/ReleaseDLL"

	configuration "DebugLib"
		targetdir "bin/DebugLib"

	configuration "DebugDLL"
		targetdir "bin/DebugDLL"

	configuration "vs*"
		buildoptions {
			"/wd4116", -- Anonymous struct in parentheses
			"/wd4200", -- Flexible array member
			"/wd4204", -- Non-constant aggregate initializer
		}

	configuration {}

	project "repl"
		kind "ConsoleApp"
		language "C"

		files {
			"src/repl/*.h",
			"src/repl/*.c"
		}

		includedirs {
			"include",
			"deps/linenoise-ng/include",
			"deps/cargo"
		}

		links {
			"lip",
            "cargo",
            "linenoise-ng"
		}

	project "tests"
		kind "ConsoleApp"
		language "C"

		defines {
			"LIP_TEST_SKIP_CPP"
		}

		includedirs {
			"include",
            "src"
        }

		files {
			"src/tests/*.h",
			"src/tests/*.c"
		}

		links {
			"lip"
        }

	project "cpp-tests"
		kind "SharedLib"
		language "C++"

		includedirs {
			"include"
        }

		files {
			"src/tests/*.cpp"
		}

		links {
			"lip"
        }

	project "lip"
		configuration { "*Lib" }
			kind "StaticLib"

		configuration { "*DLL" }
			kind "SharedLib"

		configuration {}

		language "C"

		defines {
			"LIP_BUILDING=1"
		}

		includedirs {
			"include"
		}

		files {
			"include/lip/*.h",
			"src/lip/**"
		}

	project "linenoise-ng"
		kind "StaticLib"
		language "C++"

		flags {
			"MinimumWarnings"
		}

		includedirs {
			"deps/linenoise-ng/include",
		}

		files {
			"deps/linenoise-ng/include/*.h",
			"deps/linenoise-ng/src/*.cpp"
		}

	project "cargo"
		kind "StaticLib"
		language "C"

		flags {
			"MinimumWarnings"
		}

		files {
			"deps/cargo/*.h",
			"deps/cargo/*.c"
		}

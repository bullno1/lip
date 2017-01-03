solution "lip"
	location(_ACTION)
	configurations {"Release", "Debug"}
	platforms {"x64"}
	targetdir "bin"
	debugdir "."

	flags {
		"StaticRuntime",
		"Symbols",
		"NoEditAndContinue",
		"NoNativeWChar",
		"NoExceptions"
	}

	defines {
		"_CRT_SECURE_NO_WARNINGS"
	}

	configuration "Release"
	    flags { "OptimizeSpeed" }

	configuration "Debug"
	    targetsuffix "_d"

	project "repl"
		kind "ConsoleApp"
		language "C"
		targetname "lip"

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

		includedirs {
			"include",
            "src"
        }

		files {
			"src/tests/*.h",
			"src/tests/*.c",
			"src/tests/*.cpp"
		}

		links {
			"lip"
        }

	project "lip"
		kind "StaticLib"
		language "C"

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

		files {
			"deps/cargo/*.h",
			"deps/cargo/*.c"
		}

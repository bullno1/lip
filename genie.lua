solution "lip"
	location(_ACTION)
	configurations {"Release", "Debug"}
	platforms {"x64"}
	targetdir "bin"
	debugdir "."

	flags {
		"ExtraWarnings",
		"FatalWarnings",
		"StaticRuntime",
		"Symbols",
		"NoEditAndContinue",
		"NoNativeWChar",
		"NoExceptions"
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
			"src"
		}

		links {
			"liblip"
		}

	project "liblip"
		kind "StaticLib"
		language "C"

		defines {
			"_CRT_SECURE_NO_WARNINGS"
		}

		files {
			"src/lip/*.h",
			"src/lip/*.c"
		}


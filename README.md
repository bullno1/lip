# lip - An embeddable LISP

`lip` is my own implementation of a LISP language, designed to be embedded in a host program (similar to Lua).
It is written using C99.

Currently, `lip` is still under heavy development.

## How to build

### Using the provided scripts

An Unix environment, Python and a modern C99 compiler is required.
From the project folder, run: `./numake bin/lip` and an interpreter will be created at `bin/lip`.

`numake` is my own custom build tool: https://github.com/bullno1/numake

`./watch bin/lip` will watch all dependencies of `bin/lip` and rebuild if needed.

### Using Visual Studio 2015

Visual Studio 2015 is the only Visual Studio version that has proper C99 supports, anything earlier probably will not work.
Run `vs2015.bat` and a solution will be created in the `vs2015` folder.

### Using GENie

[GENie](https://github.com/bkaradzic/genie) is a project generator for XCode, Visual Studio and Makefile.
Run it on `genie.lua` to create a project for your tool.

### Using your own tools

It is also possible to build using other build tools although it's untested.

To build liblip.a (the library), add all files under `src/lip` into your build tool/IDE.
They should be compiled as a static C library, not a C++ one.

To build the interpreter, add all files under `src/repl` into your build tool/IDE, set header include path to `src` and link the executable with liblip.a built earlier.

## What works currently

- The interpreter can execute simple scripts (see [examples](examples)).
  To run a script, pipe it into the interpreter (e.g: `bin/lip < examples/fib.lip`).
  Run `bin/lip --help` for some options (not all are implemented).
- The current speed is around that of interpreted Lua and sometimes slightly faster.
- A simple interface to bind C functions to the runtime (see [src/repl/main.c](src/repl/main.c)).

## What is planned?

- A simple error handling system similar to Lua's pcall/error.
  This means no polymorphic exception nonsense.
  Exception should only be used for panic cases.
- A module/package system similar to that of Common Lisp or Clojure.
- A type system.
- A minimal standard library.

## Why another language?

Lua is one of the best embeddable scripting language available, however there are a few parts I am not happy with:

- *Treatment of globals*: They are silently treated as `nil` and cause problems much later when used.
  It is possible to setup metatables such that they throw errors but the erroneous code still have to be executed.
  In `lip`, undefined globals would be an error at load-time or compile-time.
- *Lack of typing*: Even in a dynamic environment, type checking techniques can still be used.
- *Macros*: I am not a big fan of macros but macros are good for building DSLs.
  Having used Lua for data declaration, I like its syntactic sugar for unary function with strings and table (i.e: the parentheses can be omitted: `func "str arg"` or `func {key = value}`).
  However, its anonymous function syntax is too verbose: `function(arg1, arg2) boyd end`.
  Good macro support will make DSLs much nicer.
- *Garbage collector*: It has become a problem in multiple projects.
  Sometimes, it's because GC pauses for too long.
  Sometimes, it's because developer forgot to mark objects (and it's not easy with Lua).
  This is my chance to experiment with two ideas:
  1. A scripting language and runtime for game development that does not use garbage collection.
     Details will be revealed later.
  2. Pluggable garbage collection: There can be several type of GC: incremental, stop-the-world or just reference counting.
     An developer can choose which fits a project the most.
     Of course, programs written will have to take the plugged GC into account (e.g: you cannot create cycle in a reference counting collector).

There are also areas I want to experiment with:

- *Hackable runtime for scripting languages*: Many scripting languages such as Lua does not expose its underlying infrastructure enough for language hackers.
  To target the Lua VM, one would have to read [a document written by a third-party](http://luaforge.net/docman/83/98/ANoFrillsIntroToLua51VMInstructions.pdf).
  Making languages which compile to Lua but have line information and source name map nicely to the source language for ease of debugging is a challenging task
  There is no simple way to access Lua's compiler or AST.
  lip is designed to be modular and hackable from the start.
  There is API (albeit undocumented for now) for [codegen](src/lip/asm.h), [ast](src/lip/ast.h), [parser](src/lip/parser.h) and [compiler](src/lip/compiler.h).
  In fact, every part of it can be used independently (e.g: one can use lip's sexp parser to build their own language in their own runtime or parse their own language and generate corresponding s-expressions to feed into lip's compiler).
- *Type system*: Even in a dynamic environment, there are several type inference and checking techniques that can improve performances (by removing type checks) and improve correctness (by catching error at compile-time instead of runtime).
  lip is an attempt to explore type system in an embedded and dynamic environment.
- *Hot code reloading*: Among one of the many things which [GOAL](https://en.wikipedia.org/wiki/Game_Oriented_Assembly_Lisp) is famous for is its ability to reload code on-the-fly.
  This feature can already be simulated in many current scripting languages.
  However, I want to experiment with a runtime where this is designed as as a first-class feature.
  [Erlang](http://www.erlang.org/) is a language where this is a central design.

Building `lip` is also an exercise in language design and implementation as well as type system.
Choosing C99 instead of other languages means I could immediately use it in many of my own projects as find about the language's strength and weaknesses.

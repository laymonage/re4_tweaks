#pragma once
#define WIN32_LEAN_AND_MEAN
#define _USE_MATH_DEFINES

#include <iomanip>
#include <injector\injector.hpp>
#include <injector\calling.hpp>
#include <injector\hooking.hpp>
#include <injector\assembly.hpp>
#include <injector\utility.hpp>
#include <MemoryMgr.h>
#include <log.h>
#include <filesystem>
#include <string>
#include "Sections.h"

using namespace Memory::VP;

extern bool TweaksDevMode; // CommandLine.cpp, enabled in DEBUG build & with -dev command-line param

#define VERBOSE
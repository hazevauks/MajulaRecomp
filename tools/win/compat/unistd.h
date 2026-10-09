// Stand-in for <unistd.h> when a host tool is built with the PC toolchain (clang targeting MSVC): the POSIX file
// calls the shader tools use (open, write, close) are in <io.h> there. Used by shaders/tools/*.cpp through
// -Itools/win/compat; other toolchains have the real header and never see this one.
#pragma once
#include <io.h>
#include <sys/types.h>
#include <sys/stat.h>
#if !defined(_SSIZE_T_DEFINED)
#define _SSIZE_T_DEFINED
typedef long long ssize_t;
#endif

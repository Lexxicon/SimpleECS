#pragma once
#include <cstdarg>
#include <cstdio>
#include <cstdlib>

inline void Error(const char* Format, ...)
{
    va_list Arglist;
    va_start( Arglist, Format );
    vprintf( Format, Arglist );
    va_end( Arglist );
    abort();
}

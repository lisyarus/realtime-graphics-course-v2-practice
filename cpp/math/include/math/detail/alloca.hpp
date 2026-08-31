#pragma once

#if defined(_WIN32)
#include <malloc.h>
#define math_alloca(Type, count) \
    static_cast<Type *>(_alloca((count) * sizeof(Type)))
#else
#include <alloca.h>
#define math_alloca(Type, count) \
    static_cast<Type *>(alloca((count) * sizeof(Type)))
#endif

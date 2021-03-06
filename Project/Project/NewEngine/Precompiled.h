#ifndef PRECOMPILED_H__
#define PRECOMPILED_H__

// Referred to the https://github.com/PolygonTek/BlueshiftEngine/blob/master/Source/Runtime/Precompiled.h

// detect x86 32 bit platform
#if defined(__i386__) || defined(_M_IX86)
#if !defined(__X86__)
#define __X86__
#endif
#if !defined(__X86_32__)
#define __X86_32__
#endif
#endif

// detect x86 64 bit platform
#if defined(__x86_64__) || defined(__ia64__) || defined(_M_X64)
#if !defined(__X86__)
#define __X86__
#endif
#if !defined(__X86_64__)
#define __X86_64__
#endif
#endif

#if (defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__NT__)) && !defined(__CYGWIN__)
#if !defined(__WIN32__)
#define __WIN32__
#endif
#endif

#if defined(_DEBUG) && !defined(CG_DEBUG)
#define CG_DEBUG
#else
#define CG_RELEASE
#endif

#if defined(CG_DEBUG)
#if defined(__WIN32__) && defined(_MSC_VER)
#include <iostream>
#include <assert.h>
#include <intrin.h>
#define CG_DEBUG_ASSERT(x) if(!(x)) { std::cout << "Debug Assert | " << __FILE__ << " ( " << __LINE__ << " )", assert((0)); }
#define CG_DEBUG_BREAK(x) if(!(x)) {std::cout << "Debug Break | " << __FILE__ << " ( " << __LINE__ << " ) ", __debugbreak();}
#endif
#elif defined(CG_RELEASE)
#define CG_DEBUG_ASSERT(x)
#define CG_DEBUG_BREAK(x)
#endif

#include <cmath>

#endif
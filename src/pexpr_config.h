#pragma once

#define PEXPR_STRINGIFY(str) #str
#define PEXPR_DOUBLEQUOTE(str) PEXPR_STRINGIFY(str)

// OS
#if defined(__linux) || defined(linux)
#define PEXPR_OS_LINUX
#elif defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__WINDOWS__) || defined(__TOS_WIN__)
#define PEXPR_OS_WINDOWS
#if !defined(Win64) && !defined(_WIN64)
#define PEXPR_OS_WINDOWS_32
#else
#define PEXPR_OS_WINDOWS_64
#endif
#elif defined(__APPLE__)
#define PEXPR_OS_APPLE
#else
#error Your operating system is currently not supported
#endif

// Compiler
#if defined(__CYGWIN__)
#define PEXPR_CC_CYGWIN
#endif

#if defined(__GNUC__)
#define PEXPR_CC_GNU
#endif

#if defined(__clang__)
#define PEXPR_CC_CLANG
#endif

#if defined(__MINGW32__)
#define PEXPR_CC_MINGW32
#endif

#if defined(__INTEL_COMPILER)
#define PEXPR_CC_INTEL
#endif

#if defined(_MSC_VER)
#define PEXPR_CC_MSC
#pragma warning(disable : 4251 4996)
#endif

// Check if C++17
#ifdef PEXPR_CC_MSC
#define PEXPR_CPP_LANG _MSVC_LANG
#else
#define PEXPR_CPP_LANG __cplusplus
#endif

#if PEXPR_CPP_LANG < 201703L
#pragma warning Ignis requires C++ 17 to compile successfully
#endif

#define PEXPR_PRAGMA(x) _Pragma(#x)

// clang-format off
#define PEXPR_UNUSED(expr) do { (void)(expr); } while (false)
#define PEXPR_NOOP do {} while(false)
// clang-format on

#ifdef PEXPR_CC_MSC
#define PEXPR_DEBUG_BREAK() __debugbreak()
#define PEXPR_FUNCTION_NAME __FUNCSPEXPR__
#define PEXPR_BEGIN_IGNORE_WARNINGS PEXPR_PRAGMA(warning(push, 0))
#define PEXPR_END_IGNORE_WARNINGS PEXPR_PRAGMA(warning(pop))
#else // FIXME: Really use cpu dependent assembler?
#define PEXPR_DEBUG_BREAK() __asm__ __volatile__("int $0x03")
#define PEXPR_FUNCTION_NAME __PRETTY_FUNCTION__
#define PEXPR_BEGIN_IGNORE_WARNINGS
#define PEXPR_END_IGNORE_WARNINGS
#endif

#if defined(PEXPR_CC_GNU) || defined(PEXPR_CC_CLANG)
#define PEXPR_LIKELY(x) __builtin_expect(!!(x), 1)
#define PEXPR_UNLIKELY(x) __builtin_expect(!!(x), 0)
#define PEXPR_MAYBE_UNUSED __attribute__((unused))
#else
#define PEXPR_LIKELY(x) (x)
#define PEXPR_UNLIKELY(x) (x)
#define PEXPR_MAYBE_UNUSED
#endif

#define PEXPR_RESTRICT __restrict

#if defined(PEXPR_WITH_ASSERTS) || (!defined(PEXPR_NO_ASSERTS) && defined(PEXPR_DEBUG))
#include <assert.h>
#define _PEXPR_ASSERT_MSG(msg)                                 \
    std::cerr << "[IGNIS] ASSERT | " << __FILE__               \
              << ":" << __LINE__ << " " << PEXPR_FUNCTION_NAME \
              << " | " << (msg) << std::endl
#ifndef PEXPR_DEBUG
#define PEXPR_ASSERT(cond, msg)        \
    do {                               \
        if (PEXPR_UNLIKELY(!(cond))) { \
            _PEXPR_ASSERT_MSG((msg));  \
            std::abort();              \
        }                              \
    } while (false)
#else
#define PEXPR_ASSERT(cond, msg)        \
    do {                               \
        if (PEXPR_UNLIKELY(!(cond))) { \
            _PEXPR_ASSERT_MSG((msg));  \
            PEXPR_DEBUG_BREAK();       \
            std::abort();              \
        }                              \
    } while (false)
#endif
#else
#define PEXPR_ASSERT(cond, msg) ((void)0)
#endif

#define PEXPR_CLASS_NON_MOVEABLE(C) \
private:                            \
    C(C&&)     = delete;            \
    C& operator=(C&&) = delete

#define PEXPR_CLASS_NON_COPYABLE(C) \
private:                            \
    C(const C&) = delete;           \
    C& operator=(const C&) = delete

#define PEXPR_CLASS_NON_CONSTRUCTABLE(C) \
private:                                 \
    C() = delete

#define PEXPR_CLASS_STACK_ONLY(C)                  \
private:                                           \
    static void* operator new(size_t)    = delete; \
    static void* operator new[](size_t)  = delete; \
    static void operator delete(void*)   = delete; \
    static void operator delete[](void*) = delete

#if defined(PEXPR_CC_GNU) || defined(PEXPR_CC_CLANG)
#define PEXPR_NO_SANITIZE_ADDRESS __attribute__((no_sanitize_address))
#define PEXPR_NO_SANITIZE_THREAD __attribute__((no_sanitize_thread))
#define PEXPR_NO_SANITIZE_UNDEFINED __attribute__((no_sanitize_undefined))
#else
#define PEXPR_NO_SANITIZE_ADDRESS
#define PEXPR_NO_SANITIZE_THREAD
#define PEXPR_NO_SANITIZE_UNDEFINED
#endif

#if defined(PEXPR_CC_MSC)
#define PEXPR_EXPORT __declspec(dllexport)
#define PEXPR_IMPORT __declspec(dllimport)
#elif defined(PEXPR_CC_GNU) || defined(PEXPR_CC_CLANG)
#define PEXPR_EXPORT __attribute__((visibility("default")))
#define PEXPR_IMPORT
#else
#error Unsupported compiler
#endif

// Useful includes
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <memory>
#include <optional>
#include <unordered_map>
#include <vector>

namespace PExpr {
/* The intx_t are optional but our system demands them.
 * A system without these types can not use the raytracer
 * due to performance issues anyway.
 */

using int8  = int8_t;
using uint8 = uint8_t;

using int16  = int16_t;
using uint16 = uint16_t;

using int32  = int32_t;
using uint32 = uint32_t;

using int64  = int64_t;
using uint64 = uint64_t;

using Integer = uint64;
using Number  = double;

/* Checks */
static_assert(sizeof(int8) == 1, "Invalid bytesize configuration");
static_assert(sizeof(uint8) == 1, "Invalid bytesize configuration");
static_assert(sizeof(int16) == 2, "Invalid bytesize configuration");
static_assert(sizeof(uint16) == 2, "Invalid bytesize configuration");
static_assert(sizeof(int32) == 4, "Invalid bytesize configuration");
static_assert(sizeof(uint32) == 4, "Invalid bytesize configuration");
static_assert(sizeof(int64) == 8, "Invalid bytesize configuration");
static_assert(sizeof(uint64) == 8, "Invalid bytesize configuration");

/* Precise vector types */
using Vector2f = std::array<float, 2>;
using Vector3f = std::array<float, 2>;
using Vector4f = std::array<float, 2>;

/* Useful constants */
constexpr float FltEps = std::numeric_limits<float>::epsilon();
constexpr float FltInf = std::numeric_limits<float>::infinity();
constexpr float FltMax = std::numeric_limits<float>::max();

constexpr float Pi     = 3.14159265358979323846f;
constexpr float InvPi  = 0.31830988618379067154f; // 1/pi
constexpr float Inv2Pi = 0.15915494309189533577f; // 1/(2pi)
constexpr float Inv4Pi = 0.07957747154594766788f; // 1/(4pi)
constexpr float Pi2    = 1.57079632679489661923f; // pi half
constexpr float Pi4    = 0.78539816339744830961f; // pi quarter
constexpr float Sqrt2  = 1.41421356237309504880f;

constexpr float Deg2Rad = Pi / 180.0f;
constexpr float Rad2Deg = 180.0f * InvPi;

/// Clamps a between b and c.
template <typename T>
inline T clamp(const T& a, const T& b, const T& c)
{
    return (a < b) ? b : ((a > c) ? c : a);
}

/// Transform string to lowercase
inline std::string to_lowercase(const std::string& str)
{
    std::string tmp = str;
    std::transform(tmp.begin(), tmp.end(), tmp.begin(), [](const std::string::value_type& v) { return static_cast<std::string::value_type>(::tolower((int)v)); });
    return tmp;
}
} // namespace PExpr
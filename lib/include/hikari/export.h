#pragma once

#if defined(_MSC_VER)
#   define HIKARI_API_IMPORT __declspec(dllimport)
#   define HIKARI_API_EXPORT __declspec(dllexport)
#   define HIKARI_SUPPRESS_EXPORT_WARNING __pragma(warning(push)) __pragma(warning(disable: 4251 4275))
#   define HIKARI_RESTORE_EXPORT_WARNING __pragma(warning(pop))
#elif defined(__GNUC__)
#   define HIKARI_API_IMPORT
#   define HIKARI_API_EXPORT __attribute__((visibility("default")))
#   define HIKARI_SUPPRESS_EXPORT_WARNING
#   define HIKARI_RESTORE_EXPORT_WARNING
#else
#   define HIKARI_API_IMPORT
#   define HIKARI_API_EXPORT
#   define HIKARI_SUPPRESS_EXPORT_WARNING
#   define HIKARI_RESTORE_EXPORT_WARNING
#endif

#if defined(HIKARI_BUILD_SHARED)
#   ifdef HIKARI_EXPORT_SHARED
#       define HIKARI_API HIKARI_API_EXPORT
#   else
#       define HIKARI_API HIKARI_API_IMPORT
#   endif
#else
#   define HIKARI_API
#endif

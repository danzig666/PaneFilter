#pragma once

// Per-plugin settings for Updater.cpp.

#include <array>

#include "core/Json.hpp"
#include "core/Text.hpp"
#include "version.hpp"

namespace updcfg {
namespace core = ::gh;
inline constexpr const char* kOwner = "danzig666";
inline constexpr const char* kRepo = "PaneFilter";
// Release asset, plus "<name>.sha256". Saved under the name of the running DLL.
#ifdef _WIN64
inline constexpr const char* kDllName = "PaneFilter.x64.dll";
#else
inline constexpr const char* kDllName = "PaneFilter.x86.dll";
#endif
// Files next to the DLL that are replaced together with it (each also has a "<name>.sha256" asset).
inline constexpr std::array<const char*, 2> kExtraFiles = {"PaneFilter_Eng.lng", "PaneFilter_Eng.hlf"};
inline constexpr const char* kVersion = PLUGIN_VERSION;
inline constexpr const wchar_t* kProduct = L"PaneFilter";
}  // namespace updcfg

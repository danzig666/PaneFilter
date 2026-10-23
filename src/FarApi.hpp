#pragma once

#include <windows.h>

#include <string>
#include <vector>

#include "plugin.hpp"

// Far services and the message helpers used by the updater.

extern PluginStartupInfo PsInfo;
extern FarStandardFunctions FSF;

namespace fui {

// Shows a message box. `lines[0]` is the title. Returns the index of the
// pressed button, or -1 on Esc.
intptr_t Message(FARMESSAGEFLAGS flags, const std::vector<std::wstring>& lines,
                 const std::vector<std::wstring>& buttons, const GUID& id = GUID{});

void ShowError(const std::wstring& title, const std::wstring& text);
void ShowInfo(const std::wstring& title, const std::wstring& text);

}  // namespace fui

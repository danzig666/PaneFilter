#include "FarApi.hpp"

#include "Guids.hpp"

PluginStartupInfo PsInfo;
FarStandardFunctions FSF;

namespace fui {

intptr_t Message(FARMESSAGEFLAGS flags, const std::vector<std::wstring>& lines,
                 const std::vector<std::wstring>& buttons, const GUID& id) {
  std::vector<const wchar_t*> items;
  items.reserve(lines.size() + buttons.size());
  for (const auto& line : lines) items.push_back(line.c_str());
  for (const auto& button : buttons) items.push_back(button.c_str());
  return PsInfo.Message(&MainGuid, &id, flags, nullptr, items.data(), items.size(),
                        static_cast<intptr_t>(buttons.size()));
}

namespace {
std::vector<std::wstring> SplitLines(const std::wstring& title, const std::wstring& text) {
  std::vector<std::wstring> lines{title};
  size_t start = 0;
  for (;;) {
    size_t nl = text.find(L'\n', start);
    lines.push_back(text.substr(start, nl == std::wstring::npos ? std::wstring::npos : nl - start));
    if (nl == std::wstring::npos) break;
    start = nl + 1;
  }
  return lines;
}
}  // namespace

void ShowError(const std::wstring& title, const std::wstring& text) {
  Message(FMSG_WARNING, SplitLines(title, text), {L"OK"}, MessageGuid);
}

void ShowInfo(const std::wstring& title, const std::wstring& text) {
  Message(FMSG_LEFTALIGN, SplitLines(title, text), {L"OK"}, MessageGuid);
}

}  // namespace fui

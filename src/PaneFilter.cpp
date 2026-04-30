#include <windows.h>
#include <strsafe.h>

#include <algorithm>
#include <cwctype>
#include <string>
#include <vector>

#include "plugin.hpp"
#include "guid.hpp"
#include "FarLang.h"
#include "version.hpp"

namespace
{
	struct SourceEntry
	{
		std::wstring Name;
		std::wstring FullPath;
		unsigned long long FileSize;
		FILETIME LastWriteTime;
		uintptr_t FileAttributes;
		bool IsDirectory;
	};

	struct PanelItemSnapshot
	{
		std::wstring FileName;
		uintptr_t FileAttributes;
	};

	struct PanelState
	{
		std::wstring SourceDirectory;
		std::wstring OriginalDirectory;
		std::wstring CurrentName;
		std::wstring FilterText;
		std::wstring PanelTitle;
		std::vector<SourceEntry> Entries;
		std::vector<size_t> FilteredIndices;
		bool ShowHidden;
		OPENPANELINFO_SORTMODES SortMode;
		intptr_t SortOrder;
	};

	PluginStartupInfo Info{};
	FarStandardFunctions FSF{};

	const wchar_t* const PanelColumnTitles[] = {nullptr, nullptr, nullptr};
	const PanelMode PanelModes[] =
	{
		{L"N,S,DM", L"0,10,17", PanelColumnTitles, L"N,S,DM", L"0,8,17", 0}
	};

	const wchar_t* GetMsg(int id)
	{
		return Info.GetMsg(&MainGuid, id);
	}

	void ShowMessage(const std::vector<const wchar_t*>& lines)
	{
		Info.Message(&MainGuid, &MessageGuid, FMSG_LEFTALIGN | FMSG_WARNING | FMSG_MB_OK, nullptr, lines.data(), lines.size(), 0);
	}

	bool IsPathSeparator(wchar_t ch)
	{
		return ch == L'\\' || ch == L'/';
	}

	std::wstring JoinPath(const std::wstring& directory, const std::wstring& name)
	{
		if (directory.empty())
			return name;

		if (IsPathSeparator(directory.back()))
			return directory + name;

		return directory + L'\\' + name;
	}

	std::wstring ParentPath(const std::wstring& path)
	{
		if (path.size() <= 3)
			return path;

		size_t end = path.size();
		while (end > 0 && IsPathSeparator(path[end - 1]))
			--end;

		const size_t slash = path.find_last_of(L"\\/", end - 1);
		if (slash == std::wstring::npos)
			return path;

		if (slash <= 2)
			return path.substr(0, slash + 1);

		return path.substr(0, slash);
	}

	wchar_t NormalizeFilterChar(wchar_t ch)
	{
		return static_cast<wchar_t>(FSF.LLower(ch));
	}

	std::wstring TrimWhitespace(const std::wstring& text)
	{
		size_t start = 0;
		while (start < text.size() && iswspace(text[start]))
			++start;

		size_t end = text.size();
		while (end > start && iswspace(text[end - 1]))
			--end;

		return text.substr(start, end - start);
	}

	std::wstring NormalizeFilterText(const std::wstring& rawText)
	{
		std::wstring text = TrimWhitespace(rawText);
		for (wchar_t& ch : text)
			ch = NormalizeFilterChar(ch);
		return text;
	}

	std::wstring ExtractFilterFromKeyText(const std::wstring& rawText)
	{
		std::wstring text = NormalizeFilterText(rawText);
		if (text.empty())
			return text;

		if (text.find(L'*') != std::wstring::npos || text.find(L'?') != std::wstring::npos)
			return text;

		if (text.size() == 1)
			return text;

		for (size_t index = text.size(); index > 0; --index)
		{
			const wchar_t ch = text[index - 1];
			if ((iswalnum(ch) != 0) || ((iswprint(ch) != 0) && (iswspace(ch) == 0) && ch != L'+' && ch != L'-'))
				return std::wstring(1, ch);
		}

		return text;
	}

	bool TryExtractFilterFromKeyDescriptor(const std::wstring& rawText, std::wstring& filter)
	{
		std::wstring text = NormalizeFilterText(rawText);
		if (text.empty())
			return false;

		bool hasModifier = false;
		for (;;)
		{
			if (text.size() > 3 && text.compare(0, 3, L"alt") == 0)
			{
				text.erase(0, 3);
				hasModifier = true;
				continue;
			}

			if (text.size() > 4 && text.compare(0, 4, L"ctrl") == 0)
			{
				text.erase(0, 4);
				hasModifier = true;
				continue;
			}

			if (text.size() > 5 && text.compare(0, 5, L"shift") == 0)
			{
				text.erase(0, 5);
				hasModifier = true;
				continue;
			}

			break;
		}

		if (!hasModifier || text.empty())
			return false;

		if (text.size() != 1 && text.find(L'*') == std::wstring::npos && text.find(L'?') == std::wstring::npos)
			return false;

		filter = ExtractFilterFromKeyText(text);
		return !filter.empty();
	}

	bool ContainsWildcardChars(const std::wstring& filter)
	{
		return filter.find(L'*') != std::wstring::npos || filter.find(L'?') != std::wstring::npos;
	}

	std::wstring BuildWildcardPattern(const std::wstring& filter)
	{
		if (filter.empty())
			return filter;

		std::wstring pattern = filter;
		if (pattern.front() != L'*')
			pattern.insert(pattern.begin(), L'*');
		if (pattern.back() != L'*')
			pattern.push_back(L'*');
		return pattern;
	}

	const wchar_t* StrIContains(const wchar_t* text, const std::wstring& needle)
	{
		if (!text)
			return nullptr;

		if (needle.empty())
			return text;

		const size_t needleLength = needle.size();
		for (const wchar_t* cursor = text; *cursor; ++cursor)
		{
			if (FSF.LStrnicmp(cursor, needle.c_str(), needleLength) == 0)
				return cursor;
		}

		return nullptr;
	}

	bool WildcardMatchI(const wchar_t* text, const wchar_t* pattern)
	{
		const wchar_t* star = nullptr;
		const wchar_t* retryText = nullptr;

		while (*text)
		{
			if (*pattern == L'*')
			{
				star = pattern++;
				retryText = text;
				continue;
			}

			if (*pattern == L'?' || NormalizeFilterChar(*pattern) == NormalizeFilterChar(*text))
			{
				++pattern;
				++text;
				continue;
			}

			if (star)
			{
				pattern = star + 1;
				text = ++retryText;
				continue;
			}

			return false;
		}

		while (*pattern == L'*')
			++pattern;

		return *pattern == 0;
	}

	bool MatchesFilter(const std::wstring& name, const std::wstring& filter)
	{
		if (filter.empty())
			return true;

		if (ContainsWildcardChars(filter))
		{
			const std::wstring pattern = BuildWildcardPattern(filter);
			return WildcardMatchI(name.c_str(), pattern.c_str());
		}

		return StrIContains(name.c_str(), filter) != nullptr;
	}

	bool HasPanels()
	{
		return Info.PanelControl(PANEL_ACTIVE, FCTL_CHECKPANELSEXIST, 0, nullptr) != FALSE;
	}

	bool GetActivePanelInfo(PanelInfo& panelInfo)
	{
		panelInfo = {};
		panelInfo.StructSize = sizeof(panelInfo);
		return Info.PanelControl(PANEL_ACTIVE, FCTL_GETPANELINFO, 0, &panelInfo) != FALSE;
	}

	bool GetPanelDirectory(HANDLE panel, std::wstring& directory)
	{
		const intptr_t requiredSize = Info.PanelControl(panel, FCTL_GETPANELDIRECTORY, 0, nullptr);
		if (requiredSize <= 0)
			return false;

		std::vector<unsigned char> buffer(static_cast<size_t>(requiredSize));
		FarPanelDirectory* panelDirectory = reinterpret_cast<FarPanelDirectory*>(buffer.data());
		panelDirectory->StructSize = sizeof(FarPanelDirectory);

		if (!Info.PanelControl(panel, FCTL_GETPANELDIRECTORY, requiredSize, panelDirectory))
			return false;

		directory = panelDirectory->Name ? panelDirectory->Name : L"";
		return true;
	}

	bool GetPanelItem(HANDLE panel, size_t index, PanelItemSnapshot& panelItem)
	{
		const intptr_t requiredSize = Info.PanelControl(panel, FCTL_GETPANELITEM, index, nullptr);
		if (requiredSize <= 0)
			return false;

		std::vector<unsigned char> buffer(static_cast<size_t>(requiredSize));
		FarGetPluginPanelItem panelItemInfo{};
		panelItemInfo.StructSize = sizeof(panelItemInfo);
		panelItemInfo.Size = static_cast<size_t>(requiredSize);
		panelItemInfo.Item = reinterpret_cast<PluginPanelItem*>(buffer.data());

		if (!Info.PanelControl(panel, FCTL_GETPANELITEM, index, &panelItemInfo))
			return false;

		if (!panelItemInfo.Item->FileName)
			return false;

		panelItem.FileName = panelItemInfo.Item->FileName;
		panelItem.FileAttributes = panelItemInfo.Item->FileAttributes;
		return true;
	}

	bool GetActivePanelItem(size_t index, PanelItemSnapshot& panelItem)
	{
		return GetPanelItem(PANEL_ACTIVE, index, panelItem);
	}

	std::wstring EscapeLuaString(const std::wstring& text)
	{
		std::wstring escaped;
		escaped.reserve(text.size() + 8);
		for (const wchar_t ch : text)
		{
			switch (ch)
			{
			case L'\\':
				escaped += L"\\\\";
				break;
			case L'"':
				escaped += L"\\\"";
				break;
			case L'\r':
				escaped += L"\\r";
				break;
			case L'\n':
				escaped += L"\\n";
				break;
			case L'\t':
				escaped += L"\\t";
				break;
			default:
				escaped += ch;
				break;
			}
		}
		return escaped;
	}

	bool TryExtractFilterFromMacroValue(const FarMacroValue& value, std::wstring& filter)
	{
		switch (value.Type)
		{
		case FMVT_STRING:
			if (value.String && *value.String)
			{
				filter = ExtractFilterFromKeyText(value.String);
				return !filter.empty();
			}
			return false;

		case FMVT_INTEGER:
			if (value.Integer >= 1 && value.Integer <= 0x10FFFF)
			{
				filter = ExtractFilterFromKeyText(std::wstring(1, static_cast<wchar_t>(value.Integer)));
				return !filter.empty();
			}
			return false;

		case FMVT_DOUBLE:
			if (value.Double >= 1.0 && value.Double <= 0x10FFFF)
			{
				filter = ExtractFilterFromKeyText(std::wstring(1, static_cast<wchar_t>(value.Double)));
				return !filter.empty();
			}
			return false;

		default:
			return false;
		}
	}

	std::wstring GetStartupFilter(const OpenInfo* openInfo)
	{
		if (!openInfo)
			return L"";

		if ((openInfo->OpenFrom == OPEN_FROMMACRO || openInfo->OpenFrom == OPEN_LUAMACRO) && openInfo->Data)
		{
			const OpenMacroInfo* macroInfo = reinterpret_cast<const OpenMacroInfo*>(openInfo->Data);
			if (macroInfo->Values && macroInfo->Count)
			{
				for (size_t index = macroInfo->Count; index > 0; --index)
				{
					std::wstring filter;
					if (TryExtractFilterFromMacroValue(macroInfo->Values[index - 1], filter))
						return filter;
				}
			}
		}

		if (openInfo->OpenFrom == OPEN_COMMANDLINE && openInfo->Data)
		{
			const OpenCommandLineInfo* commandInfo = reinterpret_cast<const OpenCommandLineInfo*>(openInfo->Data);
			if (commandInfo->CommandLine)
			{
				std::wstring filter;
				if (TryExtractFilterFromKeyDescriptor(commandInfo->CommandLine, filter))
					return filter;
				return NormalizeFilterText(commandInfo->CommandLine);
			}
		}

		return L"";
	}

	void RebuildFiltered(PanelState& state)
	{
		state.FilteredIndices.clear();
		for (size_t index = 0; index < state.Entries.size(); ++index)
		{
			if (MatchesFilter(state.Entries[index].Name, state.FilterText))
				state.FilteredIndices.push_back(index);
		}
	}

	bool HasAnyMatchesForFilter(const PanelState& state, const std::wstring& filterText)
	{
		for (const SourceEntry& entry : state.Entries)
		{
			if (MatchesFilter(entry.Name, filterText))
				return true;
		}

		return false;
	}

	void UpdatePanelTitle(PanelState& state)
	{
		wchar_t buffer[512]{};
		StringCchPrintf(
			buffer,
			ARRAYSIZE(buffer),
			GetMsg(MPanelTitle),
			state.FilterText.c_str(),
			state.FilteredIndices.size(),
			state.Entries.size());
		state.PanelTitle = buffer;
	}

	bool EnumerateDirectory(PanelState& state, const std::wstring& directory)
	{
		state.Entries.clear();
		state.FilteredIndices.clear();
		state.SourceDirectory = directory;

		WIN32_FIND_DATAW findData{};
		const std::wstring searchPattern = JoinPath(directory, L"*");
		HANDLE findHandle = FindFirstFileW(searchPattern.c_str(), &findData);
		if (findHandle == INVALID_HANDLE_VALUE)
		{
			UpdatePanelTitle(state);
			return true;
		}

		do
		{
			const wchar_t* fileName = findData.cFileName;
			if (!fileName || fileName[0] == 0)
				continue;

			if (lstrcmpW(fileName, L".") == 0 || lstrcmpW(fileName, L"..") == 0)
				continue;

			if (!state.ShowHidden && (findData.dwFileAttributes & (FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM)) != 0)
				continue;

			SourceEntry entry{};
			entry.Name = fileName;
			entry.FullPath = JoinPath(directory, entry.Name);
			entry.FileSize = (static_cast<unsigned long long>(findData.nFileSizeHigh) << 32) | findData.nFileSizeLow;
			entry.LastWriteTime = findData.ftLastWriteTime;
			entry.FileAttributes = findData.dwFileAttributes;
			entry.IsDirectory = (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
			state.Entries.push_back(entry);
		}
		while (FindNextFileW(findHandle, &findData) != FALSE);

		FindClose(findHandle);

		RebuildFiltered(state);
		UpdatePanelTitle(state);
		return true;
	}

	bool CaptureInitialState(PanelState& state)
	{
		if (!HasPanels())
		{
			ShowMessage({GetMsg(MPluginTitle), GetMsg(MNoPanels), GetMsg(MInstructions)});
			return false;
		}

		PanelInfo panelInfo{};
		if (!GetActivePanelInfo(panelInfo) || panelInfo.PanelType != PTYPE_FILEPANEL)
		{
			ShowMessage({GetMsg(MPluginTitle), GetMsg(MWrongPanelType), GetMsg(MInstructions)});
			return false;
		}

		if (!GetPanelDirectory(PANEL_ACTIVE, state.SourceDirectory))
		{
			ShowMessage({GetMsg(MPluginTitle), GetMsg(MWrongPanelType), GetMsg(MInstructions)});
			return false;
		}

		state.OriginalDirectory = state.SourceDirectory;
		state.ShowHidden = (panelInfo.Flags & PFLAGS_SHOWHIDDEN) != 0;
		state.SortMode = panelInfo.SortMode;
		state.SortOrder = (panelInfo.Flags & PFLAGS_REVERSESORTORDER) != 0 ? 1 : 0;

		PanelItemSnapshot currentItem{};
		if (GetActivePanelItem(panelInfo.CurrentItem, currentItem))
			state.CurrentName = currentItem.FileName;

		return EnumerateDirectory(state, state.SourceDirectory);
	}

	size_t FindVisibleIndexByName(const PanelState& state, const std::wstring& name)
	{
		if (name.empty())
			return 0;

		for (size_t visibleIndex = 0; visibleIndex < state.FilteredIndices.size(); ++visibleIndex)
		{
			if (state.Entries[state.FilteredIndices[visibleIndex]].Name == name)
				return visibleIndex;
		}

		return 0;
	}

	std::wstring GetCurrentVisibleName(HANDLE panel, const PanelState& state)
	{
		PanelInfo panelInfo{};
		panelInfo.StructSize = sizeof(panelInfo);
		if (!Info.PanelControl(panel, FCTL_GETPANELINFO, 0, &panelInfo))
			return L"";

		PanelItemSnapshot panelItem{};
		if (!GetPanelItem(panel, panelInfo.CurrentItem, panelItem))
			return L"";

		return panelItem.FileName;
	}

	void RefreshPanel(PanelState& state, HANDLE panel, const std::wstring& preferredName)
	{
		RebuildFiltered(state);
		UpdatePanelTitle(state);

		Info.PanelControl(panel, FCTL_UPDATEPANEL, 0, reinterpret_cast<void*>(1));

		PanelRedrawInfo redrawInfo{};
		redrawInfo.StructSize = sizeof(redrawInfo);
		if (!state.FilteredIndices.empty())
		{
			redrawInfo.CurrentItem = FindVisibleIndexByName(state, preferredName);
			redrawInfo.TopPanelItem = redrawInfo.CurrentItem > 5 ? redrawInfo.CurrentItem - 5 : 0;
		}
		else
		{
			redrawInfo.CurrentItem = 0;
			redrawInfo.TopPanelItem = 0;
		}

		Info.PanelControl(panel, FCTL_REDRAWPANEL, 0, &redrawInfo);
	}

	bool JumpToItemInActivePanel(const std::wstring& itemName)
	{
		PanelInfo panelInfo{};
		if (!GetActivePanelInfo(panelInfo) || panelInfo.PanelType != PTYPE_FILEPANEL)
			return false;

		for (size_t index = 0; index < panelInfo.ItemsNumber; ++index)
		{
			PanelItemSnapshot panelItem{};
			if (!GetActivePanelItem(index, panelItem))
				continue;

			if (panelItem.FileName == itemName)
			{
				PanelRedrawInfo redrawInfo{};
				redrawInfo.StructSize = sizeof(redrawInfo);
				redrawInfo.CurrentItem = index;
				redrawInfo.TopPanelItem = index > 5 ? index - 5 : 0;
				Info.PanelControl(PANEL_ACTIVE, FCTL_REDRAWPANEL, 0, &redrawInfo);
				return true;
			}
		}

		return false;
	}

	void CloseToDirectory(HANDLE panel, const std::wstring& directory)
	{
		Info.PanelControl(panel, FCTL_CLOSEPANEL, 0, const_cast<wchar_t*>(directory.c_str()));
	}

	void CloseToFile(HANDLE panel, const std::wstring& directory, const std::wstring& fileName)
	{
		CloseToDirectory(panel, directory);
		JumpToItemInActivePanel(fileName);
	}

	void CloseToSelectedItem(HANDLE panel, const std::wstring& directory, const std::wstring& itemName)
	{
		CloseToDirectory(panel, directory);
		if (!itemName.empty())
			JumpToItemInActivePanel(itemName);
	}

	void CloseAndRunLuaMacro(HANDLE panel, const std::wstring& directory, const std::wstring& macroText)
	{
		CloseToDirectory(panel, directory);

		MacroSendMacroText macro{};
		macro.StructSize = sizeof(macro);
		macro.Flags = KMFLAGS_LUA;
		macro.SequenceText = macroText.c_str();
		Info.MacroControl(&MainGuid, MCTL_SENDSTRING, MSSC_POST, &macro);
	}

	void CloseToFileAndOpen(HANDLE panel, const std::wstring& directory, const std::wstring& fileName)
	{
		const std::wstring macroText =
			L"if Panel.SetPath(0, \"" + EscapeLuaString(directory) + L"\", \"" + EscapeLuaString(fileName) +
			L"\") then Keys(\"Enter\") end";
		CloseAndRunLuaMacro(panel, directory, macroText);
	}

	const SourceEntry* FindEntryByName(const PanelState& state, const wchar_t* fileName)
	{
		if (!fileName || !*fileName)
			return nullptr;

		for (const SourceEntry& entry : state.Entries)
		{
			if (entry.Name == fileName)
				return &entry;
		}

		return nullptr;
	}

	bool ExtractInputChar(const KEY_EVENT_RECORD& key, wchar_t& ch)
	{
		const bool ctrlPressed = (key.dwControlKeyState & (LEFT_CTRL_PRESSED | RIGHT_CTRL_PRESSED)) != 0;
		const bool altPressed = (key.dwControlKeyState & (LEFT_ALT_PRESSED | RIGHT_ALT_PRESSED)) != 0;
		const bool altGrPressed = ctrlPressed && altPressed;

		if (!ctrlPressed && key.wVirtualKeyCode == VK_MULTIPLY)
		{
			ch = L'*';
			return true;
		}

		if (altPressed && key.wVirtualKeyCode == VK_OEM_MINUS)
		{
			ch = L'*';
			return true;
		}

		if (key.uChar.UnicodeChar == 0)
		{
			if (ctrlPressed && !altGrPressed)
				return false;

			BYTE keyboardState[256]{};
			if (key.dwControlKeyState & SHIFT_PRESSED)
				keyboardState[VK_SHIFT] = 0x80;

			wchar_t translated[8]{};
			const int translatedCount = ToUnicode(
				key.wVirtualKeyCode,
				key.wVirtualScanCode,
				keyboardState,
				translated,
				ARRAYSIZE(translated),
				0);

			if (translatedCount > 0 && translated[0] >= VK_SPACE)
			{
				ch = translated[0];
				return true;
			}

			if (altPressed)
			{
				ZeroMemory(keyboardState, sizeof(keyboardState));
				if (key.dwControlKeyState & SHIFT_PRESSED)
					keyboardState[VK_SHIFT] = 0x80;
				if (key.dwControlKeyState & (LEFT_ALT_PRESSED | RIGHT_ALT_PRESSED))
					keyboardState[VK_MENU] = 0x80;
				if (key.dwControlKeyState & (LEFT_CTRL_PRESSED | RIGHT_CTRL_PRESSED))
					keyboardState[VK_CONTROL] = 0x80;

				const int altTranslatedCount = ToUnicode(
					key.wVirtualKeyCode,
					key.wVirtualScanCode,
					keyboardState,
					translated,
					ARRAYSIZE(translated),
					0);

				if (altTranslatedCount > 0 && translated[0] >= VK_SPACE)
				{
					ch = translated[0];
					return true;
				}
			}

			return false;
		}

		if (ctrlPressed && !altGrPressed)
			return false;

		if (key.uChar.UnicodeChar < VK_SPACE)
			return false;

		ch = key.uChar.UnicodeChar;
		return true;
	}
}

extern "C" void WINAPI GetGlobalInfoW(GlobalInfo* globalInfo)
{
	globalInfo->StructSize = sizeof(*globalInfo);
	globalInfo->MinFarVersion = MAKEFARVERSION(3, 0, 0, 3192, VS_RELEASE);
	globalInfo->Version = MAKEFARVERSION(PLUGIN_VER_MAJOR, PLUGIN_VER_MINOR, PLUGIN_VER_PATCH, 0, VS_RELEASE);
	globalInfo->Guid = MainGuid;
	globalInfo->Title = L"PaneFilter";
	globalInfo->Description = L"In-pane live filter for the current directory";
	globalInfo->Author = L"D4nz1g";
}

extern "C" void WINAPI SetStartupInfoW(const PluginStartupInfo* startupInfo)
{
	Info = *startupInfo;
	FSF = *startupInfo->FSF;
	Info.FSF = &FSF;
}

extern "C" void WINAPI GetPluginInfoW(PluginInfo* pluginInfo)
{
	static const wchar_t* menuStrings[1];
	menuStrings[0] = GetMsg(MPluginTitle);

	pluginInfo->StructSize = sizeof(*pluginInfo);
	pluginInfo->PluginMenu.Strings = menuStrings;
	pluginInfo->PluginMenu.Count = ARRAYSIZE(menuStrings);
	pluginInfo->PluginMenu.Guids = &PluginMenuGuid;
	pluginInfo->CommandPrefix = L"panefilter";
}

extern "C" HANDLE WINAPI OpenW(const OpenInfo* openInfo)
{
	PanelState* state = new PanelState();
	state->FilterText = GetStartupFilter(openInfo);

	if (!CaptureInitialState(*state))
	{
		delete state;
		return nullptr;
	}

	if (!state->FilterText.empty() && !HasAnyMatchesForFilter(*state, state->FilterText))
	{
		delete state;
		return nullptr;
	}

	RebuildFiltered(*state);
	UpdatePanelTitle(*state);
	return state;
}

extern "C" intptr_t WINAPI GetFindDataW(GetFindDataInfo* findDataInfo)
{
	PanelState* state = static_cast<PanelState*>(findDataInfo->hPanel);
	if (!state)
		return FALSE;

	findDataInfo->ItemsNumber = state->FilteredIndices.size();
	if (state->FilteredIndices.empty())
	{
		findDataInfo->PanelItem = nullptr;
		return TRUE;
	}

	PluginPanelItem* items = static_cast<PluginPanelItem*>(calloc(findDataInfo->ItemsNumber, sizeof(PluginPanelItem)));
	if (!items)
		return FALSE;

	for (size_t visibleIndex = 0; visibleIndex < state->FilteredIndices.size(); ++visibleIndex)
	{
		const SourceEntry& entry = state->Entries[state->FilteredIndices[visibleIndex]];
		items[visibleIndex].FileName = entry.Name.c_str();
		items[visibleIndex].FileSize = entry.FileSize;
		items[visibleIndex].LastWriteTime = entry.LastWriteTime;
		items[visibleIndex].FileAttributes = entry.FileAttributes;
	}

	findDataInfo->PanelItem = items;
	return TRUE;
}

extern "C" void WINAPI FreeFindDataW(const FreeFindDataInfo* freeFindDataInfo)
{
	free(freeFindDataInfo->PanelItem);
}

extern "C" void WINAPI GetOpenPanelInfoW(OpenPanelInfo* openPanelInfo)
{
	PanelState* state = static_cast<PanelState*>(openPanelInfo->hPanel);
	openPanelInfo->StructSize = sizeof(*openPanelInfo);
	openPanelInfo->Flags = OPIF_REALNAMES;
	openPanelInfo->CurDir = state->SourceDirectory.c_str();
	openPanelInfo->Format = L"PaneFilter";
	openPanelInfo->PanelTitle = state->PanelTitle.c_str();
	openPanelInfo->PanelModesArray = PanelModes;
	openPanelInfo->PanelModesNumber = ARRAYSIZE(PanelModes);
	openPanelInfo->StartPanelMode = 0;
	openPanelInfo->StartSortMode = state->SortMode;
	openPanelInfo->StartSortOrder = state->SortOrder;
	openPanelInfo->ShortcutData = state->SourceDirectory.c_str();
}

extern "C" intptr_t WINAPI ProcessPanelInputW(const ProcessPanelInputInfo* processInfo)
{
	PanelState* state = static_cast<PanelState*>(processInfo->hPanel);
	if (!state || processInfo->Rec.EventType != KEY_EVENT || !processInfo->Rec.Event.KeyEvent.bKeyDown)
		return FALSE;

	const KEY_EVENT_RECORD& key = processInfo->Rec.Event.KeyEvent;
	const std::wstring selectedName = GetCurrentVisibleName(processInfo->hPanel, *state);
	const bool ctrlPressed = (key.dwControlKeyState & (LEFT_CTRL_PRESSED | RIGHT_CTRL_PRESSED)) != 0;

	switch (key.wVirtualKeyCode)
	{
	case VK_ESCAPE:
		CloseToDirectory(processInfo->hPanel, state->OriginalDirectory);
		return TRUE;

	case VK_BACK:
		if (!state->FilterText.empty())
		{
			state->FilterText.pop_back();
			if (state->FilterText.empty())
				CloseToDirectory(processInfo->hPanel, state->OriginalDirectory);
			else
				RefreshPanel(*state, processInfo->hPanel, L"");
		}
		else
			CloseToDirectory(processInfo->hPanel, state->OriginalDirectory);
		return TRUE;

	case VK_DELETE:
		if (!state->FilterText.empty())
			CloseToSelectedItem(processInfo->hPanel, state->OriginalDirectory, selectedName);
		else
			CloseToDirectory(processInfo->hPanel, state->OriginalDirectory);
		return TRUE;

	default:
		if (ctrlPressed && (key.wVirtualKeyCode == 'R'))
		{
			EnumerateDirectory(*state, state->SourceDirectory);
			RefreshPanel(*state, processInfo->hPanel, selectedName);
			return TRUE;
		}

		wchar_t inputChar = 0;
		if (ExtractInputChar(key, inputChar))
		{
			const wchar_t normalizedChar = NormalizeFilterChar(inputChar);
			const std::wstring candidateFilter = state->FilterText + normalizedChar;
			if (HasAnyMatchesForFilter(*state, candidateFilter))
			{
				state->FilterText = candidateFilter;
				RefreshPanel(*state, processInfo->hPanel, L"");
			}
			return TRUE;
		}
		break;
	}

	return FALSE;
}

extern "C" intptr_t WINAPI ProcessHostFileW(const ProcessHostFileInfo* processHostFileInfo)
{
	PanelState* state = static_cast<PanelState*>(processHostFileInfo->hPanel);
	if (!state || !processHostFileInfo->PanelItem || processHostFileInfo->ItemsNumber == 0)
		return FALSE;

	const SourceEntry* entry = FindEntryByName(*state, processHostFileInfo->PanelItem[0].FileName);
	if (!entry)
		return FALSE;

	if (entry->IsDirectory)
		CloseToDirectory(processHostFileInfo->hPanel, entry->FullPath);
	else
		CloseToFileAndOpen(processHostFileInfo->hPanel, state->SourceDirectory, entry->Name);

	return TRUE;
}

extern "C" intptr_t WINAPI SetDirectoryW(const SetDirectoryInfo* setDirectoryInfo)
{
	PanelState* state = static_cast<PanelState*>(setDirectoryInfo->hPanel);
	if (!state || !setDirectoryInfo->Dir || setDirectoryInfo->Dir[0] == 0)
		return FALSE;

	std::wstring target = setDirectoryInfo->Dir;
	if (target == L"..")
	{
		CloseToDirectory(setDirectoryInfo->hPanel, state->OriginalDirectory);
		return TRUE;
	}
	else if (target.find(L':') == std::wstring::npos && !(target.size() > 1 && target[0] == L'\\' && target[1] == L'\\'))
		target = JoinPath(state->SourceDirectory, target);

	CloseToDirectory(setDirectoryInfo->hPanel, target);
	return TRUE;
}

extern "C" void WINAPI ClosePanelW(const ClosePanelInfo* closeInfo)
{
	delete static_cast<PanelState*>(closeInfo->hPanel);
}

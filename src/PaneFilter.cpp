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

	struct PanelState
	{
		std::wstring SourceDirectory;
		std::wstring CurrentName;
		std::wstring FilterText;
		std::wstring PanelTitle;
		std::vector<SourceEntry> Entries;
		std::vector<size_t> FilteredIndices;
		bool ShowHidden;
		bool SkipRescan;
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

	bool IsAbsolutePath(const std::wstring& path)
	{
		return (path.size() > 1 && path[1] == L':') || (path.size() > 1 && IsPathSeparator(path[0]) && IsPathSeparator(path[1]));
	}

	std::wstring GetRootPath(const std::wstring& path)
	{
		if (path.size() > 1 && path[1] == L':')
			return path.substr(0, 2) + L'\\';

		if (path.size() > 2 && IsPathSeparator(path[0]) && IsPathSeparator(path[1]))
		{
			const size_t serverEnd = path.find_first_of(L"\\/", 2);
			if (serverEnd == std::wstring::npos)
				return path + L'\\';

			const size_t shareEnd = path.find_first_of(L"\\/", serverEnd + 1);
			return shareEnd == std::wstring::npos ? path + L'\\' : path.substr(0, shareEnd + 1);
		}

		return path;
	}

	std::wstring MakeLongPath(const std::wstring& path)
	{
		if (path.size() < MAX_PATH - 12 || path.compare(0, 4, L"\\\\?\\") == 0)
			return path;

		if (path.compare(0, 2, L"\\\\") == 0)
			return L"\\\\?\\UNC\\" + path.substr(2);

		return L"\\\\?\\" + path;
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

	bool ContainsWildcardChars(const std::wstring& filter)
	{
		return filter.find(L'*') != std::wstring::npos || filter.find(L'?') != std::wstring::npos;
	}

	bool StripPrefix(std::wstring& text, const wchar_t* prefix)
	{
		const size_t length = wcslen(prefix);
		if (text.size() <= length || text.compare(0, length, prefix) != 0)
			return false;

		text.erase(0, length);
		return true;
	}

	// Parses FAR key names such as "AltA", "RAltShift." or "CtrlAlt*" into a one-character filter.
	bool TryExtractFilterFromKeyDescriptor(const std::wstring& rawText, std::wstring& filter)
	{
		static const wchar_t* const Modifiers[] = {L"lalt", L"ralt", L"alt", L"lctrl", L"rctrl", L"ctrl", L"shift"};

		std::wstring text = NormalizeFilterText(rawText);
		bool hasModifier = false;
		for (bool stripped = true; stripped;)
		{
			stripped = false;
			for (const wchar_t* modifier : Modifiers)
			{
				if (StripPrefix(text, modifier))
				{
					stripped = hasModifier = true;
					break;
				}
			}
		}

		if (!hasModifier || text.empty())
			return false;

		if (text.size() != 1 && !ContainsWildcardChars(text))
			return false;

		filter = text;
		return true;
	}

	std::wstring ParseStartupText(const std::wstring& rawText)
	{
		std::wstring filter;
		if (TryExtractFilterFromKeyDescriptor(rawText, filter))
			return filter;

		return NormalizeFilterText(rawText);
	}

	std::wstring CodePointToString(unsigned long long codePoint)
	{
		if (codePoint <= 0xFFFF)
			return std::wstring(1, static_cast<wchar_t>(codePoint));

		codePoint -= 0x10000;
		const wchar_t surrogates[] =
		{
			static_cast<wchar_t>(0xD800 + (codePoint >> 10)),
			static_cast<wchar_t>(0xDC00 + (codePoint & 0x3FF)),
		};
		return std::wstring(surrogates, ARRAYSIZE(surrogates));
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


	bool GetPanelItemName(HANDLE panel, size_t index, std::wstring& fileName)
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

		fileName = panelItemInfo.Item->FileName;
		return true;
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
				filter = ParseStartupText(value.String);
				return !filter.empty();
			}
			return false;

		case FMVT_INTEGER:
			if (value.Integer >= 1 && value.Integer <= 0x10FFFF)
			{
				filter = NormalizeFilterText(CodePointToString(static_cast<unsigned long long>(value.Integer)));
				return !filter.empty();
			}
			return false;

		case FMVT_DOUBLE:
			if (value.Double >= 1.0 && value.Double <= 0x10FFFF)
			{
				filter = NormalizeFilterText(CodePointToString(static_cast<unsigned long long>(value.Double)));
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
				return ParseStartupText(commandInfo->CommandLine);
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
			static_cast<unsigned>(state.FilteredIndices.size()),
			static_cast<unsigned>(state.Entries.size()));
		state.PanelTitle = buffer;
	}

	HANDLE FindFirst(const std::wstring& searchPattern, WIN32_FIND_DATAW& findData)
	{
		// FindExInfoBasic and FIND_FIRST_EX_LARGE_FETCH need Windows 7; older systems reject them.
		HANDLE findHandle = FindFirstFileExW(
			searchPattern.c_str(),
			FindExInfoBasic,
			&findData,
			FindExSearchNameMatch,
			nullptr,
			FIND_FIRST_EX_LARGE_FETCH);

		if (findHandle == INVALID_HANDLE_VALUE && GetLastError() == ERROR_INVALID_PARAMETER)
			findHandle = FindFirstFileW(searchPattern.c_str(), &findData);

		return findHandle;
	}

	bool EnumerateDirectory(PanelState& state, const std::wstring& directory)
	{
		state.Entries.clear();
		state.FilteredIndices.clear();
		state.SourceDirectory = directory;

		WIN32_FIND_DATAW findData{};
		const std::wstring searchPattern = MakeLongPath(JoinPath(directory, L"*"));
		HANDLE findHandle = FindFirst(searchPattern, findData);
		if (findHandle == INVALID_HANDLE_VALUE)
		{
			const DWORD error = GetLastError();
			UpdatePanelTitle(state);
			return error == ERROR_FILE_NOT_FOUND || error == ERROR_NO_MORE_FILES;
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
		if (!GetActivePanelInfo(panelInfo) || panelInfo.PanelType != PTYPE_FILEPANEL || (panelInfo.Flags & PFLAGS_PLUGIN) != 0)
		{
			ShowMessage({GetMsg(MPluginTitle), GetMsg(MWrongPanelType), GetMsg(MInstructions)});
			return false;
		}

		if (!GetPanelDirectory(PANEL_ACTIVE, state.SourceDirectory))
		{
			ShowMessage({GetMsg(MPluginTitle), GetMsg(MWrongPanelType), GetMsg(MInstructions)});
			return false;
		}

		state.ShowHidden = (panelInfo.Flags & PFLAGS_SHOWHIDDEN) != 0;
		state.SortMode = panelInfo.SortMode;
		state.SortOrder = (panelInfo.Flags & PFLAGS_REVERSESORTORDER) != 0 ? 1 : 0;

		GetPanelItemName(PANEL_ACTIVE, panelInfo.CurrentItem, state.CurrentName);

		if (!EnumerateDirectory(state, state.SourceDirectory))
		{
			ShowMessage({GetMsg(MPluginTitle), GetMsg(MCannotRead), state.SourceDirectory.c_str()});
			return false;
		}

		return true;
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

	std::wstring GetCurrentVisibleName(HANDLE panel)
	{
		PanelInfo panelInfo{};
		panelInfo.StructSize = sizeof(panelInfo);
		if (!Info.PanelControl(panel, FCTL_GETPANELINFO, 0, &panelInfo) || panelInfo.ItemsNumber == 0)
			return L"";

		std::wstring fileName;
		if (!GetPanelItemName(panel, panelInfo.CurrentItem, fileName))
			return L"";

		return fileName;
	}

	void RefreshPanel(PanelState& state, HANDLE panel, const std::wstring& preferredName)
	{
		RebuildFiltered(state);
		UpdatePanelTitle(state);

		// The entries are already up to date, so the GetFindDataW call triggered here must not rescan the disk.
		state.SkipRescan = true;
		Info.PanelControl(panel, FCTL_UPDATEPANEL, 1, nullptr);

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

	void CloseToDirectory(HANDLE panel, const std::wstring& directory)
	{
		Info.PanelControl(panel, FCTL_CLOSEPANEL, 0, const_cast<wchar_t*>(directory.c_str()));
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

	// FAR closes the plugin panel only after the current plugin call returns, so positioning
	// the cursor in the real panel has to happen in a posted macro that runs afterwards.
	void CloseAndPositionOn(HANDLE panel, const std::wstring& directory, const std::wstring& itemName, bool open)
	{
		if (itemName.empty())
		{
			CloseToDirectory(panel, directory);
			return;
		}

		std::wstring macroText =
			L"if Panel.SetPath(0, \"" + EscapeLuaString(directory) + L"\", \"" + EscapeLuaString(itemName) + L"\") then";
		macroText += open ? L" Keys(\"Enter\") end" : L" end";
		CloseAndRunLuaMacro(panel, directory, macroText);
	}

	const SourceEntry* FindEntryByName(const PanelState& state, const std::wstring& fileName)
	{
		if (fileName.empty())
			return nullptr;

		for (const SourceEntry& entry : state.Entries)
		{
			if (entry.Name == fileName)
				return &entry;
		}

		return nullptr;
	}

	HKL GetConsoleKeyboardLayout()
	{
		// The console window owns the input layout; FAR's own thread layout does not follow layout switches.
		if (const HWND consoleWindow = GetConsoleWindow())
		{
			if (const DWORD threadId = GetWindowThreadProcessId(consoleWindow, nullptr))
				return GetKeyboardLayout(threadId);
		}

		return GetKeyboardLayout(0);
	}

	bool TranslateKey(const KEY_EVENT_RECORD& key, const BYTE* keyboardState, wchar_t& ch)
	{
		// Bit 2: do not change the keyboard state (keeps pending dead keys intact, Windows 10 1607+).
		const UINT ToUnicodeNoStateChange = 0x4;

		wchar_t translated[8]{};
		const int translatedCount = ToUnicodeEx(
			key.wVirtualKeyCode,
			key.wVirtualScanCode,
			keyboardState,
			translated,
			ARRAYSIZE(translated),
			ToUnicodeNoStateChange,
			GetConsoleKeyboardLayout());

		if (translatedCount > 0 && translated[0] >= VK_SPACE && translated[0] != 0x7F)
		{
			ch = translated[0];
			return true;
		}

		return false;
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

		// Alt+- (or AltGr+- on layouts such as Hungarian) types '*'.
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

			if (TranslateKey(key, keyboardState, ch))
				return true;

			if (altPressed)
			{
				keyboardState[VK_MENU] = 0x80;
				if (ctrlPressed)
					keyboardState[VK_CONTROL] = 0x80;

				if (TranslateKey(key, keyboardState, ch))
					return true;
			}

			return false;
		}

		if (ctrlPressed && !altGrPressed)
			return false;

		if (key.uChar.UnicodeChar < VK_SPACE || key.uChar.UnicodeChar == 0x7F)
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

	if (state->FilteredIndices.empty())
	{
		if (state->FilterText.empty())
			ShowMessage({GetMsg(MPluginTitle), GetMsg(MNoEntries)});
		else
			MessageBeep(MB_OK);

		delete state;
		return nullptr;
	}

	// EnumerateDirectory has just filled the entries; the first GetFindDataW call can use them as is.
	state->SkipRescan = true;
	return state;
}

extern "C" intptr_t WINAPI GetFindDataW(GetFindDataInfo* findDataInfo)
{
	PanelState* state = static_cast<PanelState*>(findDataInfo->hPanel);
	if (!state)
		return FALSE;

	// FAR asks for fresh data after its own file operations (F5, F6, F8, ...), so rescan unless
	// the request came from a plain filter change.
	if (state->SkipRescan)
		state->SkipRescan = false;
	else if ((findDataInfo->OpMode & OPM_FIND) == 0)
		EnumerateDirectory(*state, state->SourceDirectory);

	findDataInfo->PanelItem = nullptr;
	findDataInfo->ItemsNumber = 0;
	if (state->FilteredIndices.empty())
		return TRUE;

	PluginPanelItem* items = static_cast<PluginPanelItem*>(calloc(state->FilteredIndices.size(), sizeof(PluginPanelItem)));
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
	findDataInfo->ItemsNumber = state->FilteredIndices.size();
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
	if (!state)
		return;

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

	HANDLE panel = processInfo->hPanel;
	const KEY_EVENT_RECORD& key = processInfo->Rec.Event.KeyEvent;
	const bool ctrlPressed = (key.dwControlKeyState & (LEFT_CTRL_PRESSED | RIGHT_CTRL_PRESSED)) != 0;
	const bool altPressed = (key.dwControlKeyState & (LEFT_ALT_PRESSED | RIGHT_ALT_PRESSED)) != 0;
	const bool shiftPressed = (key.dwControlKeyState & SHIFT_PRESSED) != 0;
	const bool noModifiers = !ctrlPressed && !altPressed && !shiftPressed;

	if (noModifiers)
	{
		switch (key.wVirtualKeyCode)
		{
		case VK_ESCAPE:
			CloseAndPositionOn(panel, state->SourceDirectory, state->CurrentName, false);
			return TRUE;

		case VK_BACK:
			if (!state->FilterText.empty())
				state->FilterText.pop_back();

			if (state->FilterText.empty())
				CloseAndPositionOn(panel, state->SourceDirectory, state->CurrentName, false);
			else
				RefreshPanel(*state, panel, GetCurrentVisibleName(panel));
			return TRUE;

		case VK_DELETE:
			CloseAndPositionOn(
				panel,
				state->SourceDirectory,
				state->FilterText.empty() ? state->CurrentName : GetCurrentVisibleName(panel),
				false);
			return TRUE;

		case VK_RETURN:
		{
			const SourceEntry* entry = FindEntryByName(*state, GetCurrentVisibleName(panel));
			if (!entry)
				return FALSE;

			if (entry->IsDirectory)
				CloseToDirectory(panel, entry->FullPath);
			else
				CloseAndPositionOn(panel, state->SourceDirectory, entry->Name, true);
			return TRUE;
		}

		default:
			break;
		}
	}

	if (ctrlPressed && !altPressed && !shiftPressed && key.wVirtualKeyCode == 'R')
	{
		const std::wstring selectedName = GetCurrentVisibleName(panel);
		EnumerateDirectory(*state, state->SourceDirectory);
		RefreshPanel(*state, panel, selectedName);
		return TRUE;
	}

	wchar_t inputChar = 0;
	if (ExtractInputChar(key, inputChar))
	{
		const std::wstring candidateFilter = state->FilterText + NormalizeFilterChar(inputChar);
		if (HasAnyMatchesForFilter(*state, candidateFilter))
		{
			state->FilterText = candidateFilter;
			RefreshPanel(*state, panel, GetCurrentVisibleName(panel));
		}
		return TRUE;
	}

	return FALSE;
}

extern "C" intptr_t WINAPI SetDirectoryW(const SetDirectoryInfo* setDirectoryInfo)
{
	PanelState* state = static_cast<PanelState*>(setDirectoryInfo->hPanel);
	if (!state || !setDirectoryInfo->Dir || setDirectoryInfo->Dir[0] == 0)
		return FALSE;

	// Find File and other silent traversals must not close the panel under FAR's feet.
	if ((setDirectoryInfo->OpMode & (OPM_FIND | OPM_SILENT)) != 0)
		return FALSE;

	std::wstring target = setDirectoryInfo->Dir;
	if (target == L"..")
	{
		CloseAndPositionOn(setDirectoryInfo->hPanel, state->SourceDirectory, state->CurrentName, false);
		return TRUE;
	}

	if (IsPathSeparator(target[0]) && !IsAbsolutePath(target))
		target = GetRootPath(state->SourceDirectory) + target.substr(1);
	else if (!IsAbsolutePath(target))
		target = JoinPath(state->SourceDirectory, target);

	CloseToDirectory(setDirectoryInfo->hPanel, target);
	return TRUE;
}

extern "C" void WINAPI ClosePanelW(const ClosePanelInfo* closeInfo)
{
	delete static_cast<PanelState*>(closeInfo->hPanel);
}

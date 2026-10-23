#include "Matching.hpp"

#include <windows.h>

#include <vector>

namespace match {

namespace {

// Letters whose accent is part of the letter itself, so Unicode decomposition leaves them alone.
wchar_t FoldStandaloneLetter(wchar_t ch)
{
	switch (ch)
	{
	case 0x0142: return L'l';  // ł
	case 0x0140: return L'l';  // ŀ
	case 0x00F8: return L'o';  // ø
	case 0x0111: return L'd';  // đ
	case 0x00F0: return L'd';  // ð
	case 0x0127: return L'h';  // ħ
	case 0x0167: return L't';  // ŧ
	case 0x0131: return L'i';  // ı
	default: return ch;
	}
}

bool WildcardMatch(const wchar_t* text, const wchar_t* pattern)
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

		if (*pattern == L'?' || *pattern == *text)
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

std::wstring BuildWildcardPattern(const std::wstring& filter)
{
	std::wstring pattern = filter;
	if (pattern.front() != L'*')
		pattern.insert(pattern.begin(), L'*');
	if (pattern.back() != L'*')
		pattern.push_back(L'*');
	return pattern;
}

}

std::wstring FoldForMatch(const std::wstring& text)
{
	if (text.empty())
		return text;

	std::wstring lower = text;
	CharLowerBuffW(&lower[0], static_cast<DWORD>(lower.size()));

	// MAP_COMPOSITE splits accented letters into the base letter plus combining marks.
	std::wstring decomposed = lower;
	const int size = FoldStringW(MAP_COMPOSITE, lower.c_str(), static_cast<int>(lower.size()), nullptr, 0);
	if (size > 0)
	{
		decomposed.assign(static_cast<size_t>(size), L'\0');
		if (FoldStringW(MAP_COMPOSITE, lower.c_str(), static_cast<int>(lower.size()), &decomposed[0], size) != size)
			decomposed = lower;
	}

	std::vector<WORD> types(decomposed.size());
	if (!GetStringTypeW(CT_CTYPE3, decomposed.c_str(), static_cast<int>(decomposed.size()), types.data()))
		types.assign(decomposed.size(), 0);

	std::wstring folded;
	folded.reserve(decomposed.size());
	for (size_t index = 0; index < decomposed.size(); ++index)
	{
		if ((types[index] & C3_NONSPACING) != 0)
			continue;

		folded.push_back(FoldStandaloneLetter(decomposed[index]));
	}

	return folded;
}

bool ContainsWildcardChars(const std::wstring& filter)
{
	return filter.find(L'*') != std::wstring::npos || filter.find(L'?') != std::wstring::npos;
}

bool MatchesFolded(const std::wstring& foldedName, const std::wstring& foldedFilter)
{
	if (foldedFilter.empty())
		return true;

	if (ContainsWildcardChars(foldedFilter))
		return WildcardMatch(foldedName.c_str(), BuildWildcardPattern(foldedFilter).c_str());

	return foldedName.find(foldedFilter) != std::wstring::npos;
}

}

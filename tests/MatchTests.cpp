// Unit tests for src/Matching.cpp. Built with -DPANEFILTER_BUILD_TESTS=ON and run in CI.

#include <cstdio>
#include <string>

#include "Matching.hpp"

namespace {

int failures = 0;

void Expect(bool condition, const char* what)
{
	if (!condition)
	{
		std::printf("FAILED: %s\n", what);
		++failures;
	}
}

void ExpectFold(const std::wstring& text, const std::wstring& expected, const char* what)
{
	const std::wstring folded = match::FoldForMatch(text);
	if (folded != expected)
	{
		std::printf("FAILED: %s, got:", what);
		for (const wchar_t ch : folded)
			std::printf(" %04X", static_cast<unsigned>(ch));
		std::printf("\n");
		++failures;
	}
}

bool Matches(const std::wstring& name, const std::wstring& filter)
{
	return match::MatchesFolded(match::FoldForMatch(name), match::FoldForMatch(filter));
}

}

int main()
{
	// Folding.
	ExpectFold(L"\u00C1rv\u00EDzt\u0171r\u0151", L"arvizturo", "Arvizturo folds");
	ExpectFold(L"\u0141\u00F3d\u017A", L"lodz", "Lodz folds");
	ExpectFold(L"Cr\u00E8me Br\u00FBl\u00E9e", L"creme brulee", "Creme brulee folds");
	ExpectFold(L"\u00F8re", L"ore", "o with stroke folds");
	ExpectFold(L"Plain.TXT", L"plain.txt", "ASCII is lower-cased");
	ExpectFold(L"", L"", "empty string");

	// Unaccented filters find accented names.
	Expect(Matches(L"\u00E9let.txt", L"ele"), "e matches e-acute");
	Expect(Matches(L"k\u0151r.doc", L"kor"), "o matches o-double-acute");
	Expect(Matches(L"\u00C1RV\u00CDZ", L"arviz"), "case and accents ignored");
	Expect(Matches(L"\u0141\u00F3d\u017A.jpg", L"lodz"), "l matches l-stroke");

	// Accented filters still match.
	Expect(Matches(L"\u00E9let.txt", L"\u00E9l"), "e-acute matches e-acute");
	Expect(Matches(L"elet.txt", L"\u00E9l"), "e-acute matches e");

	// Wildcards.
	Expect(Matches(L"\u00C1rv\u00EDzt\u0171r\u0151.txt", L"*.txt"), "*.txt");
	Expect(Matches(L"\u00C1rv\u00EDzt\u0171r\u0151.txt", L"a?v"), "? matches one letter");
	Expect(!Matches(L"report.doc", L"*.txt"), "*.txt does not match .doc");
	Expect(Matches(L"report.doc", L"r*t.d"), "pattern in the middle");

	// Plain substrings.
	Expect(Matches(L"report.doc", L"port"), "substring");
	Expect(!Matches(L"report.doc", L"x"), "missing letter");
	Expect(Matches(L"anything", L""), "empty filter matches everything");

	if (failures == 0)
		std::printf("All matching tests passed.\n");
	return failures == 0 ? 0 : 1;
}

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

bool Matches(const std::wstring& name, const std::wstring& filter)
{
	return match::MatchesFolded(match::FoldForMatch(name), match::FoldForMatch(filter));
}

}

int main()
{
	// Folding.
	Expect(match::FoldForMatch(L"Árvíztűrő") == L"arvizturo", "Arvizturo folds");
	Expect(match::FoldForMatch(L"Łódź") == L"lodz", "Lodz folds");
	Expect(match::FoldForMatch(L"Crème Brûlée") == L"creme brulee", "Creme brulee folds");
	Expect(match::FoldForMatch(L"øre") == L"ore", "o with stroke folds");
	Expect(match::FoldForMatch(L"Plain.TXT") == L"plain.txt", "ASCII is lower-cased");
	Expect(match::FoldForMatch(L"") == L"", "empty string");

	// Unaccented filters find accented names.
	Expect(Matches(L"élet.txt", L"ele"), "e matches é");
	Expect(Matches(L"kőr.doc", L"kor"), "o matches ő");
	Expect(Matches(L"ÁRVÍZ", L"arviz"), "case and accents ignored");
	Expect(Matches(L"Łódź.jpg", L"lodz"), "l matches ł");

	// Accented filters still match.
	Expect(Matches(L"élet.txt", L"él"), "é matches é");
	Expect(Matches(L"elet.txt", L"él"), "é matches e");

	// Wildcards.
	Expect(Matches(L"Árvíztűrő.txt", L"*.txt"), "*.txt");
	Expect(Matches(L"Árvíztűrő.txt", L"a?v"), "? matches one letter");
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

#pragma once

#include <string>

// Filter matching shared by the plugin and the unit tests. Names and filters
// are compared after FoldForMatch, so the match ignores case and accents.

namespace match {

// Lower case, with accents removed: "Árvíztűrő" -> "arvizturo", "Łódź" -> "lodz".
std::wstring FoldForMatch(const std::wstring& text);

bool ContainsWildcardChars(const std::wstring& filter);

// Both arguments must already be folded. A filter with * or ? is a wildcard
// pattern matched anywhere in the name; any other filter is a substring.
bool MatchesFolded(const std::wstring& foldedName, const std::wstring& foldedFilter);

}  // namespace match

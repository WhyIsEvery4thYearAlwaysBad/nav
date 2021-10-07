#include <regex>
#include "utils.hpp"

std::regex IDrx("#(\\d+)");
std::regex NumberRx("\\d+");
// Tries to get an index/ID from string.
// Returns pair ([1] - Is ID) if successful, none otherwise.
std::optional<IntIndex> StrToIndex(const std::string& str) {
	std::smatch sm;
	// It is an ID.
	if (std::regex_match(str, sm, IDrx)) return std::make_pair(true, std::stoul(sm[1].str()));
	// It is an index.
	else if (std::regex_match(str, sm, NumberRx)) return std::make_pair(false, std::stoul(str));

	return {};
}
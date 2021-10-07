#ifndef UTILS_HPP
#define UTILS_HPP
#include <utility>
#include <regex>
// Utility regxes.
extern std::regex IDrx;
extern std::regex NumberRx;
// Index type for storing indexes / Ids.
typedef std::pair<bool, unsigned int> IntIndex;
typedef std::pair<bool, unsigned short> ShortIndex;
typedef std::pair<bool, unsigned char> ByteIndex;

// Tries to get an index/ID from string.
// Returns pair if successful, none otherwise.
std::optional<IntIndex> StrToIndex(const std::string& str);
#endif
#ifndef TEST_AUTOMATION_HPP
#define TEST_AUTOMATION_HPP
#include <utility>
#include <optional>
#include <string>
// Tests the I/O of connection data.
// True on success, false on failure.
std::pair<bool, std::string > TestNavConnectionDataIO();

// Tests the I/O of encounter paths.
// True on success, false on failure.
std::pair<bool, std::string> TestEncounterPathIO();

// Tests the I/O of encounter spots.
// True on success, false on failure.
std::pair<bool, std::string > TestEncounterSpotIO();

// Tests the I/O of area data.
// True on success, false on failure.
std::pair<bool, std::string > TestNavAreaDataIO();

// Tests the I/O of NAV files.
// True on success, false on failure.
std::pair<bool, std::string > TestNAVFileIO();
#endif
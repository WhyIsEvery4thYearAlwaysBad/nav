// Test automation.
#include <iostream>
#include <sstream>
#include <utility>
#include <span>
#include "nav_connections.hpp"
#include "nav_area.hpp"
#include "nav_file.hpp"
#include "test_automation.hpp"

// Tests the reading and writing of connection data. The data size *should always* be 5 bytes, and the connections should give the same data
// True on success, false on failure.
std::pair<bool, std::string > TestNavConnectionDataIO() {
	// Reference connection.
	NavConnection connection_init;
	connection_init.TargetAreaID = 5;
	// Sample connection.
	NavConnection sample;
	std::stringstream TestFile;
	if (!connection_init.WriteData(*TestFile.rdbuf())) return {false, "Connection Data I/O: Write Failed!"};
	if (!sample.ReadData(*TestFile.rdbuf())) return {false, "Connection Data I/O: Read Failed!"};

	if (connection_init.TargetAreaID != sample.TargetAreaID) return {false, "Connection Data I/O: Failed! (Reason: Mismatched Read/Written data!)"};
	else if (TestFile.str().size() != CONNECTION_SIZE) return {false, "Connection Data I/O Failed! (Reason: Mismatched written data size of "+std::to_string(TestFile.str().size())+" bytes!)" };
	return {true, "Connection Data I/O: Passed!" };
}

// Tests the I/O of area data.
// True on success, false on failure.
std::pair<bool, std::string> TestNavAreaDataIO() {
	// Reference area data.
	NavArea area_init;
	area_init.ID = 5u;
	area_init.Flags = (unsigned int)0b00001010;
	area_init.nwCorner = {5.0f, 13.45f, 90.4f};
	area_init.seCorner = {355.0f, 513.45f, 90.4f};
	area_init.SouthWestZ = 355.0f;
	area_init.NorthEastZ = 90.4f;
	area_init.LightIntensity.emplace(std::array<float, 4>({0.0f, 0.5f, 0.2f, 1.0f}));
	area_init.InheritVisibilityFromAreaID = 5u;
	area_init.EarliestOccupationTimes = {6.0f, 31.0f};
	area_init.visAreaCount = 0u;
	area_init.PlaceID = 3u;
	// Test for every major version
	for (unsigned int version = 0; version <= 16u; version++)
	{
		std::stringstream TestFile;
		if (!area_init.WriteData(*TestFile.rdbuf(), version, {})) return {false, "Area Data I/O: Write Failed! (MajorVersion="+std::to_string(version)+")"};
		if (TestFile.eof()) std::cerr << "Area buffer messed up!\n";
		TestFile.seekg(0);
		TestFile.seekp(0);
		
		// Sample area.
		NavArea sample;
		if (!sample.ReadData(*TestFile.rdbuf(), version, {})) 
			return {false, "Area Data I/O: Read Failed! (MajorVersion="+std::to_string(version)+")"};;

		// Compare data.
		if (!area_init.hasSameNAVData(sample, version, {})) return {false, "Area Data I/O: Failed! Mismatched data!"};
	}
	return {true, "Area Data I/O: Passed!" };
}

// Tests the I/O of encounter paths.
// True on success, false on failure.
std::pair<bool, std::string > TestEncounterPathIO() {
	NavEncounterPath init;
	init.FromAreaID = 5u;
	init.FromDirection = Direction::South;
	init.ToAreaID = 6u;
	init.ToDirection = Direction::East;

	NavEncounterPath sample;
	std::stringstream TestFile;
	// Test
	if (!init.WriteData(*TestFile.rdbuf())) return {false, "Encounter path I/O: Write Failed!"};
	if (!sample.ReadData(*TestFile.rdbuf())) return {false, "Encounter path I/O: Read Failed!"};
	if (!sample.hasSameNAVData(init).value_or(false)) return {false, "Encounter path I/O: Failed! Mismatched data.\n"};
	return {true, "Encounter Path I/O: Passed!"};
}

// Tests the I/O of encounter spots.
// True on success, false on failure.
std::pair<bool, std::string > TestEncounterSpotIO() {
	NavEncounterSpot init;
	init.OrderID = 3u;
	init.ParametricDistance = 4.0f;

	NavEncounterSpot sample;
	std::stringstream TestFile;
	// Test
	if (!init.WriteData(*TestFile.rdbuf())) return {false, "Encounter Spot I/O: Write Failed!"};
	if (!sample.ReadData(*TestFile.rdbuf())) return {false, "Encounter Spot I/O: Read Failed!"};
	
	return {true, "Encounter Spot I/O: Passed!"};
}

// Tests the I/O of NAV files.
// True on success, false on failure.
std::pair<bool, std::string > TestNAVFileIO() {
	NavFile init;
	init.GetMagicNumber() = 0xFEEDFACE;
	init.GetAreaCount() = 5u;
	init.GetLadderCount();
	init.IsAnalyzed() = false;
	init.areas = std::vector<NavArea>(init.GetAreaCount());

	for (size_t i = 0; i < LATEST_NAV_MAJOR_VERSION; i++)
	{
		std::stringstream TestFile;
		init.GetMajorVersion() = i;
		if (!init.WriteData(*TestFile.rdbuf())) {
			return {false, "NAV (version="+std::to_string(i)+") File I/O: Write Failed!"};
		}
		// Sample.
		NavFile sample;
		sample.GetMajorVersion() = i;
		TestFile.seekg(0u);
		// Try reading from reference.
		if (!sample.ReadData(*TestFile.rdbuf())) {
			return {false, "NAV (version="+std::to_string(i)+") File I/O: Read Failed!"};
		}
		// Inequal NavArea containers. Something went wrong.
		if (!std::equal(init.areas.value().begin(), init.areas.value().end(), sample.areas.value().begin(), sample.areas.value().end(), 
		[&sample](NavArea& lhs, NavArea& rhs) {
			return lhs.hasSameNAVData(rhs, sample.GetMajorVersion(), sample.GetMinorVersion());
		})) {
			return {false, "NAV (version="+std::to_string(i)+") File I/O: Failed! Mismatching area data!"};
		}
	}
	return {true, "NAV File I/O: Passed!"};
}
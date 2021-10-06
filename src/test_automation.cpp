// Test automation.
#include <iostream>
#include <sstream>
#include <utility>
#include "nav_connections.hpp"
#include "nav_area.hpp"
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

		//if (!area_init.hasSameFileData(sample)) return {false, "Area Data I/O: Failed!\nReason: Mismatched Read/Written data!"};
	}
	return {true, "Area Data I/O: Succeeded!" };
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
	
	return {true, "Encounter Path I/O: Passed!"};
}
#include <fstream>
#include <string>
#include <iostream>
#include <exception>
#include <bit>
#include <unistd.h>
#include "nav_file.hpp"
#include "nav_area.hpp"

NavFile::NavFile() {}

NavFile::NavFile(const std::string& path)
: FilePath(path) {
	
	std::ifstream navFile(FilePath, std::ios_base::in | std::ios_base::binary);
	if (navFile.is_open() && navFile.good()) {
		navFile.close();
		FillMetaDataFromFile();
	}
	else {
		std::cerr << "fatal: Filestream (" << FilePath << ") is screwed up.\n";
		return;
	}
}

NavFile::~NavFile() {}

const std::filesystem::path& NavFile::GetFilePath() {
	return FilePath;
}

unsigned int NavFile::GetMagicNumber() {
	return MagicNumber;
}

unsigned int NavFile::GetMajorVersion() {
	return MajorVersion;
}

std::optional<unsigned int>& NavFile::GetMinorVersion() {
	return MinorVersion;
}

std::optional<unsigned int>& NavFile::GetBSPSize() {
	return BSPSize;
}

std::optional<bool> NavFile::IsAnalyzed() {
	return isAnalyzed;
}

unsigned short NavFile::GetPlaceCount() {
	return PlaceCount;
}

std::optional<bool> NavFile::GetHasUnnamedAreas() {
	return hasUnnamedAreas;
}

unsigned int NavFile::GetAreaCount() {
	return AreaCount;
}

unsigned int NavFile::GetLadderCount() {
	return LadderCount;
}

const std::streampos& NavFile::GetAreaDataLoc() {
	return AreaDataLoc;
}

const std::streampos& NavFile::GetLadderDataLoc() {
	return LadderDataLoc;
}

// Validate the NAV File.
bool NavFile::IsValidFile() {
	if (MagicNumber != 0xFEEDFACE) {
		std::cerr << "Mismatching magic number ("<<std::to_string(MagicNumber)<<"); not a Source Engine NAV file.\n";
		return false;
	}
	return true;
}

// TODO: Error checking.
void NavFile::FillMetaDataFromFile() {
	std::ifstream navFile;
	navFile.open(FilePath, std::ios_base::in | std::ios_base::binary);
	if (!navFile.good()) return;
	// Get MagicNumber.
	if (!navFile.read(reinterpret_cast<char*>(&MagicNumber), VALVE_INT_SIZE)) return;
	if (!IsValidFile()) exit(EXIT_FAILURE);
	// Get major version
	if (!navFile.read(reinterpret_cast<char*>(&MajorVersion), VALVE_INT_SIZE)) return;
	// Get minor version.
	if (MajorVersion >= 10) {
		unsigned int placeholder;
		if (!navFile.read(reinterpret_cast<char*>(&placeholder), VALVE_INT_SIZE)) return;
		MinorVersion = placeholder;
	}
	// Get BSP Size
	if (MajorVersion >= 4) {
		unsigned int placeholder;
		if (!navFile.read(reinterpret_cast<char*>(&placeholder), VALVE_INT_SIZE)) return;
		BSPSize = placeholder;
	}
	// Is mesh file analyzed.
	if (MajorVersion >= 14) {
		bool placeholder;
		if (!navFile.read(reinterpret_cast<char*>(&placeholder), VALVE_CHAR_SIZE)) return;
		isAnalyzed = placeholder;
	}
	// Get Places
	if (!navFile.read(reinterpret_cast<char*>(&PlaceCount), VALVE_SHORT_SIZE)) return;
	for (size_t i = 0; i < PlaceCount; i++)
	{
		unsigned short nameLength;
		if (!navFile.read(reinterpret_cast<char*>(&nameLength), VALVE_SHORT_SIZE)) return;
		if (!navFile.seekg(nameLength, std::ios_base::cur)) return;
	}
	// Has unnamed areas?
	if (MajorVersion >= 11) if (!(hasUnnamedAreas = (bool)navFile.get())) return;
	// Traverse area data
	if (!navFile.read(reinterpret_cast<char*>(&AreaCount), VALVE_INT_SIZE)) return;
	AreaDataLoc = navFile.tellg(); // Mark the location of area data.
	for (size_t i = 0; i < AreaCount; i++)
	{
		std::optional<size_t> Length = TraverseNavAreaData(navFile.tellg());
		if (Length.has_value()) navFile.seekg(Length.value(), std::ios_base::cur);
		else {
			#ifdef NDEBUG
			std::cerr << "NavFill:FillMetaDataFromFile(): FATAL: Could not get area data length!\n";
			#endif
			exit(EXIT_FAILURE);
		}
	}
	// Ladders
	LadderDataLoc = navFile.tellg();
	if (!navFile.read(reinterpret_cast<char*>(&LadderCount), VALVE_INT_SIZE)) return;
	if (!navFile.seekg(LADDER_SIZE * LadderCount, std::ios_base::cur)) return;
}

// Travel through data of an area.
// Returns data length of an area (at specified file position).
std::optional<size_t> NavFile::TraverseNavAreaData(const std::streampos& pos) {
	std::filebuf inFileBuf;
	inFileBuf.open(FilePath, std::ios_base::in | std::ios_base::binary);
	if (!inFileBuf.is_open()) {
		#ifdef NDEBUG
			std::cerr << "NavFile::TraverseNavAreaData(): File buffer \'" << FilePath << "\' could not open.\n";
		#endif
		return {};
	}
	std::streampos startPos = inFileBuf.pubseekpos(pos);
	if (inFileBuf.pubseekoff(VALVE_INT_SIZE, std::ios_base::cur) == -1) return {}; // ID
	{
		std::optional<unsigned char> size = getAreaAttributeFlagSize(MajorVersion, MinorVersion);
		if (!size.has_value())
		{
			#ifdef NDEBUG
			std::cerr << "NavFile::TraverseNavAreaData(): Could not get attribute flag size!\n";
			#endif
			return {};
		}
		else if(!inFileBuf.pubseekoff(size.value(), std::ios_base::cur)) return {};
	}
	// Skip over nwCorner[3], seCorner[3], NorthEastZ, and SouthWestZ.
	if (!inFileBuf.pubseekoff(VALVE_FLOAT_SIZE * 8, std::ios_base::cur)) return {};
	// Traverse connection data.
	for (unsigned char currDirection = (char)Direction::North; currDirection < (char)Direction::Count; currDirection++)
	{
		unsigned int connectionCount = 0u;
		if (inFileBuf.sgetn(reinterpret_cast<char*>(&connectionCount), VALVE_INT_SIZE) != VALVE_INT_SIZE) return {};
		if (!inFileBuf.pubseekoff(connectionCount * VALVE_INT_SIZE, std::ios_base::cur)) return {};
	}
	// Hiding spots
	unsigned char hideSpotCount = inFileBuf.sbumpc();
	if (!inFileBuf.pubseekoff((VALVE_INT_SIZE + (VALVE_FLOAT_SIZE * 3) + VALVE_CHAR_SIZE) * hideSpotCount, std::ios_base::cur)) return {};

	// Approach spots which are not in newer NAV versions.
	if (MajorVersion < 15) {
		unsigned char approachAreaCount;
		inFileBuf.sgetn(reinterpret_cast<char*>(&approachAreaCount), sizeof(approachAreaCount));
		if (!inFileBuf.pubseekoff(APPROACH_SPOT_SIZE * approachAreaCount, std::ios_base::cur)) return {};
	}

	// Encounter Paths.
	unsigned int encounterPathCount;
	inFileBuf.sgetn(reinterpret_cast<char*>(&encounterPathCount), VALVE_INT_SIZE);
	for (unsigned int pathIndex=0; pathIndex < encounterPathCount; pathIndex++) {
		inFileBuf.pubseekoff(ENCOUNTER_PATH_SIZE, std::ios_base::cur);
		unsigned char spotCount = inFileBuf.sbumpc();
		if (!inFileBuf.pubseekoff(ENCOUNTER_SPOT_SIZE * spotCount, std::ios_base::cur)) return {};
	}
	// Skip over place ID.
	if (!inFileBuf.pubseekoff(VALVE_SHORT_SIZE, std::ios_base::cur)) return {};
	// Handle ladders
	for (char i = 0; i < 2; i++)
	{
		unsigned int ladderConnectionCount;
		inFileBuf.sgetn(reinterpret_cast<char*>(&ladderConnectionCount), VALVE_INT_SIZE);
		if (!inFileBuf.pubseekoff(VALVE_INT_SIZE * ladderConnectionCount, std::ios_base::cur)) return {}; // Skip over ladder IDs.
	}
	// Occupy times
	if (!inFileBuf.pubseekoff(VALVE_FLOAT_SIZE * 2, std::ios_base::cur)) return {};

	// Light intensity
	if (MajorVersion >= 11 && !inFileBuf.pubseekoff(VALVE_FLOAT_SIZE * 4, std::ios_base::cur)) return {};

	// Visible areas
	if (MajorVersion >= 16) {
		unsigned int visibleAreaCount;
		inFileBuf.sgetn(reinterpret_cast<char*>(&visibleAreaCount), sizeof(visibleAreaCount));
		if (!inFileBuf.pubseekoff(VISIBLE_AREA_SIZE * visibleAreaCount, std::ios_base::cur)) return {};
	}

	// Skip over InheritVisibilityFromAreaID.
	if (!inFileBuf.pubseekoff(VALVE_INT_SIZE, std::ios_base::cur)) return {};

	// Skip over custom data.
	if (!inFileBuf.pubseekoff(getCustomDataSize(inFileBuf, GetAsEngineVersion(MajorVersion, MinorVersion)), std::ios_base::cur)) return {};

	return (inFileBuf.pubseekoff(0, std::ios_base::cur) - startPos);
}

// Find an area with ID.
// Return position to the area's data if found, nothing otherwise.
std::optional<std::streampos> NavFile::FindArea(const unsigned int& ID) {
	// Setup temporary file buffer.
	std::filebuf inFileBuf;
	inFileBuf.open(FilePath, std::ios_base::in | std::ios_base::binary);
	if (!inFileBuf.is_open()) {
		#ifdef NDEBUG
			std::cerr << "Failed to open file buffer \'" << FilePath << "\'!\n";
		#endif
		return {};
	}
	if (!inFileBuf.pubseekpos(AreaDataLoc)) return {};
	for (size_t i = 0; i < AreaCount; i++)
	{
		IntID areaID;
		inFileBuf.sgetn(reinterpret_cast<char*>(&areaID), VALVE_INT_SIZE) << '\n';
		std::streampos resultPos = inFileBuf.pubseekoff(-VALVE_INT_SIZE, std::ios_base::cur);
		if (areaID == ID) return resultPos;
		else {
			std::optional<size_t> DataLength = TraverseNavAreaData(resultPos);
			if (DataLength.has_value()) {
				if (!inFileBuf.pubseekoff(DataLength.value(), std::ios_base::cur)) return {};
			}
			else {
				std::cerr << "FATAL: Cannot retrive area data size!\n";
				exit(EXIT_FAILURE);
			}
		}
	}
	return {};
}

void NavFile::OutputData(std::ostream& ostream) {
	std::cout << FilePath << ":\n"
	<< "\tMagic Number: 0x" << std::hex << MagicNumber << '\n'
	<< "\tMajor Version: " <<std::dec<< std::to_string(MajorVersion) << '\n';
	if (MinorVersion.has_value()) ostream << "\tMinor Version: " << std::to_string(MinorVersion.value()) << '\n';
	if (BSPSize.has_value()) ostream << "\tReference BSP size: " << std::to_string(BSPSize.value()) << '\n';
	if (isAnalyzed.has_value()) ostream << "\tAnalyzed? " << std::boolalpha << isAnalyzed.value() << '\n';
	ostream << "\tPlace Count: " << std::dec << PlaceCount << '\n';
	if (hasUnnamedAreas.has_value()) ostream << "\tHas unnamed areas? " << std::boolalpha << hasUnnamedAreas.value() << '\n';
	ostream << "\tArea Count: " << std::dec << std::to_string(AreaCount) << '\n'
	<< "\tLadder Count: " << std::to_string(LadderCount) << '\n';
}
#include <fstream>
#include <string>
#include <iostream>
#include <exception>
#include <bit>
#include <unistd.h>
#include <span>
#include "nav_file.hpp"
#include "nav_area.hpp"

NavFile::NavFile() {}

NavFile::NavFile(const std::filesystem::path& path)
: FilePath(path) {
}

NavFile::~NavFile() {}

const std::filesystem::path& NavFile::GetFilePath() {
	return FilePath;
}

unsigned int& NavFile::GetMagicNumber() {
	return MagicNumber;
}

unsigned int& NavFile::GetMajorVersion() {
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

unsigned int& NavFile::GetAreaCount() {
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

/* Write header info.
Returns true on success, false on failure. */
bool NavFile::WriteData(std::streambuf& buf) {
	// Write header.
	if (buf.sputn(reinterpret_cast<char*>(&MagicNumber), VALVE_INT_SIZE) != VALVE_INT_SIZE) {
		#ifdef NDEBUG
		std::cerr << "NavFile::WriteData(): failed to write magic number.\n";
		#endif
		return false;
	}
	// Write major version.
	if (buf.sputn(reinterpret_cast<char*>(&MajorVersion), VALVE_INT_SIZE) != VALVE_INT_SIZE) {
		#ifdef NDEBUG
		std::cerr << "NavFile::WriteData(): failed to write major version.\n";
		#endif
		return false;
	}
	// Write minor version if MajorVersion is at least 10.
	{
		unsigned int tmp = MinorVersion.value_or(0u);
		if (MajorVersion >= 10 && buf.sputn(reinterpret_cast<char*>(&tmp), VALVE_INT_SIZE) != VALVE_INT_SIZE) {
			#ifdef NDEBUG
			std::cerr << "NavFile::WriteData(): failed to write minor version.\n";
			#endif
			return false;
		}

		// Write BSPSize if major version is ≥4.
		tmp = BSPSize.value_or(0u);
		if (MajorVersion >= 4 && buf.sputn(reinterpret_cast<char*>(&tmp), VALVE_INT_SIZE) != VALVE_INT_SIZE) {
			#ifdef NDEBUG
			std::cerr << "NavFile::WriteData(): failed to write BSP size.\n";
			#endif
			return false;
		}
	}
	// Write analyzed boolean if major version is ≥14.
	{
		bool tmp = isAnalyzed.value_or(false);
		if (MajorVersion >= 4 && buf.sputc(tmp) == EOF) {
			#ifdef NDEBUG
			std::cerr << "NavFile::WriteData(): failed to write analyzed boolean.\n";
			#endif
			return false;
		}
	}

	// Write place count.
	if (MajorVersion >= 5)  {
		if (buf.sputn(reinterpret_cast<char*>(&PlaceCount), VALVE_SHORT_SIZE) != VALVE_SHORT_SIZE) {
			#ifdef NDEBUG
			std::cerr << "NavFile::WriteData(): failed to write place count.\n";
			#endif
			return false;
		}
		{
			// Write place data.
			if (PlaceNames.size() < PlaceCount) PlaceNames.resize(PlaceCount);
			if (!std::all_of(PlaceNames.begin(), PlaceNames.end(), [&buf](std::string& placeName) -> bool {
				unsigned short placeNameLength = std::clamp<unsigned short>(placeName.length(), 0u, UINT16_MAX);
				if (buf.sputn(reinterpret_cast<char*>(&placeNameLength), VALVE_SHORT_SIZE) != VALVE_SHORT_SIZE) {
					#ifdef NDEBUG
					std::cerr << "NavFile::WriteData(): failed to write place name length.\n";
					#endif
					return false;
				}
				if (buf.sputn(placeName.data(), placeNameLength) != placeNameLength) {
					#ifdef NDEBUG
					std::cerr << "NavFile::WriteData(): failed to write place name.\n";
					#endif
					return false;
				}
				return true;
			})) {
				#ifdef NDEBUG
				std::cerr << "NavFile::WriteData(): failed to write place data.\n";
				#endif
				return false;
			}
		}

		// Has unnamed areas?
		if (MajorVersion > 11 && buf.sputc(hasUnnamedAreas.value_or(false)) == EOF) {
			#ifdef NDEBUG
			std::cerr << "NavFile::WriteData(): Could not write 'unnamed areas exist' boolean.\n";
			#endif
			return false;
		}
	}
	
	// Write area count.
	if (buf.sputn(reinterpret_cast<char*>(&AreaCount), VALVE_INT_SIZE) != VALVE_INT_SIZE) {
		#ifdef NDEBUG
		std::cerr << "NavFile::WriteData(): Could not write area count.\n";
		#endif
		return false;
	}
	AreaDataLoc = buf.pubseekoff(0, std::ios_base::cur);
	// Writes area data.
	if (AreaCount > 0u) {
		if (!areas.has_value()) areas = std::vector<NavArea>(AreaCount);
		unsigned int index = 0u;
		if (!std::all_of(areas.value().begin(), areas.value().end(), [&buf, &majV = MajorVersion, &minV = MinorVersion, &index](NavArea& area) -> bool {
			if (area.WriteData(buf, majV, minV)) {
				index++;
				return true;
			}
			else {
				#ifdef NDEBUG
				std::cerr << "NavFile::WriteData(): Failed to write area data.\n";
				#endif
				return false;
			}
		})) 
			return false;
	}
	
	// Write ladder count
	if (buf.sputn(reinterpret_cast<char*>(&LadderCount), VALVE_INT_SIZE) != VALVE_INT_SIZE) {
		#ifdef NDEBUG
		std::cerr << "NavFile::WriteData(): Could not write area count.\n";
		#endif
		return false;
	}
	LadderDataLoc = buf.pubseekoff(0, std::ios_base::cur);
	return true;
}
/* Read header info.
Returns true on success, false on failure. */
bool NavFile::ReadData(std::streambuf& buf) {
	// Read header.
	if (buf.sgetn(reinterpret_cast<char*>(&MagicNumber), VALVE_INT_SIZE) != VALVE_INT_SIZE) {
		#ifdef NDEBUG
		std::cerr << "NavFile::ReadData(): failed to write magic number.\n";
		#endif
		return false;
	}
	// Read major version.
	if (buf.sgetn(reinterpret_cast<char*>(&MajorVersion), VALVE_INT_SIZE) != VALVE_INT_SIZE) {
		#ifdef NDEBUG
		std::cerr << "NavFile::ReadData(): failed to write major version.\n";
		#endif
		return false;
	}
	// Read minor version if MajorVersion is at least 10.
	{
		unsigned int tmp;
		if (MajorVersion >= 10) {
			if (buf.sgetn(reinterpret_cast<char*>(&tmp), VALVE_INT_SIZE) != VALVE_INT_SIZE) {
				#ifdef NDEBUG
				std::cerr << "NavFile::ReadData(): failed to write minor version.\n";
				#endif
				return false;
			}
			MinorVersion = tmp;
		}

		// Read BSPSize if major version is ≥4.
		if (MajorVersion >= 4) {
			if (buf.sgetn(reinterpret_cast<char*>(&tmp), VALVE_INT_SIZE) != VALVE_INT_SIZE) {
				#ifdef NDEBUG
				std::cerr << "NavFile::ReadData(): failed to write BSP size.\n";
				#endif
				return false;
			}
			BSPSize = tmp;
		}
	}
	// Read analyzed boolean if major version is ≥14.
	{
		if (MajorVersion >= 4 && (isAnalyzed = buf.sbumpc()) == EOF) {
			#ifdef NDEBUG
			std::cerr << "NavFile::ReadData(): failed to read analyzed boolean.\n";
			#endif
			return false;
		}
	}

	// Read place count.
	if (MajorVersion >= 5)  {
		if (buf.sgetn(reinterpret_cast<char*>(&PlaceCount), VALVE_SHORT_SIZE) != VALVE_SHORT_SIZE) {
			#ifdef NDEBUG
			std::cerr << "NavFile::ReadData(): failed to read place count.\n";
			#endif
			return false;
		}

		// Read place data.
		for (unsigned int i = 0u; i < PlaceCount; i++) {
			unsigned short placeNameLength;
			if (buf.sgetn(reinterpret_cast<char*>(&placeNameLength), VALVE_SHORT_SIZE) != VALVE_SHORT_SIZE) {
				#ifdef NDEBUG
				std::cerr << "NavFile::ReadData(): failed to read place name length.\n";
				#endif
				return false;
			}
			char* placeNameBuf;
			if (buf.sgetn(placeNameBuf, placeNameLength) != placeNameLength) {
				#ifdef NDEBUG
				std::cerr << "NavFile::ReadData(): failed to read place name.\n";
				#endif
				return false;
			}
		}

		// Read has unnamed areas?
		if (MajorVersion > 11) {
			hasUnnamedAreas = buf.sbumpc();
			if (hasUnnamedAreas == EOF) {
				#ifdef NDEBUG
				std::cerr << "NavFile::ReadData(): Could not read 'unnamed areas exist' boolean.\n";
				#endif
				return false;
			}
		}
	}

	// Read area count.
	if (buf.sgetn(reinterpret_cast<char*>(&AreaCount), VALVE_INT_SIZE) != VALVE_INT_SIZE) {
		#ifdef NDEBUG
		std::cerr << "NavFile::ReadData(): Could not read area count.\n";
		#endif
		return false;
	}

	AreaDataLoc = buf.pubseekoff(0, std::ios_base::cur);
	// Reserve memory for areas.
	if (!areas.has_value()) areas = std::vector<NavArea>(AreaCount);
	else {
		areas.value().clear();
		areas.value().resize(AreaCount);
	}
	// Store area data.
	unsigned int index = 0u;
	if (!std::all_of(areas.value().begin(), areas.value().end(), [&buf, &majV = MajorVersion, &minV = MinorVersion, &index](NavArea& area) mutable {
		if (area.ReadData(buf, majV, minV)) {
			index++;
			return true;
		}
		else {
			#ifdef NDEBUG
			std::cerr << "NavFile::ReadData(index "<<std::to_string(index)<<"): failed to read area data.\n";
			#endif
			return false;
		}
	}))
	{
		return false;
	}

	// Read ladder count.
	if (buf.sgetn(reinterpret_cast<char*>(&LadderCount), VALVE_INT_SIZE) != VALVE_INT_SIZE) {
		#ifdef NDEBUG
		std::cerr << "NavFile::ReadData(): Could not read ladder count.\n";
		#endif
		return false;
	}
	// Done
	return true;
}

// Validate the NAV File.
bool NavFile::IsValidFile() {
	if (MagicNumber != 0xFEEDFACE) {
		std::cerr << "Mismatching magic number ("<<std::to_string(MagicNumber)<<"); not a Source Engine NAV file.\n";
		return false;
	}
	return true;
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
				return {};
;			}
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
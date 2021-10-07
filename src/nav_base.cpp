#include <iostream>
#include <optional>
#include <map>
#include <regex>
#include "nav_base.hpp"

// Map between Direction and string.
std::map<Direction, std::string> directionToStr = {
	{Direction::North, "North"},
	{Direction::South, "South"},
	{Direction::West, "West"},
	{Direction::East, "East"}
};

// Map between string and Direction.
std::map<std::string, Direction> strToDirection = {
	{ "north", Direction::North },
	{ "south", Direction::South },
	{ "east", Direction::East },
	{ "west", Direction::West }
};

/*
	Functions
*/
// Get Engine Version from Major and Minor version.
EngineVersion GetAsEngineVersion(const unsigned int& MajorVersion, const std::optional<unsigned int>& MinorVer) {

	if (MajorVersion == 16) return EngineVersion::TEAM_FORTRESS_2;
	
	return EngineVersion::DEFAULT;
}



// get the custom data length (in bytes) from a NAV file.
std::size_t getCustomDataSize(std::streambuf& buf, const EngineVersion& version) {
	switch (version)
	{
	// Team Fortress 2 data:
	// TFAttributes (4-byte).
	case EngineVersion::TEAM_FORTRESS_2:
		return 4u;
		break;
	
	default:
		return 0u;
		break;
	}
}

// Get the ideal attribute flag size (in bytes) of an area.
// Returns a value if it can find the Area attribute size.
std::optional<unsigned char> getAreaAttributeFlagSize(const unsigned int& MajorVersion, const std::optional<unsigned int>& MinorVersion) {
	if (MajorVersion <= 8) return VALVE_CHAR_SIZE;
	else if (MajorVersion < 13) return VALVE_SHORT_SIZE;
	else if (MajorVersion <= LATEST_NAV_MAJOR_VERSION) return VALVE_INT_SIZE;
	else return {};
}

bool NavVisibleArea::WriteData(std::streambuf& out) {
	if (out.sputn(reinterpret_cast<char*>(&VisibleAreaID), VALVE_INT_SIZE) != VALVE_INT_SIZE) {
		std::cerr << "NavVisibleArea::WriteData(): Could not read vis area ID!\n";
		return false;
	}
	if (out.sputc(Attributes) == std::streambuf::traits_type::eof()) {
		#ifdef NDEBUG
		std::cerr << "NavVisibleArea::WriteData(): Could not read vis area attributes!\n";
		#endif
		return false;
	}
	return true;
}

bool NavVisibleArea::ReadData(std::streambuf& in) {
	if (in.sgetn(reinterpret_cast<char*>(&VisibleAreaID), VALVE_INT_SIZE) != VALVE_INT_SIZE) {
		std::cerr << "NavVisibleArea::ReadData(): Could not read area ID!\n";
		return false;
	}
	Attributes = in.sbumpc();
	if (Attributes == std::streambuf::traits_type::eof()) {
		std::cerr << "NavVisibleArea::ReadData(): Could not read vis area attributes!\n";
		return false;
	}
	return true;
}

bool NavVisibleArea::hasSameNAVData(const NavVisibleArea& rhs) const {
	return VisibleAreaID == rhs.VisibleAreaID
	&& Attributes == rhs.Attributes;
}

bool NavHideSpot::WriteData(std::streambuf& out) {
	if (out.sputn(reinterpret_cast<char*>(&ID), VALVE_INT_SIZE) != VALVE_INT_SIZE) {
		#ifdef NDEBUG
		std::cerr << "NavHideSpot::WriteData(): Failed to write area ID!\n";
		#endif
		return false;
	}
	for (auto &&pos : position)
	{
		if (out.sputn(reinterpret_cast<char*>(&pos), VALVE_FLOAT_SIZE) != VALVE_FLOAT_SIZE) {
			#ifdef NDEBUG
			std::cerr << "NavHideSpot::WriteData(): Failed to write hide spot position!\n";
			#endif
			return false;
		}
	}
	
	if (out.sputc(Attributes) == std::streambuf::traits_type::eof()) {
		#ifdef NDEBUG
		std::cerr << "NavHideSpot::WriteData(): Failed to write hide spot attributes!\n";
		#endif
		return false;
	}
	return true;
}

// Read hide spot data from buffer/
// Return true on success. false on failure.
bool NavHideSpot::ReadData(std::streambuf& in) {
	if (in.sgetn(reinterpret_cast<char*>(&ID), VALVE_INT_SIZE) != VALVE_INT_SIZE) {
		std::cerr << "NavHideSpot::ReadData(): Could not read hide spot ID!\n";
		return false;
	}
	if (in.sgetn(reinterpret_cast<char*>(&position), VALVE_FLOAT_SIZE * 3) != VALVE_FLOAT_SIZE * 3) {
		std::cerr << "NavHideSpot::ReadData(): Could not read hide spot position!\n";
		return false;
	}
	Attributes = in.sbumpc();
	if (Attributes == std::streambuf::traits_type::eof()) {
		std::cerr << "NavHideSpot::ReadData(): Could not read attribute flag!\n";
		return false;
	}
	return true;
}

void NavHideSpot::OutputData(std::ostream& out) {
	out << "#" << ID << ":\n"
	<< "\tPosition: " << position[0] << ", " <<position[1] << ", " << position[2] << '\n'
	<< "\tAttributes: " << std::hex << (int)Attributes;
}

bool NavHideSpot::hasSameNAVData(const NavHideSpot& rhs) const {
	return ID == rhs.ID
	&& position == rhs.position
	&& Attributes == rhs.Attributes;
}

// Fill data from stream buffer.
bool NavApproachSpot::ReadData(std::streambuf& in) {
	if (in.sgetn(reinterpret_cast<char*>(&approachHereId), VALVE_INT_SIZE) != VALVE_INT_SIZE) {
		#ifdef NDEBUG
		std::cerr << "NavApproachSpot::ReadData(): Failed to read approach current ID\n";
		#endif
		return false;
	}
	if (in.sgetn(reinterpret_cast<char*>(&approachPrevId), VALVE_INT_SIZE) != VALVE_INT_SIZE) {
		#ifdef NDEBUG
		std::cerr << "NavApproachSpot::ReadData(): Failed to read approach source ID\n";
		#endif
		return false;
	}
	approachType = in.sbumpc();
	if (approachType == EOF) {
		#ifdef NDEBUG
		std::cerr << "NavApproachSpot::ReadData(): Failed to read approach type!\n";
		#endif
		return false;
	}
	if (in.sgetn(reinterpret_cast<char*>(&approachNextId), VALVE_INT_SIZE) != VALVE_INT_SIZE) {
		#ifdef NDEBUG
		std::cerr << "NavApproachSpot::ReadData(): Failed to read next approach ID1\n";
		#endif
		return false;
	}
	approachHow = in.sbumpc();
	if (approachHow == EOF) {
		#ifdef NDEBUG
		std::cerr << "NavApproachSpot::ReadData(): Failed to read approach method!\n";
		#endif
		return false;
	}
	return true;
}

// Write approach spot data to stream.
// Returns true on success, false on failure.
bool NavApproachSpot::WriteData(std::streambuf& out) {
	if (out.sputn(reinterpret_cast<char*>(&approachHereId), VALVE_INT_SIZE) != VALVE_INT_SIZE) {
		#ifdef NDEBUG
		std::cerr << "NavApproachSpot::WriteData(): Failed to write approach source ID!\n";
		#endif
		return false;
	}
	if (out.sputn(reinterpret_cast<char*>(&approachPrevId), VALVE_INT_SIZE) != VALVE_INT_SIZE) {
		#ifdef NDEBUG
		std::cerr << "NavApproachSpot::WriteData(): Failed to write approach prev ID!\n";
		#endif
		return false;
	}
	if (out.sputc(approachType) == EOF) {
		#ifdef NDEBUG
		std::cerr << "NavApproachSpot::WriteData(): Failed to write approach type!\n";
		#endif
		return false;
	}
	if (out.sputn(reinterpret_cast<char*>(&approachNextId), VALVE_INT_SIZE) != VALVE_INT_SIZE) {
		#ifdef NDEBUG
		std::cerr << "NavApproachSpot::WriteData(): Failed to write approach next ID!\n";
		#endif
		return false;
	}
	if (out.sputc(approachHow) == EOF) {
		#ifdef NDEBUG
		std::cerr << "NavApproachSpot::WriteData(): Failed to write approach method!\n";
		#endif
		return false;
	}
	return true;
}

std::optional<bool> NavApproachSpot::hasSameNAVData(NavApproachSpot& rhs) {
	std::stringstream leftStr, rightStr;
	if (!WriteData(*leftStr.rdbuf())) {
		return {};
	}
	if (!rhs.WriteData(*rightStr.rdbuf())) {
		return {};
	}
	return leftStr.str() == rightStr.str();
}

// Read data of the structure.
bool NavEncounterPath::ReadData(std::streambuf& in) {
	if (in.sgetn(reinterpret_cast<char*>(&FromAreaID), VALVE_INT_SIZE) != VALVE_INT_SIZE) {
		std::cerr << "NavEncounterPath::ReadData(): Could not read source area ID!\n";
		return false;
	}
	FromDirection = (Direction)in.sbumpc();
	if (FromDirection == (Direction)std::streambuf::traits_type::eof()) {
		std::cerr << "NavEncounterPath::ReadData(): Could not read source direction!\n";
		return false;
	}
	if (in.sgetn(reinterpret_cast<char*>(&ToAreaID), VALVE_INT_SIZE) != VALVE_INT_SIZE) {
		std::cerr << "NavEncounterPath::ReadData(): Could not read target area ID!\n";
		return false;
	}
	ToDirection = (Direction)in.sbumpc();
	if (ToDirection == (Direction)std::streambuf::traits_type::eof()) {
		std::cerr << "NavEncounterPath::ReadData(): Could not read target direction!\n";
		return false;
	}
	spotCount = in.sbumpc();
	if (spotCount == std::streambuf::traits_type::eof()) {
		std::cerr << "NavEncounterPath::ReadData(): Could not read encounter spot count!\n";
		return false;
	}
	for (size_t i = 0; i < spotCount; i++)
	{
		NavEncounterSpot spot;
		if (!spot.ReadData(in)) {
			std::cerr << "NavEncounterPath::ReadData(): Could not read encounter spot data!\n";
			return false;
		}
		spotContainer.push_back(spot);
	}
	return true;
}

// Write data to the structure.
bool NavEncounterPath::WriteData(std::streambuf& out) {
	if (out.sputn(reinterpret_cast<char*>(&FromAreaID), VALVE_INT_SIZE) != VALVE_INT_SIZE) {
		std::cerr << "NavEncounterPath::WriteData(): Could not write source area ID!\n";
		return false;
	}
	if (out.sputc((char)FromDirection) != (char)FromDirection) {
		std::cerr << "NavEncounterPath::WriteData(): Could not write source direction!\n";
		return false;
	}
	if (out.sputn(reinterpret_cast<char*>(&ToAreaID), VALVE_INT_SIZE) != VALVE_INT_SIZE) {
		std::cerr << "NavEncounterPath::WriteData(): Could not write destination area ID!\n";
		return false;
	}
	if (out.sputc((char)ToDirection) != (char)ToDirection) {
		std::cerr << "NavEncounterPath::WriteData(): Could not write destination direction!\n";
		return false;
	}
	if (out.sputc(spotCount) != spotCount) {
		std::cerr << "NavEncounterPath::WriteData(): Could not write hide spot count!\n";
		return false;
	}
	if (spotCount == spotContainer.size()) for (size_t i = 0; i < spotCount; i++)
	{
		NavEncounterSpot spot;
		if (!spot.WriteData(out)) {
			#ifdef NDEBUG
			std::cerr << "NavEncounterPath::WriteData(): Could not write hide spot data !\n";
			#endif
			return false;
		}
		spotContainer.push_back(spot);
	}
	else {
		NavEncounterSpot blank;
		for (size_t i = 0; i < spotCount; i++)
		{
			if (!blank.WriteData(out)) {
				#ifdef NDEBUG
				std::cerr << "NavEncounterPath::WriteData(): Could not write hide spot data !\n";
				#endif
				return false;
			}
		}
	}
	return true;
}

std::optional<bool> NavEncounterPath::hasSameNAVData(NavEncounterPath& rhs) {
	std::stringstream leftStr, rightStr;
	if (!WriteData(*leftStr.rdbuf())) {
		return {};
	}
	if (!rhs.WriteData(*rightStr.rdbuf())) {
		return {};
	}
	return leftStr.str() == rightStr.str();
}

// Output data.
void NavEncounterPath::Output(std::ostream& out) {
	out << "\tEntry Area (ID): " << FromAreaID
	<< "\n\tDirection of entry: " << directionToStr[FromDirection]
	<< "\n\tTarget Area (ID): " << ToAreaID
	<< "\n\tTarget Direction: " << directionToStr[ToDirection]
	<< "\n\t<" << (int)spotCount << " encounter spots>";
}

bool NavEncounterSpot::ReadData(std::streambuf& in) {
	if (in.sgetn(reinterpret_cast<char*>(&OrderID), VALVE_INT_SIZE) != VALVE_INT_SIZE) {
		std::cerr << "NavEncounterSpot::ReadData(): Could not read order ID!\n";
		return false;
	}
	ParametricDistance = in.sbumpc();
	if (ParametricDistance == std::streambuf::traits_type::eof()) {
		std::cerr << "NavEncounterSpot::ReadData(): Could not read distance!\n";
		return false;
	}
	return true;
}

bool NavEncounterSpot::WriteData(std::streambuf& out) {
	if (out.sputn(reinterpret_cast<char*>(&OrderID), VALVE_INT_SIZE) != VALVE_INT_SIZE) {
		std::cerr << "NavEncounterSpot::WriteData(): Could not write order ID!\n";
		return false;
	}
	if (out.sputc((char)ParametricDistance) != (char)(ParametricDistance)) {
		std::cerr << "NavEncounterSpot::WriteData(): Could not write distance!\n";
		return false;
	}
	return true;
}
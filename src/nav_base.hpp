#ifndef NAV_BASE_HPP
#define NAV_BASE_HPP
#include <map>
#include <fstream>
#include <optional>
#include <vector>
#include <tuple>
#include <any>
// Data type sizes in valve stuff.
#define VALVE_CHAR_SIZE 1
#define VALVE_SHORT_SIZE 2
#define VALVE_INT_SIZE 4
#define VALVE_FLOAT_SIZE 4
#define LATEST_NAV_MAJOR_VERSION 16 // The latest NAV major version used. (Team Fortress 2)

#define NDEBUG

// An ID in NAVs is usually represented as 32-bit, 16-bit, or 8-bit
typedef unsigned int IntID;
typedef unsigned short ShortID;
typedef unsigned char ByteID;

// Cardinal direction.
enum class Direction : unsigned char {
	North = 0,
	East,
	South,
	West,
	Count
};

// Maps.
extern std::map<Direction, std::string> directionToStr;
extern std::map<std::string, Direction> strToDirection;

// Engine Version
enum EngineVersion {
	DEFAULT, // Base Engine
	COUNTER_STRIKE_SOURCE, // CSS
	TEAM_FORTRESS_2, // TF2
	LEFT_4_DEAD, // L4D
	LEFT_4_DEAD_2, // L4D2
	COUNTER_STRIKE_GLOBAL_OFFENSIVE, //CS:GO

	UNKNOWN
};

// Get the Enginer Version from NAV versions.
EngineVersion GetAsEngineVersion(const unsigned int& MajorVersion, const std::optional<unsigned int>& MinorVersion);
// get the custom data length (in bytes) from a NAV file.
std::size_t getCustomDataSize(std::streambuf& buf, const EngineVersion& version);
// Get the ideal attribute flag size (in bytes) of an area.
std::optional<unsigned char> getAreaAttributeFlagSize(const unsigned int& MajorVersion, const std::optional<unsigned int>& MinorVersion);

// Hiding spots.
#define HIDE_SPOT_SIZE (VALVE_INT_SIZE + (VALVE_FLOAT_SIZE * 3) + VALVE_CHAR_SIZE)
class NavHideSpot {
public:
	unsigned int ID; // ID of the hide spot.
	std::array<float, 3> position; // position
	unsigned char Attributes; // Attributes.

	bool ReadData(std::streambuf& in);
	void OutputData(std::ostream& out);
	bool WriteData(std::streambuf& out);

	bool hasSameNAVData(const NavHideSpot& rhs) const;
};

// Approach spots
#define APPROACH_SPOT_SIZE 14 // Size of approach spots in bytes.w
class NavApproachSpot
{
public:
	IntID approachHereId = 0u, // The source ID.
	approachPrevId = 0u, approachNextId = 0u;
	unsigned char approachType = 0u, approachHow = 0u;
	// Write data to stream.
	bool WriteData(std::streambuf& out);
	bool ReadData(std::streambuf& in);

	std::optional<bool> hasSameNAVData(NavApproachSpot& rhs);
};

#define ENCOUNTER_SPOT_SIZE 5 // Total size of encounter spot data.

// NavEncounterSpot represents a spot along an encounter path
class NavEncounterSpot {
public:
	unsigned int OrderID; // The ID of the order of this spot
	float ParametricDistance; // The parametric distance

	bool WriteData(std::streambuf& out);
	bool ReadData(std::streambuf& in);
};

#define ENCOUNTER_PATH_SIZE 10 // Total size of encounter path data.

class NavArea;

// Encounter paths.
class NavEncounterPath {
public:
	unsigned int FromAreaID; // The ID of the area the path comes from
	Direction FromDirection; // The direction from the source
	unsigned int ToAreaID; // The ID of the area the path ends in
	Direction ToDirection; // The direction from the destination
	unsigned char spotCount;
	std::vector<NavEncounterSpot> spotContainer; // The spots along this path

	bool WriteData(std::streambuf& out);
	bool ReadData(std::streambuf& in);

	// Output data.
	void Output(std::ostream& out);

	std::optional<bool> hasSameNAVData(NavEncounterPath& rhs);
};

#define VISIBLE_AREA_SIZE 5 // Total size of visible area data.
// Visible Area
class NavVisibleArea {
public:
	unsigned int VisibleAreaID; // ID of the visible area
	unsigned char Attributes; // Bit-wise attributes

	bool WriteData(std::streambuf& out);
	bool ReadData(std::streambuf& in);

	bool hasSameNAVData(const NavVisibleArea& rhs) const;
};

#endif
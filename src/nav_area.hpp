#include <deque>
#include <list>
#include <variant>
#include <utility>
#include <memory>
#include <vector>
#include <any>
#ifndef NAV_AREA_HPP
#define NAV_AREA_HPP
#include "nav_connections.hpp"
#include "nav_place.hpp"
class NavConnection;
class NavPlace;
class NavApproachSpot;
class NavVisibleArea;
class NavHideSpot;
class NavEncounterSpot;
class NavEncounterPath;

class NavArea {
	public:

	~NavArea();
	// Total size (in bytes)
	size_t size = 0u;
	/* Nav Area data */
	IntID ID = 0u;// ID of the NavArea. 
	unsigned int Flags; // Attributes set on this area.
	std::array<float, 3> nwCorner; // Location of the north-west corners.
	std::array<float, 3> seCorner; // Location of the south-east corners.
	std::optional<float> NorthEastZ, SouthWestZ; // The Z-coord for the north-east and south-west corners.
	std::optional<std::array<float, 4> > LightIntensity; // The light intensity of the corners (NESW).
	// Amount of connections and the connection data in each direction (NESW).
	std::array<std::pair<unsigned int, std::vector<NavConnection> >, 4> connectionData = {
		std::make_pair(0, std::vector<NavConnection>()), 
		std::make_pair(0, std::vector<NavConnection>()), 
		std::make_pair(0, std::vector<NavConnection>()), 
		std::make_pair(0, std::vector<NavConnection>())}; 
	unsigned short PlaceID = 0; // The ID of the place this area is in.
	/*	[1] - Amount of hide spots. 
		[2] - Container
	*/
	std::pair<unsigned char, std::vector<NavHideSpot> > hideSpotData; 
	// Approach spot count (removed in MVer 15)
	unsigned char approachSpotCount = 0u; 
	std::optional<std::vector<NavApproachSpot> > approachSpotData; // Approach spot container (removed in MVer 15)
	std::pair<IntID, std::list<IntID> > ladderData[2]; // Container for ladder data.
	unsigned int encounterPathCount = 0u; // Amount of encounter paths.
	std::optional<std::deque<NavEncounterPath> > encounterPaths;
	std::array<float, 2> EarliestOccupationTimes = {0.0f, 0.0f}; // The earliest time teams can occupy this area

	std::optional<unsigned int> visAreaCount; // <----- Introduced in major version 16.
	std::optional<std::vector<NavVisibleArea> > visAreas; // Amount of vis areas.
	unsigned int InheritVisibilityFromAreaID = 0u; // ID of the area to inherit our visibility from*/

	// Game-specific datum count
	size_t customDataSize = 0u;
	// Game-specific data.
	std::vector<unsigned char> customData;
	// Funcs
	void OutputData(std::ostream& ostream) const;

	// Write data to stream.
	// Returns true if successful, false on failure.
	bool WriteData(std::streambuf& out, const unsigned int& MajorVersion, const std::optional<unsigned int>& MinorVersion);

	// Fills data from stream.
	bool ReadData(std::streambuf& buf, const unsigned int& MajorVersion, const std::optional<unsigned int>& MinorVersion);

	std::optional<bool> hasSameNAVData(const NavArea& rhs, const unsigned int& MajorVersion, const std::optional<unsigned int>& MinorVersion);
};
#define LADDER_SIZE 69 // Total size of ladder data.
#endif
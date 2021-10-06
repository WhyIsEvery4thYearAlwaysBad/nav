#include <iostream>
#include <iomanip>
#include <fstream>
#include <climits>
#include <memory>
#include "nav_area.hpp"
#include "nav_base.hpp"

NavArea::~NavArea() {
	customData;
}

// Outputs data of the nav area.
void NavArea::OutputData(std::ostream& ostream) {
	ostream << "Area #" << std::to_string(ID) << ":\n\tAttribute Flag: 0x" << std::hex << Flags
	<< "\n\tNorthWest corner vector: ";
	for (size_t i = 0; i < nwCorner.size(); i++)
	{
		ostream << std::dec << nwCorner[i];
		if (i < nwCorner.size() - 1) ostream << ", ";
	}
	
	ostream << "\n\tSouthEast corner vector: ";
	for (size_t i = 0; i < seCorner.size(); i++)
	{
		ostream << std::dec << seCorner[i];
		if (i < seCorner.size() - 1) ostream << ", ";
	}
	ostream << "\n\tNorthEastZ: " << (NorthEastZ.has_value() ? std::to_string(NorthEastZ.value()) : "undefined")
	<< "\n\tSouthWestZ: " << (SouthWestZ.has_value() ? std::to_string(SouthWestZ.value()) : "undefined")
	<< "\n\tConnection Count (NESW): ";
	for (unsigned char i = 0; i < (char)Direction::Count; i++)
	{
		ostream << (connectionData[i].first);
		if (i < (char)Direction::Count - 1) ostream << ", ";
	}
	ostream << "\n\tHiding Spot Count: " << std::to_string(std::get<unsigned char>(hideSpotData))
	<< "\n\tEncounter Path count: " << std::to_string(encounterPathCount)
	<< "\n\tPlace ID: " << PlaceID;
	ostream << "\n\tOccupation Times per Team: ";
	for (size_t i = 0; i < EarliestOccupationTimes.size(); i++)
	{
		ostream << EarliestOccupationTimes.at(i);
		if (i < EarliestOccupationTimes.size() - 1) ostream << ", ";
	}	
	if (LightIntensity.has_value()) 
	{
		ostream << "\n\tLight Intensities (NESW): "; 
		for (size_t i = 0; i < (char)Direction::Count; i++)
		{
			ostream << LightIntensity.value()[i];
			if (i < (char)Direction::Count - 1) ostream << ", ";
		}
	}
	ostream << "\n\tVisibility Inheritance ID: " << InheritVisibilityFromAreaID << '\n';
}

// Fill data from buffer.
// Returns true on success, false on failure.
bool NavArea::ReadData(std::streambuf& buf, const unsigned int& MajorVersion, const std::optional<unsigned int>& MinorVersion) {
	std::streampos startPos = buf.pubseekoff(0, std::ios_base::cur);
	if (buf.sgetn(reinterpret_cast<char*>(&ID), VALVE_INT_SIZE) != VALVE_INT_SIZE) {
		std::cerr << "NavArea::ReadData(): FATAL: Could not read area ID!\n";
		return false;
	}; // ID
	{
		// 8-bit flag
		if (MajorVersion < 8) {
			Flags = buf.sbumpc();
			if (Flags == std::streambuf::traits_type::eof()) {
				std::cerr << "NavArea::ReadData(): FATAL: Could not read area 8-bit flag!\n";
				return false;
			}
		}
		// 16-bit flag
		else if (MajorVersion <= 13) {
			unsigned short tmp;
			if (buf.sgetn(reinterpret_cast<char*>(&tmp), VALVE_SHORT_SIZE) != VALVE_SHORT_SIZE) {
				std::cerr << "NavArea::ReadData(): FATAL: Could not read area 16-bit flag!\n";
				return false;
			}
			Flags = tmp;
		}
		// 32-bit flag
		else {
			if (buf.sgetn(reinterpret_cast<char*>(&Flags), VALVE_INT_SIZE) != VALVE_INT_SIZE) {
				std::cerr << "NavArea::ReadData(): FATAL: Could not read area 32-bit flag!\n";
				return false;
			}
		}
	}
	// Read nwCorner corner vector.
	for (size_t i = 0; i < nwCorner.size(); i++)
	{
		if (buf.sgetn(reinterpret_cast<char*>(nwCorner.data() + i), VALVE_FLOAT_SIZE) != VALVE_FLOAT_SIZE) {
			std::cerr << "NavArea::ReadData(): FATAL: Could not read nwCorner["<<i<<"]!\n";
				return false;
		}
	}
	// Read seCorner corner vector.
	for (size_t i = 0; i < seCorner.size(); i++)
	{
		if (buf.sgetn(reinterpret_cast<char*>(seCorner.data() + i), VALVE_FLOAT_SIZE) != VALVE_FLOAT_SIZE) {
			std::cerr << "NavArea::ReadData(): FATAL: Could not read nwCorner["<<i<<"]!\n";
				return false;
		}
	}
	// Read NorthEastZ…
	{
		float tmp;
		if (buf.sgetn(reinterpret_cast<char*>(&tmp), VALVE_FLOAT_SIZE) != VALVE_FLOAT_SIZE) {
			std::cerr << "NavArea::ReadData(): Could not get NorthEastZ!\n";
			return false;
		}
		NorthEastZ = tmp;
		// …and SouthWestZ.
		if (buf.sgetn(reinterpret_cast<char*>(&tmp), VALVE_FLOAT_SIZE) != VALVE_FLOAT_SIZE) {
			std::cerr << "NavArea::ReadData(): Could not get SouthWestZ!\n";
			return false;
		}
		SouthWestZ = tmp;
	}

	// Read connection data.
	for (unsigned char currDirection = (char)Direction::North; currDirection < (char)Direction::Count; currDirection++)
	{
		if (buf.sgetn(reinterpret_cast<char*>(&connectionData[currDirection].first), VALVE_INT_SIZE) != VALVE_INT_SIZE) {
			std::cerr << "NavArea::ReadData(): Could not get direction " << (int)currDirection <<" connection data count!\n";
			return false;
		}
		for (size_t i = 0; i < connectionData[currDirection].first; i++)
		{
			connectionData[currDirection].second.emplace_back();
			if (!connectionData[currDirection].second.back().ReadData(buf)) {
				#ifdef NDEBUG
				std::cerr << "NavArea::ReadData(): Failed to read connection data!\n";
				#endif
				return false;
			}
		}
	}
	// Hiding spot count.
	if (buf.sgetn(reinterpret_cast<char*>(&hideSpotData.first), VALVE_CHAR_SIZE) != VALVE_CHAR_SIZE) {
		std::cerr << "NavArea::ReadData(): Could not get hide spot count!\n";
		return false;
	}
	for (size_t i = 0; i < hideSpotData.first; i++)
	{
		hideSpotData.second.emplace_back();
		if (!hideSpotData.second.back().ReadData(buf)) {
			std::cerr << "NavArea::ReadData(): Could not read hide spot data!\n";
			return false;
		}
	}

	// Approach spots which are not in newer NAV versions.
	if (MajorVersion < 15) {
		approachSpotCount = buf.sbumpc();
		if (approachSpotCount == std::streambuf::traits_type::eof()) {
			std::cerr << "NavArea::ReadData(): Could not read approach spot count!\n";
			return false;
		}
		if (approachSpotCount > 0 && !approachSpotData.has_value()) approachSpotData = std::vector<NavApproachSpot>();
		for (size_t i = 0; i < approachSpotCount; i++)
		{
			approachSpotData.value().emplace_back();
			if (!approachSpotData.value().back().ReadData(buf)) {
				#ifdef NDEBUG
				std::cerr << "NavArea::ReadData(): Failed to read approach spot data!\n";
				#endif
				return false;
			}
		}
	}

	// Encounter Paths.
	if (buf.sgetn(reinterpret_cast<char*>(&encounterPathCount), VALVE_INT_SIZE) != VALVE_INT_SIZE) {
		std::cerr << "NavArea::ReadData(): Could not get encounter path count!\n";
		return false;
	}
	if (encounterPathCount > 0 && !encounterPaths.has_value()) encounterPaths = std::deque<NavEncounterPath>(encounterPathCount);
	for (unsigned int pathIndex=0; pathIndex < encounterPathCount; pathIndex++) {
		if (!encounterPaths.value().at(pathIndex).ReadData(buf)) {
			#ifdef NDEBUG
			std::cerr << "NavArea::ReadData(): Could not read encounter path data!\n";
			#endif
			return false;
		}
	}
	// Read place ID.
	if (buf.sgetn(reinterpret_cast<char*>(&PlaceID), VALVE_SHORT_SIZE) != VALVE_SHORT_SIZE) {
		#ifdef NDEBUG
		std::cerr << "NavArea::ReadData(): Could not get place ID!\n";
		#endif
		return false;
	}
	// Handle ladders
	for (char i = 0; i < 2; i++)
	{
		if (buf.sgetn(reinterpret_cast<char*>(&ladderData[i].first), VALVE_INT_SIZE) != VALVE_INT_SIZE) {
			#ifdef NDEBUG
			std::cerr << "NavArea::ReadData(): Could not get ladder data count (for direction " << i << ")!\n";
			#endif
			return false;
		}
		for (size_t i = 0; i < ladderData[i].first; i++)
		{
			IntID id;
			if (buf.sgetn(reinterpret_cast<char*>(&id), VALVE_INT_SIZE) != VALVE_INT_SIZE) {
				std::cerr << "NavArea::ReadData(): Could not read ladder ID!\n";
				return false;
			}
			ladderData[i].second.push_back(id);
		}
		
	}
	// Occupy times
	for (size_t i = 0; i < EarliestOccupationTimes.size(); i++)
	{
		if (buf.sgetn(reinterpret_cast<char*>(EarliestOccupationTimes.data() + i), VALVE_FLOAT_SIZE) != VALVE_FLOAT_SIZE) {
			std::cerr << "NavArea::ReadData(): Could not read occupy time for team '" << i << "'!\n";
			return false;
		}
	}
	// Light intensity
	if (MajorVersion >= 11) {
		if (!LightIntensity.has_value()) LightIntensity.emplace(std::array<float, 4>({0.0f, 0.0f, 0.0f, 0.0f}));
		for (size_t i = 0; i < (char)Direction::Count; i++)
		{
			if (buf.sgetn(reinterpret_cast<char*>(LightIntensity.value().data() + i), VALVE_FLOAT_SIZE) != VALVE_FLOAT_SIZE) {
				std::cerr << "NavArea::ReadData(): Could not read light intensity for corner["<<i<<"]!\n";
				return false;
			}
		}
	}
	// Visible areas
	if (MajorVersion >= 16) {
		{
			unsigned int tmp;
			if (buf.sgetn(reinterpret_cast<char*>(&tmp), VALVE_INT_SIZE) != VALVE_INT_SIZE) {
				std::cerr << "NavArea::ReadData(): Could not read visible area count!\n";
				return false;
			}
			visAreaCount = tmp;
		}
		if (visAreaCount.value() > 0) 
			visAreas.emplace(std::vector<NavVisibleArea>(visAreaCount.value()));
		for (size_t i = 0; i < visAreaCount; i++)
		{
			if (!visAreas.value().at(i).ReadData(buf)) {
				std::cerr << "NavArea::ReadData(): Could not read visArea data!\n";
				return false;
			}
		}
	}
	// Get InheritVisibilityFromAreaID.
	if (buf.sgetn(reinterpret_cast<char*>(&InheritVisibilityFromAreaID), VALVE_INT_SIZE) != VALVE_INT_SIZE) {
		std::cerr << "NavArea::ReadData(): Could not read InheritVisibilityFromAreaID!\n";
		return false;
	}
	// Create custom game data IF the custom data is there.
	if (buf.in_avail() > 0) {
		customDataSize = getCustomDataSize(buf, GetAsEngineVersion(MajorVersion, {}));
		buf.pubseekoff(-customDataSize, std::ios_base::cur);
		if (buf.sgetn(customData.data(), customDataSize) != customDataSize) {
			#ifdef NDEBUG
			std::cerr << "NavArea::ReadData(): Could not read custom data!\n";
			#endif
			return false;
		}
	}
	// Set area size
	size = buf.pubseekoff(0, std::ios_base::cur) - startPos;
	// Done.
	return true;
}

// Write the nav data into a stream.
bool NavArea::WriteData(std::streambuf& out, const unsigned int& MajorVersion, const std::optional<unsigned int>& MinorVersion) {
	if (out.sputn(reinterpret_cast<char*>(&ID), VALVE_INT_SIZE) != VALVE_INT_SIZE) {
		std::cerr << "NavArea::WriteData(): Could not write area ID!\n";
		return false;
	}
	if (MajorVersion < 8) {
		unsigned char tmp = Flags;
		if (out.sputc(Flags) == std::streambuf::traits_type::eof()) {
			#ifdef NDEBUG
			std::cerr << "NavArea::WriteData(): Failed to write 8-bit attribute flag!\n";
			#endif
			return false;
		}
	}
	else if (MajorVersion <= 13) {
		unsigned short tmp = Flags;
		if (out.sputn(reinterpret_cast<char*>(&tmp), VALVE_SHORT_SIZE) != VALVE_SHORT_SIZE) {
			#ifdef NDEBUG
			std::cerr << "NavArea::WriteData(): Failed to write 16-bit attribute flag!\n";
			#endif
			return false;
		}
	}
	else if (out.sputn(reinterpret_cast<char*>(&Flags), VALVE_INT_SIZE) != VALVE_INT_SIZE) {
		#ifdef NDEBUG
		std::cerr << "NavArea::WriteData(): Failed to write 32-bit attribute flag!\n";
		#endif
		return false;
	}
	
	// Write nwCorner corner vector.
	for (size_t i = 0; i < nwCorner.size(); i++)
	{
		if (out.sputn(reinterpret_cast<char*>(nwCorner.data() + i), VALVE_FLOAT_SIZE) != VALVE_FLOAT_SIZE) {
			#ifdef NDEBUG
			std::cerr << "NavArea::WriteData(): Failed to write nwCorner["<<i<<"]!\n";
			#endif
			return false;
		}
	}
	// Write seCorner corner vector.
	for (size_t i = 0; i < seCorner.size(); i++)
	{
		if (out.sputn(reinterpret_cast<char*>(seCorner.data() + i), VALVE_FLOAT_SIZE) != VALVE_FLOAT_SIZE) {
			#ifdef NDEBUG
			std::cerr << "NavArea::WriteData(): Failed to write seCorner["<<i<<"]!\n";
			#endif
			return false;
		}
	}
	
	// Write NorthEastZ…
	float tmp = NorthEastZ.value_or(0.0f);
	if (out.sputn(reinterpret_cast<char*>(&tmp), VALVE_FLOAT_SIZE) != VALVE_FLOAT_SIZE) {
		#ifdef NDEBUG
		std::cerr << "NavArea::WriteData(): Failed to write NorthEast Z height!\n";
		#endif
		return false;
	}
	// …and SouthWestZ.
	tmp = SouthWestZ.value_or(0.0f);
	if (out.sputn(reinterpret_cast<char*>(&tmp), VALVE_FLOAT_SIZE) != VALVE_FLOAT_SIZE) {
		#ifdef NDEBUG
		std::cerr << "NavArea::WriteData(): Failed to write SouthWest Z height!\n";
		#endif
		return false;
	}

	// Write connection data.
	for (unsigned char currDirection = (char)Direction::North; currDirection < (char)Direction::Count; currDirection++)
	{
		if (out.sputn(reinterpret_cast<char*>(&connectionData[currDirection].first), VALVE_INT_SIZE) != VALVE_INT_SIZE) {
			#ifdef NDEBUG
			std::cerr << "NavArea::WriteData(): Failed to write conneciton count for direction "<<currDirection<<"!\n";
			#endif
			return false;
		}
		for (size_t i = 0; i < connectionData[currDirection].first; i++)
		{
			if (!connectionData[currDirection].second[i].WriteData(out)) {
				#ifdef NDEBUG
				std::cerr << "NavArea::WriteData(): Failed to write connection data!\n";
				#endif
				return false;
			}
		}
	}
	// Hiding spots
	if (out.sputc(hideSpotData.first) == std::streambuf::traits_type::eof()) {
		#ifdef NDEBUG
		std::cerr << "NavArea::WriteData(): Failed to write hide spot count!\n";
		#endif
		return false;
	}
	for (size_t i = 0; i < hideSpotData.first; i++)
	{
		if (!hideSpotData.second[i].WriteData(out)) {
			#ifdef NDEBUG
			std::cerr << "NavArea::WriteData(): Failed to write hide spot data!\n";
			#endif
			return false;
		}
	}
	// Approach spots are not in newer NAV versions.
	if (MajorVersion < 15) {
		if (out.sputc(approachSpotCount) == EOF) {
			#ifdef NDEBUG
			std::cerr << "NavArea::WriteData(): Failed to write approach spot count!\n";
			#endif
			return false;
		}
		if (approachSpotData.has_value()) for (size_t i = 0; i < approachSpotCount; i++)
		{
			if (!approachSpotData.value()[i].WriteData(out)) {
				#ifdef NDEBUG
				std::cerr << "NavArea::WriteData(): Failed to write approach spot data!\n";
				#endif
				return false;
			}
		}
		else for (size_t i = 0; i < approachSpotCount * APPROACH_SPOT_SIZE; i++)
		{
			NavApproachSpot sp;
			if (!sp.WriteData(out)) {
				#ifdef NDEBUG
				std::cerr << "NavArea::WriteData(): Failed to write blank approach spot data!\n";
				#endif
				return false;
			}
		}
	}
	// Encounter Paths.
	if (out.sputn(reinterpret_cast<char*>(&encounterPathCount), VALVE_INT_SIZE) != VALVE_INT_SIZE) {
		#ifdef NDEBUG
		std::cerr << "NavArea::WriteData(): Failed to write encounter path count!\n";
		#endif
		return false;
	}
	if (encounterPaths.has_value()) for (size_t i = 0; i < encounterPathCount; i++)
	{
		if (!encounterPaths.value()[i].WriteData(out)) {
			#ifdef NDEBUG
			std::cerr << "NavArea::WriteData(): Failed to write encounter path data!\n";
			#endif
			return false;
		}
	}

	// Write place ID.
	if (out.sputn(reinterpret_cast<char*>(&PlaceID), VALVE_SHORT_SIZE) != VALVE_SHORT_SIZE) {
		#ifdef NDEBUG
		std::cerr << "NavArea::WriteData(): Failed to write place ID!\n";
		#endif
		return false;
	}
	// Handle ladders
	for (char i = 0; i < 2; i++)
	{
		// Ladder count
		if (out.sputn(reinterpret_cast<char*>(&ladderData[i].first), VALVE_INT_SIZE) != VALVE_INT_SIZE) {
			#ifdef NDEBUG
			std::cerr << "NavArea::WriteData(): Failed to write (direction "<<i<<") ladder count!\n";
			#endif
			return false;
		}
		// Data
		if (ladderData[i].first > 0u) for (auto& ladderID : ladderData[i].second)
		{
			if (out.sputn(reinterpret_cast<char*>(&ladderID), VALVE_INT_SIZE) != VALVE_INT_SIZE) {
				#ifdef NDEBUG
				std::cerr << "NavArea::WriteData(): Failed to write (direction "<<i<<") ladder data!\n";
				#endif
				return false;
			}
		}
	}
	// Occupy times
	for (size_t i = 0u; i < EarliestOccupationTimes.size(); i++) 
		if (out.sputn(reinterpret_cast<char*>(EarliestOccupationTimes.data() + i), VALVE_FLOAT_SIZE) != VALVE_FLOAT_SIZE) {
			#ifdef NDEBUG
			std::cerr << "NavArea::WriteData(): Failed to write occupation time for team "<<i<<" data!\n";
			#endif
			return false;
		}

	// Light intensity
	if (MajorVersion >= 11) {
		float nullval[4] = {0.0f, 0.0f, 0.0f, 0.0f};
		if (LightIntensity.has_value()) for (size_t i = 0; i < (char)Direction::Count; i++)
		{
			if (out.sputn(reinterpret_cast<char*>(LightIntensity.value().data() + i), VALVE_FLOAT_SIZE) != VALVE_FLOAT_SIZE) {
				#ifdef NDEBUG
				std::cerr << "NavArea::WriteData(): Failed to write light intensity for direction "<<i<<"!\n";
				#endif
				return false;
			}
		}
		// No LightIntensities recorded.
		else if (out.sputn(reinterpret_cast<char*>(nullval), VALVE_FLOAT_SIZE * 4) != VALVE_FLOAT_SIZE * 4) {
			#ifdef NDEBUG
			std::cerr << "NavArea::WriteData(): Failed to write blank light intensities!\n";
			#endif
			return false;
		}
	}

	// Visible areas
	if (MajorVersion >= 16) {
		{
			unsigned int tmp = visAreaCount.value_or(0);
			if (out.sputn(reinterpret_cast<char*>(&tmp), VALVE_INT_SIZE) != VALVE_INT_SIZE) {
				std::cerr << "NavArea::WriteData(): Failed to write visibility area count!\n";
				return false;
			}
		}
		if (visAreaCount.value_or(0) > 0) {
			if (visAreas.has_value()) {
				for (auto &&VisArea : visAreas.value())
				{
					if (!VisArea.WriteData(out)) {
						std::cerr << "NavArea::WriteData(): Failed to write visArea data!\n";
						return false;
					}
				}
			}
			// visAreas vector is not set, so write blanks.
			else for (size_t i = 0; i < visAreaCount; i++)
			{
				NavVisibleArea visArea;
				visArea.Attributes = 0;
				visArea.VisibleAreaID = 0u;
				if (!visArea.WriteData(out)) {
					std::cerr << "NavArea::WriteData(): Failed to write blank visArea data!\n";
					return false;
				}
			}
		}
	}
	// Try to write InheritVisibilityFromAreaID.
	if (out.sputn(reinterpret_cast<char*>(&InheritVisibilityFromAreaID), VALVE_INT_SIZE) != VALVE_INT_SIZE) {
		std::cerr << "NavArea::WriteData(): Failed to write InheritVisibilityFromAreaID!\n";
		return false;
	}
	// Write custom data.
	if (customDataSize > 0 && out.sputn(reinterpret_cast<char*>(customData.data()), customDataSize) != customDataSize) {
		std::cerr << "NavArea::WriteData(): Failed to write custom data!\n";
		return false;
	}
	return true;
}

void NavCustomData::ReadData(std::streambuf& out) {

}

void NavCustomData::WriteData(std::streambuf& out) {

}


TF2NavCustomData::TF2NavCustomData() {

}

TF2NavCustomData::TF2NavCustomData(NavCustomData& data) {
	type = data.type;
}

void TF2NavCustomData::ReadData(std::streambuf& buf) {
	buf.sgetn(reinterpret_cast<char*>(&TFAttributes), VALVE_INT_SIZE);
}

void TF2NavCustomData::WriteData(std::streambuf& out) {
	out.sputn(reinterpret_cast<char*>(&TFAttributes), VALVE_INT_SIZE);
}

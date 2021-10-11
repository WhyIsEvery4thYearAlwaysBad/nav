// Map strings to methods that edit a property.
// The methods return how far to jump.
#ifndef PROPERTY_FUNC_MAP_HPP
#define PROPERTY_FUNC_MAP_HPP
#include <unordered_map>
#include <iostream>
#include <functional>
#include "nav_area.hpp"
const std::unordered_map<std::string, std::function<size_t(NavArea&, std::deque<std::string>::iterator, std::deque<std::string>::iterator)> > strAreaEditMethodMap = {
	{"NAV_AREA_ID", [](NavArea& area, std::deque<std::string>::iterator it, std::deque<std::string>::iterator end_it) -> size_t {
		if (std::all_of(it->cbegin(), it->cend(), isxdigit)) {
			area.ID = std::stoul(*it);
			return 1u;
		}
		else {
			std::clog << "Value \'"<<*it<<"\' is not an integer.\n";
			return 0u;
		}
	}},
	{ "NAV_AREA_ATTRIBUTE_FLAG", [](NavArea& area, std::deque<std::string>::iterator it, std::deque<std::string>::iterator end_it) -> size_t { 
		if (std::all_of(it->cbegin(), it->cend(), isxdigit)) {
			area.Flags = std::stoul(*it);
			return 1u;
		}
		else return 0u;
	}},
	{ "NAV_AREA_NORTHWEST_CORNER", [](NavArea& area, std::deque<std::string>::iterator beg_it, std::deque<std::string>::iterator end_it) -> size_t {
		if (std::distance(beg_it, end_it) >= 3u) {
			std::transform(beg_it, beg_it + 3, area.nwCorner.begin(), [](const std::string& s) -> float { return std::stof(s); });
			return 3u;
		}
		else return 0u;
	} },
	{ "NAV_AREA_SOUTHEAST_CORNER", [](NavArea& area, std::deque<std::string>::iterator beg_it, std::deque<std::string>::iterator end_it) -> size_t {
		if (std::distance(beg_it, end_it) >= 3u) {
			std::transform(beg_it, beg_it + 3, area.seCorner.begin(), [](const std::string& s) -> float { return std::stof(s); });
			return 3u;
		}
		else return 0u;
	} },
	// NorthEastZ
	{ "NAV_AREA_NORTHEAST_Z", [](NavArea& area, std::deque<std::string>::iterator it, std::deque<std::string>::iterator end_it) -> size_t { 
		if (std::all_of(it->cbegin(), it->cend(), [](unsigned char c) { return isdigit(c) || c == '.'; })) {
			area.NorthEastZ = std::stof(*it);
			return 1u;
		}
		else {
			std::clog << "Value \'"<<*it<<"\' is not a float.\n";
			return 0u;
		}
	}},
	// SouthWestZ
	{ "NAV_AREA_SOUTHWEST_Z", [](NavArea& area, std::deque<std::string>::iterator it, std::deque<std::string>::iterator end_it) -> size_t { 
		if (std::all_of(it->cbegin(), it->cend(), [](unsigned char c) { return isdigit(c) || c == '.'; })) {
			area.SouthWestZ = std::stof(*it);
			return 1u;
		}
		else {
			std::clog << "Value \'"<<*it<<"\' is not a float.\n";
			return 0u;
		}
	}},
	// Place ID
	{ "NAV_AREA_PLACE_ID", [](NavArea& area, std::deque<std::string>::iterator it, std::deque<std::string>::iterator end_it) -> size_t { 
		if (std::all_of(it->cbegin(), it->cend(), isxdigit)) {
			area.PlaceID = std::stoul(*it);
			return 1u;
		}
		else {
			std::clog << "Value \'"<<*it<<"\' is not a short.\n";
			return 0u;
		}
	}},
	// EarliestOccupationTimes
	{ "NAV_AREA_OCCUPATION_TIMES", [](NavArea& area, std::deque<std::string>::iterator beg_it, std::deque<std::string>::iterator end_it) -> size_t {
		if (std::distance(beg_it, end_it) >= 2u) {
			std::transform(beg_it, beg_it + 2u, area.EarliestOccupationTimes.begin(), [](const std::string& s) -> float { return std::stof(s); });
			return 2u;
		}
		else {
			std::clog << "Expected 2 floats.\n";
			return 0u;
		}
	}},
	// LightIntensity
	{ "NAV_AREA_LIGHT_INTENSITY", [](NavArea& area, std::deque<std::string>::iterator beg_it, std::deque<std::string>::iterator end_it) -> size_t {
		if (area.LightIntensity.has_value()) {
			if (std::distance(beg_it, end_it) >= 4u) {
				std::transform(beg_it, beg_it + 4u, area.LightIntensity.value().begin(), [](const std::string& s) -> float { return std::stof(s); });
				return 4u;
			}
			else {
				std::clog << "Expected 4-float array.\n";
				return 0u;
			}
		}
		else {
			std::clog << "LightIntensity do not exist.\n";
			return false;
		}
	}},
	// InheritVisibilityFromAreaID
	{ "NAV_AREA_VIS_INHERITANCE", [](NavArea& area, std::deque<std::string>::iterator it, std::deque<std::string>::iterator end_it) -> size_t {
		if (std::all_of(it->cbegin(), it->cend(), isxdigit)) {
			area.InheritVisibilityFromAreaID = std::stoul(*it);
			return 1u;
		}
		else {
			std::clog << "Parameter \'"<<*it<<"\' is not an integer.\n";
			return 0u;
		}
	} }
};

// For hide spots
const std::unordered_map<std::string, std::function<size_t(NavHideSpot&, std::deque<std::string>::iterator, std::deque<std::string>::iterator)> > strHideSpotEditMethodMap = {
	{"NAV_HIDE_SPOT_ID", [](NavHideSpot& hideSpot, std::deque<std::string>::iterator it, std::deque<std::string>::iterator end_it) -> size_t {
		if (std::all_of(it->cbegin(), it->cend(), isxdigit)) {
			hideSpot.ID = std::stoul(*it);
			return 1u;
		}
		else {
			std::clog << "Value \'"<<*it<<"\' is not an integer.\n";
			return 0u;
		}
	}},
	// Hide spot attributes.
	{ "NAV_HIDE_SPOT_ATTRIBUTE_FLAG", [](NavHideSpot& hideSpot, std::deque<std::string>::iterator it, std::deque<std::string>::iterator end_it) -> size_t { 
		if (std::all_of(it->cbegin(), it->cend(), isxdigit)) {
			hideSpot.Attributes = std::stoul(*it);
			return 1u;
		}
		else return 0u;
	}},
	// Hide spot position
	{ "NAV_HIDE_SPOT_POSITION", [](NavHideSpot& hideSpot, std::deque<std::string>::iterator beg_it, std::deque<std::string>::iterator end_it) -> size_t {
		if (std::distance(beg_it, end_it) >= 3u) {
			std::transform(beg_it, end_it, hideSpot.position.begin(), [](const std::string& s) -> float { return std::stof(s); });
			return 3u;
		}
		else return 0u;
	} }
};


// For encounter paths
const std::unordered_map<std::string, std::function<size_t(NavEncounterPath&, std::deque<std::string>::iterator, std::deque<std::string>::iterator)> > strHideSpotEditMethodMap = {
	{"NAV_HIDE_SPOT_ID", [](NavHideSpot& hideSpot, std::deque<std::string>::iterator it, std::deque<std::string>::iterator end_it) -> size_t {
		if (std::all_of(it->cbegin(), it->cend(), isxdigit)) {
			hideSpot.ID = std::stoul(*it);
			return 1u;
		}
		else {
			std::clog << "Value \'"<<*it<<"\' is not an integer.\n";
			return 0u;
		}
	}},
	// Hide spot attributes.
	{ "NAV_HIDE_SPOT_ATTRIBUTE_FLAG", [](NavHideSpot& hideSpot, std::deque<std::string>::iterator it, std::deque<std::string>::iterator end_it) -> size_t { 
		if (std::all_of(it->cbegin(), it->cend(), isxdigit)) {
			hideSpot.Attributes = std::stoul(*it);
			return 1u;
		}
		else return 0u;
	}},
	// Hide spot position
	{ "NAV_HIDE_SPOT_POSITION", [](NavHideSpot& hideSpot, std::deque<std::string>::iterator beg_it, std::deque<std::string>::iterator end_it) -> size_t {
		if (std::distance(beg_it, end_it) >= 3u) {
			std::transform(beg_it, end_it, hideSpot.position.begin(), [](const std::string& s) -> float { return std::stof(s); });
			return 3u;
		}
		else return 0u;
	} }
};

#endif
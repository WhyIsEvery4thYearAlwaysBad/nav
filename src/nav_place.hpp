#ifndef NAV_PLACE_HPP
#define NAV_PLACE_HPP
#include <string>
#define PLACE_ID_SIZE 2 // Size of place ids.

class NavArea;

struct NavPlace
{
	unsigned int ID; // ID.
	std::string Name; // Name of the pLace.
	NavArea* Areas; // Areas in the place.
};
#endif
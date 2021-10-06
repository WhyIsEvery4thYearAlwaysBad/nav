#ifndef NAV_CONNECTIONS_HPP
#define NAV_CONNECTIONS_HPP
#include <fstream>
#include "nav_base.hpp"
class NavArea;

#define CONNECTION_SIZE 4
class NavConnection {
	public:
	NavArea *SourceArea; // The starting area for this connection
	IntID TargetAreaID; // The ID of the target area for this NavConnection
	NavArea *TargetArea; // The target area for this connection
	Direction NavDirection; // The direction of the connection between these two areas

	bool ReadData(std::streambuf& buf);
	bool WriteData(std::streambuf& out);
};
#endif
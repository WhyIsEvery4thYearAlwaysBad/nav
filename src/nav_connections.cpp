#include <fstream>
#include <iostream>
#include "nav_connections.hpp"

// Fill data from stream buffer.
// Return true if successful.
bool NavConnection::ReadData(std::streambuf& buf) {
	if (buf.sgetn(reinterpret_cast<char*>(&TargetAreaID), VALVE_INT_SIZE) != VALVE_INT_SIZE) {
		std::cerr << "NavConnection::ReadData(): Failed to read TargetAreaID!\n";
		return false;
	}
	return true;
}

// Write connection data to stream buffer.
// Return true if successful, false upon failure.
bool NavConnection::WriteData(std::streambuf& out) {
	if (out.sputn(reinterpret_cast<char*>(&TargetAreaID), VALVE_INT_SIZE) != VALVE_INT_SIZE) {
		std::cerr << "NavConnection::WriteData(): Could not write area ID";
		return false;
	}
	return true;
}
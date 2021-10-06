#ifndef NAV_TOOL_HPP
#define NAV_TOOL_HPP
#include <vector>
#include "nav_file.hpp"
// Type of command.
enum class CmdType {
	CREATE, // Create area.
	CONNECT, // Connects two areas/ladder.
	DISCONNECT, // Disconnects two areas/ladder.
	DELETE, // Deletes an area/ladder
	MOVE, // Moves an area/ladder
	INFO, // Prints info of nav file.
	COMPRESS, // Compress area IDs
	TEST, // Test program.

	INVALID,
	COUNT
};

// The type of data to apply 
enum class TargetType {
	FILE,
	AREA,
	LADDER,
	CONNECTION,
	ENCOUNTER_PATH,
	ENCOUNTER_SPOT,
	HIDE_SPOT,
	
	INVALID,
	COUNT
};

struct ToolCmd {
	CmdType type = CmdType::INVALID;
	TargetType target = TargetType::INVALID;
	NavFile file;
	// IDs for type of data.
	std::optional<IntID> areaID, encounterPathID;
	std::optional<ByteID> encounterSpotID, hideSpotID;
	std::string param;
};

class NavTool {
	private:		
		ToolCmd cmd;
		NavFile inFile;
		std::string CommandLine;
	public:
		NavTool();
		NavTool(int& argc, char** argv);
		~NavTool();

	std::optional<ToolCmd> ParseCommandLine(int& argc, char **argv);
	bool DispatchCommand(ToolCmd& cmd);
};
#endif
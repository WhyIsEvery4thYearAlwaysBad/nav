#ifndef NAV_TOOL_HPP
#define NAV_TOOL_HPP
#include <vector>
#include "utils.hpp"
#include "nav_base.hpp"
#include "nav_file.hpp"
// Type of command.
enum class ActionType {
	CREATE, // Create area.
	EDIT, // Edit data.
	CONNECT, // Connects two areas/ladder.
	DISCONNECT, // Disconnects two areas/ladder.
	DELETE, // Deletes an area/ladder
	MOVE, // Moves an area/ladder
	INFO, // Prints info of nav file.
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
	ActionType cmdType = ActionType::INVALID;
	TargetType target = TargetType::INVALID;
	std::optional<NavFile> file;
	// IDs for type of data.
	std::optional<IntIndex> areaLocParam;
	std::optional<IntID> encounterPathID, connectionID;
	std::optional<ByteID> encounterSpotID, hideSpotID;
	std::optional<Direction> direc;
	// Parameters for the action.
	std::optional<size_t> RequestedIndex;
	std::deque<std::string> actionParams;
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
	// Executes the create action.
	// True if successful
	bool ActionCreate(ToolCmd& cmd);
	bool ActionEdit(ToolCmd& cmd);
	bool ActionInfo(ToolCmd& cmd);
};
#endif
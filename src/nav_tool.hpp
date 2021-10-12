#ifndef NAV_TOOL_HPP
#define NAV_TOOL_HPP
#include <vector>
#include "toml++/toml.hpp"
#include "utils.hpp"
#include "nav_base.hpp"
#include "nav_file.hpp"
// Type of command.
enum class ActionType {
	CREATE, // Create data.
	EDIT, // Edit data.
	DELETE, // Delete data.
	INFO, // Prints info of nav file.
	TEST, // Test program.
	// I want to add nav_analyze into the program, but that's too heavy handed for me currently.
	// ANALYZE, // Analyzes mesh.

	/* These commands are redundant, as they can be emulated by the above commands.
		CONNECT, // Connects two areas/ladder. (Can be emulated with `nav <path to connection> create` and `nav <path to connection> edit id 5`)
		DISCONNECT, // Disconnects two areas/ladder. (Simply remove a connection.)
		COMPRESS // Sets area IDs to iota. (Use a shell for-loop with `nav <area> edit`) */
	
	COUNT, 
	INVALID
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
	APPROACH_SPOT,
	
	INVALID,
	COUNT
};

struct ToolCmd {
	ActionType cmdType = ActionType::INVALID;
	TargetType target = TargetType::INVALID;
	std::optional<NavFile> file;
	// IDs for type of data.
	std::optional<IntIndex> areaLocParam;
	std::optional<IntID> encounterPathIndex;
	// Index to connection,
	std::optional<std::pair<Direction, IntID> > connectionIndex;
	std::optional<ByteID> encounterSpotID, hideSpotID, approachSpotIndex;
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
		// Program config file.
		toml::parse_result programConfig;
	public:
		NavTool();
		NavTool(int& argc, char** argv);
		~NavTool();

	std::optional<ToolCmd> ParseCommandLine(int& argc, char **argv);
	bool DispatchCommand(ToolCmd& cmd);
	// Executes the create action.
	// True if successful
	// Crate action.
	bool ActionCreate(ToolCmd& cmd);
	// Edit action.
	bool ActionEdit(ToolCmd& cmd);
	// Delete action.
	bool ActionDelete(ToolCmd& cmd);
	// Info action.
	bool ActionInfo(ToolCmd& cmd);
};
#endif
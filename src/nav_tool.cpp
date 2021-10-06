#include <iostream>
#include <iomanip>
#include <map>
#include <utility>
#include <cstring>
#include <functional>
#include <getopt.h>
#include <filesystem>
#include "nav_tool.hpp"
#include "test_automation.hpp"

#define NDEBUG

NavTool::NavTool() {
	
}

NavTool::NavTool(int& argc, char** argv) {
	std::optional<ToolCmd> cmdResult = ParseCommandLine(argc, argv);
	if (cmdResult.has_value()) {
		cmd = cmdResult.value();
		cmdResult.reset();
		if (!DispatchCommand(cmd)) exit(EXIT_FAILURE);
	}
	else exit(EXIT_FAILURE);
}

NavTool::~NavTool() {
	
}

// Mapping from cmd to CmdType
const std::map<std::string, CmdType> cmdStrToCmdType = {
	{"create", CmdType::CREATE},
	{"compress", CmdType::COMPRESS},
	{"connect", CmdType::CONNECT},
	{"disconnect", CmdType::DISCONNECT},
	{"delete",  CmdType::DELETE},
	{"info", CmdType::INFO},
	{"test", CmdType::TEST}
};

// Map to `TargetType` from string.
const std::map<std::string, TargetType> strToTargetType = {
	{"area", TargetType::AREA},
	{"encounter-path", TargetType::ENCOUNTER_PATH},
	{"encounter-spot", TargetType::ENCOUNTER_SPOT},
	{"hide-spot", TargetType::HIDE_SPOT}
};

// Parses Command line.
// Returns ToolCmd if successful, nothing on failure.
std::optional<ToolCmd> NavTool::ParseCommandLine(int& argc, char* argv[]) {
	ToolCmd cmd;
	if (argc < 2) {
		std::cerr << "Usage: nav file <filepath>…. Use '--help' for info.\n";
		return {};
	}
	unsigned int argit = 1u;
	// Get file.
	if (!strcmp(argv[argit], "file")) {
		if (argc < 3) {
			std::cerr << "Missing file path!\n";
			return {};
		}
		argit++;
		// Does the NAV file exist.
		if (!std::filesystem::exists(argv[argit])) {
			std::cerr << "fatal: File \'"<<argv[argit]<<"\' not found.\n";
			return {};
		}
		cmd.file = NavFile(argv[argit]);
		cmd.target = TargetType::FILE;
		// Ensure there is something ahead of us.
		if (argit >= argc - 1) {
			std::cerr << "Missing command!\n";
			return {};
		}
		// Process any target specifiers.
		else {
			argit++;
			auto strTargetMapIt = strToTargetType.find(std::string(argv[argit]));
			// Parse target data.
			std::string argbuf = argv[argit];
			while (argit < argc && strToTargetType.find(argbuf) != strToTargetType.cend())
			{
				argbuf = argv[argit];
				strTargetMapIt = strToTargetType.find(argbuf);
				TargetType t;
				if (strTargetMapIt == strToTargetType.cend()) break;
				else t = strTargetMapIt->second;
				// Grab param.
				switch (t)
				{
				// Target data is an area.
				case TargetType::AREA:
					cmd.areaID = std::stoul(argv[argit + 1]);
					break;
				case TargetType::HIDE_SPOT:
					if (cmd.areaID.has_value()) cmd.hideSpotID = std::stoul(argv[argit + 1]);
					else
					{
						std::cerr << "Area ID not specified!\n";
						return {};
					}
					break;
				case TargetType::ENCOUNTER_PATH:
					cmd.encounterPathID = std::stoul(argv[argit + 1]);
					break;
				case TargetType::ENCOUNTER_SPOT:
					// Ensure encounter path is specified
					if (cmd.target == TargetType::ENCOUNTER_PATH) cmd.encounterSpotID = std::stoul(argv[argit + 1]);
					else {
						std::cerr << "Encounter path must be specified!\n";
						return {};
					}
					break;
				case TargetType::INVALID:
				default:
					break;
				}
				cmd.target = t;
				argit += 2;
			}
			auto strMapIt = cmdStrToCmdType.find(std::string(argv[argit]));
			// Process command.
			if (strMapIt != cmdStrToCmdType.cend())
			{
				cmd.type = strMapIt->second;
			}
		}
	}
	// Test command.
	else if (!strcmp(argv[1], "test")) {
		cmd.type = CmdType::TEST;
	}
	else
	{
		std::cerr << "Expected 'file' argument. Got '" << argv[1] << "'\n";
	}
	
	return cmd;
}

// Actually executed the command.
bool NavTool::DispatchCommand(ToolCmd& cmd) {
	switch (cmd.type)
	{

	// Connect an area.
	case CmdType::CONNECT:
		{
			// Generate temporary file.
			std::filesystem::path TempPath = std::filesystem::temp_directory_path();
			std::cout << "Temp path: " << TempPath.string() << '\n';
		}
		break;
	// Get info of something.
	case CmdType::INFO:
		// Get info of area.
		if (cmd.target != TargetType::FILE) {
			std::optional<std::streampos> areaLoc = cmd.file.FindArea(cmd.areaID.value());
			if (!cmd.areaID.has_value()) {
				#ifdef NDEBUG
				std::cerr << "NavTool::DispatchCommand(ToolCmd&): FATAL: areaID not set!\n";
				#endif
				return false;
			}
			if (areaLoc.has_value()) {
				NavArea targetArea;
				std::filebuf buf;
				if (!buf.open(cmd.file.GetFilePath(), std::ios_base::in | std::ios_base::binary)) {
					std::cerr << "FATAL: Could not open file buffer!\n";
					return false;
				}
				if (!buf.pubseekpos(areaLoc.value())) {
					return false;
				}
				if (!targetArea.ReadData(buf, cmd.file.GetMajorVersion(), cmd.file.GetMinorVersion())) {
					std::cerr << "FATAL: Could not read area data!\n";
					return false;
				}
				// Output
				switch (cmd.target)
				{
				case TargetType::AREA:
					targetArea.OutputData(std::cout);
					break;
				// Output hide spot data.
				case TargetType::HIDE_SPOT:
					{
						if (!cmd.hideSpotID.has_value()) {
							std::cerr << "FATAL: hide spot ID not set!\n";
							return false;
						}
						// Empty hide spot data.
						if (targetArea.hideSpotData.first < 1) {
							std::cout << "Area #" << targetArea.ID << " has 0 hide spots.\n";
							return false;
						}
						// hideSpot ID is higher than hide spot count.
						if (targetArea.hideSpotData.first <= cmd.hideSpotID.value()) {
							std::cerr << "Hide spot #" << std::to_string(cmd.hideSpotID.value()) << " is out of range!\n";
							return false;
						}
						// Output ID.
						std::cout << "Hide Spot #"<<std::to_string(cmd.hideSpotID.value())<<" of Area #"<<targetArea.ID << ":\n";
						std::cout.fill(' ');
						std::cout << std::setw(4) << "Position: ";
						NavHideSpot& hideSpotRef = targetArea.hideSpotData.second.at(cmd.hideSpotID.value());
						// Output hide spot position
						for (size_t i = 0; i <hideSpotRef.position.size(); i++)
						{
							std::cout << hideSpotRef.position.at(i);
							if (i < hideSpotRef.position.size() - 1) std::cout << ", ";
						}
						// Output hide spot attribute.
						std::cout << "\n\tAttributes: " << std::hex << std::showbase << (int)hideSpotRef.Attributes << '\n';
					}
					break;
				// Get encounter path data.
				case TargetType::ENCOUNTER_PATH:
				case TargetType::ENCOUNTER_SPOT:
					{
						// Area has no encounter paths
						if (targetArea.encounterPathCount < 1) {
							std::cout << "Area #" << targetArea.ID << " has 0 encounter paths.\n";
							return false;
						}
						// Look for encounter path.
						if (cmd.target == TargetType::ENCOUNTER_PATH)
						{
							if (targetArea.encounterPathCount <= cmd.encounterPathID.value()) {
								std::cerr << "Requested encounter path is out of range.\n";
								return false;
							}
							std::cout << "Encounter Path #" << cmd.encounterPathID.value() << " of area #" << targetArea.ID <<":\n";
							targetArea.encounterPaths.value().at(cmd.encounterPathID.value()).Output(std::cout);
							std::cout << '\n';
						}
						// Output encounter spot data.
						else if (cmd.target == TargetType::ENCOUNTER_SPOT) {
							if (!cmd.encounterSpotID.has_value()) {
								std::cerr << "FATAL: Encounter Spot ID not set!\n";
								return false;
							}
							if (targetArea.encounterPathCount <= cmd.encounterPathID.value()) {
								std::cerr << "The encounter path ID of the requested encounter spot is out of range.\n";
								return false;
							}
							else {
								if (targetArea.encounterPaths.value().at(cmd.encounterPathID.value()).spotCount <= cmd.encounterSpotID) {
									std::cerr << "Requested Encounter Spot of Area #"<<targetArea.ID<<":Encounter Path #"<<cmd.encounterPathID.value()<<" is out of range.\n";
									return false;
								}
								std::cout << "Encounter Spot #" << cmd.encounterPathID.value() << " of area #" << targetArea.ID;
								NavEncounterSpot& spotRef = targetArea.encounterPaths.value().at(cmd.encounterPathID.value()).spotContainer.at(cmd.encounterSpotID.value());
								std::cout << ":\n\tOrder ID: " << spotRef.OrderID
								<< "\n\tParametric Distance: " << spotRef.ParametricDistance
								<< '\n';
							}
						}
					}
					break;
				default:
					break;
				}
			}
			else
			{
				std::cerr << "Could not find area #" << cmd.areaID.value();
			}
			
		}
		else if (cmd.target == TargetType::FILE)
		{
			cmd.file.OutputData(std::cout);
		}
		else {
			std::cerr << "FATAL: Invalid target!\n";
		}
		break;
	
	// Test
	case CmdType::TEST:
		{
			std::deque<std::function<std::pair<bool, std::string>() > > funcs = {TestNavConnectionDataIO, TestEncounterPathIO, TestNavAreaDataIO};
			for (size_t i = 0; i < funcs.size(); i++)
			{
				std::cout << funcs.at(i)().second << '\n';
			}
		}
		break;
	case CmdType::INVALID:
		std::cerr << "Invalid command!\n";
		return false;

	default:
		break;
	}
	
	return true;
}

int main(int argc, char **argv) {
	NavTool navApp(argc, argv);
	exit(EXIT_SUCCESS);
}
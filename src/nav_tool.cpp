#include <iostream>
#include <iomanip>
#include <map>
#include <utility>
#include <cstring>
#include <functional>
#include <getopt.h>
#include <random>
#include <filesystem>
#include <regex>
#include <iterator>
#include "utils.hpp"
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

// Mapping from cmd to ActionType
const std::map<std::string, ActionType> cmdStrToCmdType = {
	{"create", ActionType::CREATE},
	{"edit", ActionType::EDIT},
	{"connect", ActionType::CONNECT},
	{"disconnect", ActionType::DISCONNECT},
	{"delete",  ActionType::DELETE},
	{"info", ActionType::INFO},
	{"test", ActionType::TEST}
};

// Map to `TargetType` from string.
const std::map<std::string, TargetType> strToTargetType = {
	{"file", TargetType::FILE},
	{"area", TargetType::AREA},
	{"connection", TargetType::CONNECTION},
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
	// Process the target data.
	auto strTargetMapIt = strToTargetType.find(std::string(argv[argit]));
	std::string argbuf = argv[argit];
	while (argit < argc && strToTargetType.find(argbuf) != strToTargetType.cend())
	{
		strTargetMapIt = strToTargetType.find(argbuf);
		TargetType t;
		if (strTargetMapIt == strToTargetType.cend()) break;
		else t = strTargetMapIt->second;
		// Grab param.
		switch (t)
		{

		case TargetType::FILE:
			{
				if (argit + 1 >= argc) {
					std::cerr << "Missing file path.\n";
					return {};
				}
				// Validate path.
				if (!std::filesystem::exists(argv[argit + 1])) {
					std::cerr << "File \'"<<argv[argit + 1] <<"\' does not exist.\n";
					return {};
				}
				else if (std::filesystem::is_directory(argv[argit + 1])) {
					std::cerr << "Path to file is a directory.\n";
					return {};
				}
				// Store path.
				cmd.file.emplace(argv[argit + 1]);
				argit += 2;
			}
			break;
		// Target data is an area.
		case TargetType::AREA:
			if (cmd.file.has_value())
			{
				if (argit + 1 >= argc) {
					std::cerr << "Missing area ID or Index.\n";
					return {};
				}
				auto ret = StrToIndex(argv[argit + 1]);
				// It's invalid.
				if (!ret.has_value()) {
					std::cerr << "Invalid value "<<argv[argit + 1]<<"!\n";
					return {};
				}
				cmd.areaLocParam = ret;
			}
			else {
				std::cerr << "Missing file path to NAV.\n";
				return {};
			}
			argit += 2u;
			break;
		// Target data is hide spot
		case TargetType::HIDE_SPOT:
			if (cmd.areaLocParam.has_value()) {
				if (argit + 1 >= argc) {
					std::cerr << "Missing hide spot index.\n";
					return {};
				}
				cmd.hideSpotID = std::stoul(argv[argit + 1]);
			}
			else
			{
				std::cerr << "Area not specified!\n";
				return {};
			}
			argit += 2u;
			break;
		case TargetType::ENCOUNTER_PATH:
			if (!cmd.areaLocParam.has_value()) {
				std::cerr << "Area not specified!\n";
				return {};
			}
			if (argit + 1 >= argc) {
				std::cerr << "Missing encounter path index.\n";
				return {};
			}
			cmd.encounterPathID = std::stoul(argv[argit + 1]);
			argit += 2u;
			break;
		case TargetType::ENCOUNTER_SPOT:
			// Ensure encounter path is specified
			if (cmd.target == TargetType::ENCOUNTER_PATH) {
				if (argit + 1 >= argc) {
					std::cerr << "Missing encounter spot Index.\n";
					return {};
				}
				cmd.encounterSpotID = std::stoul(argv[argit + 1]);
			}
			else {
				std::cerr << "Encounter path must be specified!\n";
				return {};
			}
			argit += 2u;
			break;
		// Connection
		case TargetType::CONNECTION:
			if (!cmd.areaLocParam.has_value()) {
				std::cerr << "Area ID not specified!\n";
				return {};
			}
			else if (argit + 1 >= argc) {
				std::cerr << "Missing direction parameter.\n";
				return {};
			}
			else if (argit + 2 >= argc) {
				std::cerr << "Missing connection index.\n";
				return {};
			}
			// Parse direction as text or integer.
			if (std::regex_match(argv[argit + 1], std::regex("\\d+"))) cmd.direc = static_cast<Direction>(std::stoi(argv[argit + 1]));
			else {
				auto strDirecIt = strToDirection.find(argv[argit + 1]);
				if (strDirecIt == strToDirection.cend()) {
					std::cerr << "Invalid direction specified!\n";
					return {};
				}
				else cmd.direc = strDirecIt->second;
			}
			cmd.connectionID = std::stoul(argv[argit + 2]);
			argit += 3u;
			break;
		case TargetType::INVALID:
		default:
			std::cerr << "Invalid target \'"<<argv[argit]<<"\'!\n";
			return {};
			break;
		}
		cmd.target = t;
		if (argit < argc) argbuf = argv[argit];
	}
	// Try to process action.
	if (argit < argc) {
		auto strMapIt = cmdStrToCmdType.find(std::string(argv[argit]));
		if (strMapIt != cmdStrToCmdType.cend())
		{
			cmd.cmdType = strMapIt->second;
			// Get action parameters.
			if (cmd.cmdType == ActionType::EDIT) {
				while (++argit < argc)
				{
					cmd.actionParams.emplace_back(argv[argit]);
				}
				if (cmd.actionParams.size() < 2)
				{
					std::cerr << "Missing parameters.\n";
					return {};
				}
			}
		}
	}
	else {
		std::cerr << "Expected command!\n";
		return {};
	}
	// We need a file unless if were testing.
	if (!cmd.file.has_value() && cmd.cmdType != ActionType::TEST) {
		
		std::cerr << "Expected 'file' argument. Got '" << argv[1] << "'\n";
		return {};
	}
	
	return cmd;
}

// Actually executed the command.
bool NavTool::DispatchCommand(ToolCmd& cmd) {
	inFile = cmd.file.value();
	if (cmd.cmdType != ActionType::TEST) {
		// Try to fill in meta data.
		if (!inFile.FillMetaDataFromFile()) {
			std::cerr << "Failed to parse file header.\n";
			return false;
		}
	}
	switch (cmd.cmdType)
	{

	// Create data.
	case ActionType::CREATE:
		return ActionCreate(cmd);
		break;
	case ActionType::EDIT:
		return ActionEdit(cmd);
		break;
	// Get info of something.
	case ActionType::INFO:
		return ActionInfo(cmd);
		break;
	
	// Test
	case ActionType::TEST:
		{
			std::deque<std::function<std::pair<bool, std::string>() > > funcs = {TestNavConnectionDataIO, TestEncounterSpotIO, TestEncounterPathIO, TestNavAreaDataIO};
			for (size_t i = 0; i < funcs.size(); i++)
			{
				std::cout << funcs.at(i)().second << '\n';
			}
		}
		break;
	case ActionType::INVALID:
		std::cerr << "Invalid command!\n";
		return false;

	default:
		break;
	}
	
	return true;
}

// Executes the create action.
// Returns true if successful, false on failure.
bool NavTool::ActionCreate(ToolCmd& cmd) {
	NavFile& ref = inFile;
	switch (cmd.target)
	{
	case TargetType::FILE:
		break;
	
	case TargetType::AREA:
		{
			
		}
		break;
	default:
		break;
	}
	return true;
}

bool NavTool::ActionEdit(ToolCmd& cmd)
{
	std::filesystem::file_status inFileStatus = std::filesystem::status(inFile.GetFilePath());
	// Read only, can't edit.
	if (!(bool)(inFileStatus.permissions() & std::filesystem::perms::owner_write)) {
		std::cerr << "Input file is read only.\n";
		return false;
	}
	// Generate temporary file path.
	std::filesystem::path TempPath = std::filesystem::temp_directory_path();
	{
		std::mt19937 mt_gen;
		std::string tempFileName = std::to_string(mt_gen());
		TempPath.append("nav/");
		// Create directories
		if (!std::filesystem::exists(TempPath)) std::filesystem::create_directories(TempPath);
		TempPath.append(tempFileName);
	}
	// Setup temp file buffer.
	std::filebuf tmpfile;
	if (!tmpfile.open(TempPath, std::ios_base::in | std::ios_base::out | std::ios_base::trunc | std::ios_base::binary)) {
		std::cerr << "Could not create temporary file.\n";
		return false;
	}
	// 
	std::filebuf inBuf;
	if (!inBuf.open(inFile.GetFilePath(), std::ios_base::in | std::ios_base::out | std::ios_base::binary)) {
		std::cerr << "fatal: Failed to open buffer.\n";
		return false;
	}	
	/* 
		TODO: Actually set stuff.
	*/
	switch (cmd.target)
	{
	case TargetType::FILE:
		{
			// TODO:
			return true;
		}
		break;
	case TargetType::LADDER:
		// TODO;
		return true;
		break;
	default:
		break;
	}

	inBuf.pubseekpos(inFile.GetAreaDataLoc());
	// Processing areas.
	if (!cmd.areaLocParam.has_value()) {
		std::cerr << "fatal: area index not defined.\n";
		return false;
	}
	std::optional<std::streampos> areaLoc;
	// It's an ID.
	if (cmd.areaLocParam.value().first == true) 
		areaLoc = inFile.FindArea(cmd.areaLocParam.value().second);
	// It's an index
	else
	{
		// Out of bounds.
		if (std::clamp(cmd.areaLocParam.value().second, 0u, inFile.GetAreaCount()) != cmd.areaLocParam.value().second) {
			std::cerr << "Specified area index is out of range.\n";
			return false;
		}
		areaLoc = inFile.GetAreaDataLoc();
		for (size_t i = 0; i < cmd.areaLocParam.value().second; i++)
		{
			auto areaSize = inFile.TraverseNavAreaData(areaLoc.value());
			if (areaSize.has_value()) areaLoc.value() += areaSize.value();
			else {
				std::cerr << "fatal: Could not get area size!\n";
				return false;
			}
		}
	}
	
	if (!areaLoc.has_value()) {
		std::cerr << "fatal: Failed to get area position!\n";
		return false;
	}
	// Got the area location. Now get data.
	NavArea area;
	inBuf.pubseekpos(areaLoc.value(), std::ios_base::in | std::ios_base::out);
	if (!area.ReadData(inBuf, inFile.GetMajorVersion(), inFile.GetMinorVersion())) {
		std::cerr << "fatal: Could not get area data.\n";
		return false;
	}
	// Now we edit the target data.
	switch (cmd.target) {
		case TargetType::AREA:
			// Get the properties from the arguments and set the area properties to those values.
			for (size_t i = 0; i < cmd.actionParams.size(); i++)
			{
				if (cmd.actionParams.at(i) == "id") area.ID = std::stoul(cmd.actionParams.at(++i), nullptr, 0u);
				else if (cmd.actionParams.at(i) == "attributes") area.Flags = std::stoul(cmd.actionParams.at(++i), nullptr, 0u);
				else if (cmd.actionParams.at(i) == "nwCorner") {
					std::transform(cmd.actionParams.begin() + ++i, cmd.actionParams.begin() + i + 3, area.nwCorner.begin(), [](std::string& s) -> float {
						return std::stof(s);
					});
				}
				else if (cmd.actionParams.at(i) == "seCorner") {
					++i;
					std::transform(cmd.actionParams.begin() + ++i, cmd.actionParams.begin() + i + 3, area.seCorner.begin(), [](std::string& s) -> float {
						return std::stof(s);
					});
				}
				else if (cmd.actionParams.at(i) == "NorthEastZ") area.NorthEastZ = std::stof(cmd.actionParams.at(++i));
				else if (cmd.actionParams.at(i) == "SouthWestZ") area.SouthWestZ = std::stof(cmd.actionParams.at(++i));
				else if (cmd.actionParams.at(i) == "PlaceID") area.PlaceID = std::clamp<ShortID>(std::stoul(cmd.actionParams.at(++i), nullptr, 0u), 0u, UINT16_MAX);

				else if (cmd.actionParams.at(i) == "InheritVisibilityFromAreaID") area.InheritVisibilityFromAreaID = std::stoul(cmd.actionParams.at(++i), nullptr, 0u);
				else if (cmd.actionParams.at(i) == "LightIntensity") 
					std::transform(cmd.actionParams.begin() + ++i, cmd.actionParams.begin() + i + 4, area.LightIntensity.value().begin(), [](std::string& s) -> float {
						return std::stof(s);
					});
			}
			break;
		case TargetType::HIDE_SPOT:
		{
			// Error handling.
			if (!cmd.hideSpotID.has_value()) {
				std::cerr << "fatal: hide spot index not defined.\n";
				return false;
			}
			if (std::clamp<unsigned char>(cmd.hideSpotID.value(), 0, area.hideSpotData.first) != cmd.hideSpotID.value()) {
				std::cerr << "Hide spot index parameter is out of range.\n";
				return false;
			}

			// Get hide spot.
			NavHideSpot& hSpot = area.hideSpotData.second[cmd.hideSpotID.value()];
			// Edit data.
			for (size_t i = 0; i < cmd.actionParams.size(); i++)
			{
				if (cmd.actionParams.at(i) == "id") hSpot.ID = std::stoi(cmd.actionParams.at(++i), nullptr, 0u);
				else if (cmd.actionParams.at(i) == "position") {
					++i;
					std::transform(cmd.actionParams.begin() + i, cmd.actionParams.begin() + i + 2, hSpot.position.begin(), [](const std::string& s) -> float {
						return std::stof(s);
					});
				}
				else if (cmd.actionParams.at(i) == "attributes") hSpot.Attributes = std::stoi(cmd.actionParams.at(++i), nullptr, 0u);
			}
		}
		break;
	}
	inBuf.pubseekpos(areaLoc.value(), std::ios_base::in | std::ios_base::out);
	// Ensure the area data will be properly written.
	if (!area.WriteData(tmpfile, inFile.GetMajorVersion(), inFile.GetMinorVersion())) {
		std::cerr << "Failed to write area data to temporary file!\n";
		return false;
	}
	tmpfile.pubseekpos(0u, std::ios_base::in | std::ios_base::out);
	if (!area.ReadData(tmpfile, inFile.GetMajorVersion(), inFile.GetMinorVersion())) {
		std::cerr << "Failed to read area data to temporary file!\n";
		return false;
	}
	// Clear. Write to inFile.
	if (!area.WriteData(inBuf, inFile.GetMajorVersion(), inFile.GetMinorVersion())) {
		std::cerr << "Failed to read area data to input file!\n";
		return false;
	}
	return true;
}

bool NavTool::ActionInfo(ToolCmd& cmd)
{
	if (cmd.target == TargetType::INVALID) {
		return false;
	}
	else if (cmd.target == TargetType::FILE)
	{
		inFile.OutputData(std::cout);
	}
	// Get info of area.
	else if (cmd.target != TargetType::FILE) {
		if (!cmd.areaLocParam.has_value()) {
			#ifdef NDEBUG
			std::cerr << "NavTool::DispatchCommand(ToolCmd&): FATAL: Area not specified!\n";
			#endif
			return false;
		}
		std::optional<std::streampos> areaLoc;
		
		// Is ID
		if (cmd.areaLocParam.value().first == true) {
			areaLoc = inFile.FindArea(cmd.areaLocParam.value().second);
			if (!areaLoc.has_value()) {
				std::cout << "Could not find area #"<<cmd.areaLocParam.value().second<<"\n";
				return false;
			}
		}
		// Index
		else {
			// Clamp.
			if (std::clamp(cmd.areaLocParam.value().second, 0u, inFile.GetAreaCount()) != cmd.areaLocParam.value().second) {
				std::cerr << "Area index parameter is out of range.\n";
				return false;
			}
			std::filebuf inBuf;
			if (!inBuf.open(inFile.GetFilePath(), std::ios_base::in)) return false;
			areaLoc = inBuf.pubseekpos(inFile.GetAreaDataLoc());
			for (size_t i = 0; i < cmd.areaLocParam.value().second && i < inFile.GetAreaCount(); i++)
			{
				std::optional<size_t> Size = inFile.TraverseNavAreaData(areaLoc.value());
				if (Size.has_value()) areaLoc.value() += Size.value();
				else {
					std::cerr << "fatal: Could not get area size!\n";
					return false;
				}
			}
		}
		if (!areaLoc.has_value())
		{
			if (cmd.areaLocParam.value().first == true) std::cerr << "Could not find area #" << cmd.areaLocParam.value().second << ".\n";
			else std::cerr << "Could not find an area in index " << cmd.areaLocParam.value().second << ".\n";
		}
		NavArea targetArea;
		std::filebuf buf;
		if (!buf.open(inFile.GetFilePath(), std::ios_base::in | std::ios_base::binary)) {
			std::cerr << "FATAL: Could not open file buffer!\n";
			return false;
		}
		if (!buf.pubseekpos(areaLoc.value())) {
			return false;
		}
		if (!targetArea.ReadData(buf, inFile.GetMajorVersion(), inFile.GetMinorVersion())) {
			std::cerr << "FATAL: Could not read area data!\n";
			return false;
		}
		// Output
		switch (cmd.target)
		{
		case TargetType::AREA:
			{
				targetArea.OutputData(std::cout);
				// Also output custom data if possible.
				std::cout << "\tCustom Data: ";
				switch (GetAsEngineVersion(inFile.GetMajorVersion(), inFile.GetMinorVersion()))
				{
					// Get TFAttributes flag.
				case TEAM_FORTRESS_2:
					{
						unsigned int TFAttributes;
						std::copy(targetArea.customData.begin(), targetArea.customData.begin() + 3, &TFAttributes);
						std::cout << "(Team Fortress 2)\n\t\tTFAttributes Flag: " << std::hex << TFAttributes << '\n';
					}
					break;
				
				case EngineVersion::UNKNOWN:
					std::cout << "(Unkown)\n";
					break;
				default:
					break;
				}
			}
			break;
		// Output hide spot data.
		case TargetType::HIDE_SPOT:
			{
				if (!cmd.hideSpotID.has_value()) {
					std::cerr << "fatal: hide spot ID not set!\n";
					return false;
				}
				// Empty hide spot data.
				if (targetArea.hideSpotData.first < 1) {
					std::cout << "Area #" << targetArea.ID << " has 0 hide spots.\n";
					return false;
				}
				// hideSpot ID is higher than hide spot count.
				if (targetArea.hideSpotData.first <= cmd.hideSpotID.value()) {
					std::cerr << "Hide spot #" << std::to_string(cmd.hideSpotID.value()) << " is out of range.\n";
					return false;
				}
				// Output ID.
				std::cout << "Hide Spot #"<<std::to_string(cmd.hideSpotID.value())<<" of Area #"<<targetArea.ID << ":\n";
				std::cout.fill(' ');
				std::cout << std::setw(4) << "\tPosition: ";
				NavHideSpot& hideSpotRef = targetArea.hideSpotData.second.at(cmd.hideSpotID.value());
				// Output hide spot position
				for (size_t i = 0; i < hideSpotRef.position.size(); i++)
				{
					std::cout << hideSpotRef.position.at(i);
					if (i < hideSpotRef.position.size() - 1) std::cout << ", ";
				}
				// Output hide spot attribute.
				std::cout << "\n\tAttributes: " << std::hex << std::showbase << (int)hideSpotRef.Attributes << '\n';
			}
			break;
		case TargetType::CONNECTION:
			{
				// Error checking
				if (!cmd.connectionID.has_value()) {
					std::cerr << "fatal: No connection ID set!\n";
					return false;
				}
				else if (!cmd.direc.has_value()) {
					std::cerr << "fatal: No direction set!\n";
					return false;
				}
				else if (cmd.direc >= Direction::Count || (unsigned char)cmd.direc.value() < 0) {
					std::cerr << "fatal: Invalid direction set!\n";
					return false;
				}
				if (targetArea.connectionData.at((char)cmd.direc.value()).first <= cmd.connectionID.value()) {
					std::cout << "Connection #!"<< cmd.connectionID.value() << " is out of range\n";
					return false;
				}
				// Read.
				NavConnection& ref = targetArea.connectionData.at((unsigned char)cmd.direc.value()).second.at(cmd.connectionID.value());
				// Output
				std::cout << directionToStr[cmd.direc.value()] << " connection #" << cmd.connectionID.value()
				<< ":\n\tTarget Area ID: " << ref.TargetAreaID << '\n';
			}
			break;
		// Get encounter path/spot data.
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
		case TargetType::LADDER:
			// TODO:
			break;
		default:
			break;
		}
	}
	return true;
}

int main(int argc, char **argv) {
	NavTool navApp(argc, argv);
	exit(EXIT_SUCCESS);
}
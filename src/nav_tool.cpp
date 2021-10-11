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
#include <span>
#include "toml++/toml.hpp"
#include "utils.hpp"
#include "property_func_map.hpp"
#include "nav_tool.hpp"
#include "test_automation.hpp"

#define NDEBUG
// Default TOML config.
std::string defaultConf = R"(
[map]
magic_number = "NAV_MESH_MAGIC_NUMBER"
version = "NAV_MESH_VERSION"

area_id = "NAV_AREA_ID" # Area ID
attributes = "NAV_AREA_FLAG" # Attribute Flag
nwCorner = "NAV_AREA_NORTHWEST_CORNER"
seCorner = "NAV_AREA_SOUTHWEST_CORNER"
NorthEastZ = "NAV_AREA_NORTHEAST_Z" # NorthEastZ
vis_inherit = "NAV_AREA_VIS_INHERITANCE" # InheritVisibilityFromAreaID

# Specific data for Team Fortress 2
[map.version.16]
subversion = "NAV_MESH_SUBVERSION"

[map.version.16.custom_data]
offset = 0
length = 4)";

NavTool::NavTool() {
	
}

NavTool::NavTool(int& argc, char** argv) {
	std::optional<ToolCmd> cmdResult = ParseCommandLine(argc, argv);
	if (cmdResult.has_value()) {
		// Try to load in toml config.
		{
			if (getenv("CONFIG")) {
				std::filesystem::path path_to_config = std::string(getenv("CONFIG"));
				// Does it exist?
				if (!std::filesystem::exists(path_to_config)) {
					std::clog << "CONFIG file does not exist.\n";
					exit(EXIT_FAILURE);
				}
				// Validate path.
				std::filesystem::file_status stats = std::filesystem::status(path_to_config);
				// Needs to be able to read file.
				if (!((bool)(stats.permissions() & std::filesystem::perms::owner_read))) {
					std::clog << "warning: Can't read CONFIG file. Ignoring." << std::endl;
				}

				// Attempt to load toml config.
				programConfig = toml::parse_file(path_to_config.string());

				// TODO: Validate loading of toml file.
			}
		}
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
	{"hide-spot", TargetType::HIDE_SPOT},
	{"ladder", TargetType::LADDER}
};

// Map between string and data offset. Used for the edit command.
const std::map<std::string, size_t> strToOffset;

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
			cmd.encounterPathIndex = std::stoul(argv[argit + 1]);
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
			cmd.connectionIndex = std::stoul(argv[argit + 2]);
			argit += 3u;
			break;
		case TargetType::APPROACH_SPOT:
			if (std::regex_match(argv[argit + 1], std::regex("\\d+"))) cmd.approachSpotIndex;
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
			while (++argit < argc)
			{
				cmd.actionParams.emplace_back(argv[argit]);
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
	if (cmd.file.has_value()) inFile = cmd.file.value();
	if (cmd.cmdType != ActionType::TEST) {
		std::filebuf inBuf;
		if (!inBuf.open(inFile.GetFilePath(), std::ios_base::in)) {
			std::cerr << "fatal: Failed to open file buffer.\n";
			return false;
		}
		// Try to fill in file data.
		if (!inFile.ReadData(inBuf)) {
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
	// Delete something.
	case ActionType::DELETE:
		return ActionDelete(cmd);
		break;
	// Test
	case ActionType::TEST:
		{
			std::deque<std::function<std::pair<bool, std::string>() > > funcs = {TestNavConnectionDataIO, TestEncounterSpotIO, TestEncounterPathIO, TestNavAreaDataIO, TestNAVFileIO};
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
		std::filesystem::remove(TempPath);
		return false;
	}
	switch (cmd.target)
	{
	case TargetType::FILE:
		// TODO:
		return true;
		break;
	
	case TargetType::LADDER:
		// TODO:
		return true;
		break;
	case TargetType::INVALID:
		return false;
	default:
		break;
	}

	if (inFile.GetAreaCount() < 1 && cmd.target != TargetType::AREA) {}
	if (!inFile.areas.has_value()) inFile.areas = std::vector<NavArea>(inFile.GetAreaCount());
	
	auto areaIt = inFile.areas.value().end();
	std::optional<std::streampos> areaLoc;
	// Find the area, unless if we're creating an area.
	if (cmd.target != TargetType::AREA) {
		// Processing areas.
		if (!cmd.areaLocParam.has_value()) {
			std::cerr << "fatal: area index not defined.\n";
			std::filesystem::remove(TempPath);
			return false;
		}
		// It's an ID.
		if (cmd.areaLocParam.value().first == true) {
			areaLoc = inFile.FindArea(cmd.areaLocParam.value().second);
		}
		// It's an index
		else
		{
			// Out of bounds.
			if (std::clamp(cmd.areaLocParam.value().second, 0u, inFile.GetAreaCount()) != cmd.areaLocParam.value().second) {
				std::cerr << "Specified area index is out of range.\n";
				std::filesystem::remove(TempPath);
				return false;
			}
			// Set area iterator to the located area at index.
			areaIt = inFile.areas.value().begin() + cmd.areaLocParam.value().second;
		}
	}
	// Now we edit the target data.
	switch (cmd.target) {
		case TargetType::AREA:
			// Add area.
			areaIt = inFile.areas.value().emplace(areaIt);
			// Set new area ID.
			if (cmd.areaLocParam.value().first == true) {
				areaIt->ID = cmd.areaLocParam.value().second;
				// Can't allow for creating areas with duplicate IDs.
			}
			// It's an index. Just use the area count.
			else areaIt->ID = ++inFile.GetAreaCount();
			// Set blank coords.
			areaIt->nwCorner = {0.0f, 0.0f, 0.0f};
			areaIt->seCorner = {0.0f, 0.0f, 0.0f};
			// Fill custom data.
			areaIt->customDataSize = getCustomDataSize(inBuf, GetAsEngineVersion(inFile.GetMajorVersion(), inFile.GetMinorVersion()));
			areaIt->customData.resize(areaIt->customDataSize, 0);
			break;
		// creating hide spot.
		case TargetType::HIDE_SPOT:
		{
			// Error handling.
			if (!cmd.hideSpotID.has_value()) {
				std::cerr << "fatal: hide spot index not defined.\n";
				std::filesystem::remove(TempPath);
				return false;
			}
			// Clamp index.
			if (std::clamp<unsigned char>(cmd.hideSpotID.value(), 0, areaIt->hideSpotData.first) != cmd.hideSpotID.value()) {
				std::cerr << "Hide spot index parameter is out of range.\n";
				std::filesystem::remove(TempPath);
				return false;
			}
			// Create hide spot.
			areaIt->hideSpotData.second.emplace_back();
			areaIt->hideSpotData.first++;
		}
		// Create connection.
		case TargetType::CONNECTION:
			{
				if (!cmd.direc.has_value()) {
					std::cerr << "fatal: Direction not defined.\n";
					return false;
				}
				else if (!cmd.connectionIndex.has_value()) {
					std::cerr << "fatal: Connection index not defined.\n";
				}

				// Make connection and increment connection count.
				areaIt->connectionData.at((unsigned char)cmd.direc.value()).second.emplace_back();
				areaIt->connectionData.at((unsigned char)cmd.direc.value()).first++;
			}
			break;
		
		// Create encounter path.
		case TargetType::ENCOUNTER_PATH:
		{
			if (!cmd.encounterPathIndex.has_value()) {
				std::cerr << "fatal: Encounter path index not set.\n";
				return false;
			}
			if (!areaIt->encounterPaths.has_value()) {
				std::clog << "Area has no encounter paths.\n";
				return false;
			}
			areaIt->encounterPathCount++;
			// Create encounter path.
			areaIt->encounterPaths.value().emplace(areaIt->encounterPaths.value().begin() + cmd.encounterPathIndex.value());
		}
		break;
		// Create encounter spot.
		case TargetType::ENCOUNTER_SPOT:
		{
			if (!cmd.encounterSpotID.has_value()) {
				std::clog << "fatal: Encounter spot index not set.\n";
				return false;
			}
			else if (!cmd.encounterPathIndex.has_value()) {
				std::clog << "fatal: Encounter path index not set.\n";
				return false;
			}
			// Encounter path has to BE there.
			if (areaIt->encounterPathCount < 1) {
				std::clog << "There are no encounter paths\n";
				return false;
			}
			NavEncounterPath& refEPath = areaIt->encounterPaths.value().at(cmd.encounterPathIndex.value());
			// Increment counter.
			refEPath.spotCount++;
			refEPath.spotContainer.emplace(refEPath.spotContainer.begin() + cmd.encounterSpotID.value());
		}
		break;
	}
	
	// Ensure the data will be properly written and read.
	if (!inFile.WriteData(tmpfile)) {
		std::clog << "fatal: Failed to write NAV data to temp buffer.\n";
		std::filesystem::remove(TempPath);
		return false;
	}
	tmpfile.pubseekpos(0u);
	if (!inFile.ReadData(tmpfile)) {
		std::clog << "fatal: Failed to read NAV data to temp buffer.\n";
		std::filesystem::remove(TempPath);
		return false;
	}
	inBuf.pubseekpos(0u);
	// Clear. Write data to inFile.
	if (!inFile.WriteData(inBuf)) {
		std::clog << "Failed to write NAV data to input file!\n";
		std::filesystem::remove(TempPath);
		return false;
	}
	std::filesystem::remove(TempPath);
	return true;
}

bool NavTool::ActionEdit(ToolCmd& cmd)
{
	// Check mappings to ensure they actually exist.
	if (!std::all_of(cmd.actionParams.cbegin(), cmd.actionParams.cend(), [&file = inFile, &conf = programConfig](std::string_view view) -> bool {
		// Ensure it is a number.
		if (std::all_of(view.begin(), view.end(), [](unsigned char c) -> bool {
				return std::isxdigit(c) || c == '.';
			})) {
			return true;
		}
		// or it exists as a toml value.
		else if (conf["map"]["nav_version"][std::to_string(file.GetMajorVersion())] || conf["map"]) {	
			if (conf["map"]["nav_version"][std::to_string(file.GetMajorVersion())][view] || conf["map"][view]) return true;
			else {
				std::clog << "Property alias not found. Exiting.\n";
				return false;
			}
		}
		else {
			std::clog << "'map' table missing, but still attempted to parse string. Exiting\n";
			return false;
		}
	})) {
		return false;
	}
	
	std::filesystem::file_status inFileStatus = std::filesystem::status(inFile.GetFilePath());
	// No write permissions, so can't edit.
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
		std::filesystem::remove(TempPath);
		return false;
	}
	
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
	// Area iterator.
	auto areaIt = inFile.areas.value().end();
	// Processing areas.
	if (!cmd.areaLocParam.has_value()) {
		std::cerr << "fatal: area index not defined.\n";
		std::filesystem::remove(TempPath);
		return false;
	}
	// Area locator parameter an ID. Need to verify the Area with the ID actually exists.
	if (cmd.areaLocParam.value().first == true) {
		// NavFile areas are sorted, so simply use that.
		if (std::is_sorted(inFile.areas.value().begin(), inFile.areas.value().end(), [](const NavArea& lhs, const NavArea& rhs) {
			return lhs.ID < rhs.ID;
		})) {
			if (std::clamp(cmd.areaLocParam.value().second, inFile.areas.value().front().ID, inFile.areas.value().back().ID) != cmd.areaLocParam.value().second) {
				std::clog << "Area ID not found.\n";
				return false;
			}
		}
		else {
			// Copy area IDs.
			std::vector<IntID> AreaIDContainer;
			std::transform(inFile.areas.value().begin(), inFile.areas.value().end(), AreaIDContainer.begin(), [](const NavArea& area) {
				return area.ID;
			});
			// Sort the area IDs.
			std::sort(AreaIDContainer.begin(), AreaIDContainer.end());
			if (std::clamp(cmd.areaLocParam.value().second, inFile.areas.value().front().ID, inFile.areas.value().back().ID) != cmd.areaLocParam.value().second) {
				std::clog << "Area ID not found.\n";
				return false;
			}
		}
		// Validated the area ID is within the area ID range. Search for it.
		areaIt = std::find_if(inFile.areas.value().begin(), inFile.areas.value().end(), [&cmd](const NavArea& area) {
				return cmd.areaLocParam.value().second == area.ID;
			});
	}
	// Area locator parameter is an index. Simply set the iterator to the areas[index].
	else
	{
		// Out of bounds.
		if (std::clamp(cmd.areaLocParam.value().second, 0u, inFile.GetAreaCount()) != cmd.areaLocParam.value().second) {
			std::cerr << "Specified area index is out of range.\n";
			std::filesystem::remove(TempPath);
			return false;
		}
		areaIt = inFile.areas.value().begin() + cmd.areaLocParam.value().second;
	}
	// Now we edit the target data.
	switch (cmd.target) {
		case TargetType::AREA:
			{
				std::stringstream dataBuf;
				
				// Get the properties from the arguments and set the area properties to those values.
				for (size_t i = 0; i < cmd.actionParams.size();)
				{
					// 2 integer values.
					if (std::all_of(cmd.actionParams.at(i).cbegin(), cmd.actionParams.at(i).cend(), isxdigit)) {
						// Error checking.
						if ((cmd.actionParams.begin() + i) >= cmd.actionParams.end())
						{
							std::clog << "Expected an integer.\n";
							return false;
						}
						if (!std::all_of(cmd.actionParams.at(i + 1).cbegin(), cmd.actionParams.at(i + 1).cend(), isxdigit))
						{
							std::clog << "Value \'"<<cmd.actionParams.at(i + 1)<<"\' should be an integer.\n";
							return false;
						}
						//
						dataBuf.clear();
						dataBuf.seekg(0u);
						dataBuf.seekp(0u);
						// Write data to buffer.
						if (!areaIt->WriteData(*dataBuf.rdbuf(), inFile.GetMajorVersion(), inFile.GetMinorVersion())) {
							std::clog << "Failed to store area data. Exiting.\n";
							return false;
						}
						// Make a change.
						dataBuf.seekp(std::stoull(cmd.actionParams.at(i), nullptr, 0u));
						unsigned char Val = std::stoi(cmd.actionParams.at(i + 1), nullptr, 0u);
						dataBuf.put(Val);
						// Read data from buffer.
						if (!areaIt->ReadData(*dataBuf.rdbuf(), inFile.GetMajorVersion(), inFile.GetMinorVersion())) {
							std::clog << "Failed to get edited area data. Exiting.\n";
							return false;
						}
						// Iterate.
						i += 2;
					}
					// 
					else if (programConfig["map"]["nav_version"][std::to_string(inFile.GetMajorVersion())][cmd.actionParams.at(i)]) {
						const std::string propStr = programConfig["map"]["nav_version"][std::to_string(inFile.GetMajorVersion())][cmd.actionParams.at(i)].as_string()->value_exact<std::string>().value_or("");
						size_t arg_count = strAreaEditMethodMap.at(propStr)(std::ref(*areaIt), cmd.actionParams.begin() + i + 1, cmd.actionParams.end());
						// Fail.
						if (arg_count == 0) {
							return false;
						}
						else i += arg_count + 1;
					}
					// Is it a string? Try to parse it as a map to an area property.
					else if (programConfig["map"][cmd.actionParams.at(i)]) {
						const std::string propStr = programConfig["map"][cmd.actionParams.at(i)].as_string()->value_exact<std::string>().value_or("");
						size_t arg_count = strAreaEditMethodMap.at(propStr)(std::ref(*areaIt), cmd.actionParams.begin() + i + 1, cmd.actionParams.end());
						// Fail.
						if (arg_count == 0) {
							return false;
						}
						else i += arg_count + 1;
					}
					else
					{
						std::clog << "Invalid property. Exiting.\n";
						return false;
					}
				}
			}
			break;
		// Editing hide spot.
		case TargetType::HIDE_SPOT:
		{
			// Error handling.
			if (!cmd.hideSpotID.has_value()) {
				std::cerr << "fatal: hide spot index not defined.\n";
				std::filesystem::remove(TempPath);
				return false;
			}
			if (std::clamp<unsigned char>(cmd.hideSpotID.value(), 0, areaIt->hideSpotData.first) != cmd.hideSpotID.value()) {
				std::cerr << "Hide spot index parameter is out of range.\n";
				std::filesystem::remove(TempPath);
				return false;
			}
			std::stringstream dataBuf;
			// New hide spot.
			auto hSpotIt = areaIt->hideSpotData.second.begin() + cmd.hideSpotID.value();
			for (size_t i = 0; i < cmd.actionParams.size(); i++)
			{
				// 2 integer values.
				if (std::all_of(cmd.actionParams.at(i).cbegin(), cmd.actionParams.at(i).cend(), isxdigit)) {
					// Error checking.
					if ((cmd.actionParams.begin() + i) >= cmd.actionParams.end())
					{
						std::clog << "Expected an integer.\n";
						return false;
					}
					if (!std::all_of(cmd.actionParams.at(i + 1).cbegin(), cmd.actionParams.at(i + 1).cend(), isxdigit))
					{
						std::clog << "Value \'"<<cmd.actionParams.at(i + 1)<<"\' should be an integer.\n";
						return false;
					}
					//
					dataBuf.clear();
					dataBuf.seekg(0u);
					dataBuf.seekp(0u);
					// Write data to buffer.
					if (!hSpotIt->WriteData(*dataBuf.rdbuf())) {
						std::clog << "Failed to store area data. Exiting.\n";
						return false;
					}
					// Make a change.
					dataBuf.seekp(std::stoull(cmd.actionParams.at(i), nullptr, 0u));
					unsigned char Val = std::stoi(cmd.actionParams.at(i + 1), nullptr, 0u);
					dataBuf.put(Val);
					// Read data from buffer.
					if (!hSpotIt->ReadData(*dataBuf.rdbuf())) {
						std::clog << "Failed to get edited area data. Exiting.\n";
						return false;
					}
					// Iterate.
					i += 2;
				}
				else if (programConfig["map"]["nav_version"][std::to_string(inFile.GetMajorVersion())][cmd.actionParams.at(i)]) {
					const std::string propStr = programConfig["map"]["nav_version"][std::to_string(inFile.GetMajorVersion())][cmd.actionParams.at(i)].as_string()->value_exact<std::string>().value_or("");
					size_t arg_count = strHideSpotEditMethodMap.at(propStr)(std::ref(*hSpotIt), cmd.actionParams.begin() + i + 1, cmd.actionParams.end());
					// Fail.
					if (arg_count == 0) {
						return false;
					}
					else i += arg_count + 1;
				}
				// Is it a string? Try to parse it as an alias.
				else if (programConfig["map"][cmd.actionParams.at(i)]) {
					const std::string propStr = programConfig["map"][cmd.actionParams.at(i)].as_string()->value_exact<std::string>().value_or("");
					size_t arg_count = strHideSpotEditMethodMap.at(propStr)(std::ref(*hSpotIt), cmd.actionParams.begin() + i + 1, cmd.actionParams.end());
					// Fail.
					if (arg_count == 0) {
						return false;
					}
					else i += arg_count + 1;
				}
				else
				{
					std::clog << "Invalid property. Exiting.\n";
					return false;
				}
			}
		}
		// Encounter path.
		case TargetType::ENCOUNTER_PATH:
			{
				if (!cmd.encounterPathIndex.has_value()) {
					std::clog << "Encounter path index is undefined." << std::endl;
				}
				auto ePathIt = areaIt->encounterPaths.value().begin() + cmd.encounterPathIndex.value();
				for (size_t i = 0; i < cmd.actionParams.size(); i++)
				{
					if (std::all_of(cmd.actionParams.at(i).cbegin(), cmd.actionParams.at(i).cend(), isxdigit)) {
						std::streampos position = std::stoull(cmd.actionParams.at(i), nullptr, 0u);
					}
				}
			}
			break;
		case TargetType::ENCOUNTER_SPOT:
			{
				if (!cmd.encounterPathIndex.has_value())
				{
					std::clog << "Encounter path index is undefined." << std::endl;
				}
				auto eSpotIt = areaIt->encounterPaths.value().at(cmd.encounterPathIndex.value()).spotContainer.at(cmd.encounterSpotID.value());

				// TODO:
			}
			break;
		default:
			std::clog << "Can't handle this type of data yet\n";
			break;
	}
	// Ensure the NAV data will be properly written.
	if (!inFile.WriteData(tmpfile)) {
		std::clog << "Failed to write area data to temporary file!\n";
		std::filesystem::remove(TempPath);
		return false;
	}
	tmpfile.pubseekpos(0u, std::ios_base::in | std::ios_base::out);
	if (!inFile.ReadData(tmpfile)) {
		std::clog << "Failed to read area data to temporary file!\n";
		std::filesystem::remove(TempPath);
		return false;
	}
	// Clear. Write to inFile
	inBuf.pubseekpos(0);
	if (!inFile.WriteData(inBuf)) {
		std::clog << "Failed to read area data to input file!\n";
		std::filesystem::remove(TempPath);
		return false;
	}
	std::filesystem::remove(TempPath);
	return true;
}


bool NavTool::ActionDelete(ToolCmd& cmd)
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
		std::filesystem::remove(TempPath);
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
	// Iterator to specified area.
	auto areaIt = inFile.areas.value().end();
	// Processing areas.
	if (!cmd.areaLocParam.has_value()) {
		std::cerr << "fatal: area index not defined.\n";
		std::filesystem::remove(TempPath);
		return false;
	}
	// It's an ID. Do some checking to ensure we can reach it.
	if (cmd.areaLocParam.value().first == true) 
		{
			// Sort area container so that we can get the mins and maxes.
			if (!std::is_sorted(inFile.areas.value().begin(), inFile.areas.value().end(), [](const NavArea& lhs, const NavArea& rhs) {return lhs.ID < rhs.ID;})) 
				std::sort(inFile.areas.value().begin(), inFile.areas.value().end(), [](const NavArea& lhs, const NavArea& rhs) constexpr -> bool {
				return lhs.ID < rhs.ID;
			});
			// Check if the ID is within the minmax ID range.
			if (std::clamp(cmd.areaLocParam.value().second, inFile.areas.value().front().ID, inFile.areas.value().back().ID) != cmd.areaLocParam.value().second) {
				std::cerr << "Requested ID is outside the found ID range #"<<std::to_string(inFile.areas.value().front().ID)<<" to #"<<std::to_string(inFile.areas.value().back().ID)<<".\n";
				return false;
			}
			// Try find the area.
			areaIt = std::find_if<std::vector<NavArea>::iterator >(inFile.areas.value().begin(), inFile.areas.value().end(), [&cmd](const NavArea& a) constexpr -> bool {
				return cmd.areaLocParam.value().second == a.ID;
			});
		}
	// It's an index
	else
	{
		// Out of bounds.
		unsigned int preClampValue = cmd.areaLocParam.value().second;
		if (std::clamp<unsigned int>(cmd.areaLocParam.value().second, 0u, inFile.areas.value().size()) != preClampValue) {
			std::cerr << "Specified area index is out of range.\n";
			std::filesystem::remove(TempPath);
			return false;
		}
		// Go to the area where the index points to.
		areaIt = inFile.areas.value().begin() + cmd.areaLocParam.value().second;
	}
	// Did we find the specified area.
	if (areaIt == inFile.areas.value().cend()) {
		std::cerr << "Could not find area.\n";
		return false;
	}
	// We got the area. 
	// Now we delete the target data.
	switch (cmd.target) {
		case TargetType::AREA:
			// Delete area.
			--inFile.GetAreaCount();
			inFile.areas.value().erase(areaIt);
			break;
		case TargetType::HIDE_SPOT:
		{
			// Error handling.
			if (!cmd.hideSpotID.has_value()) {
				std::cerr << "fatal: hide spot index not defined.\n";
				std::filesystem::remove(TempPath);
				return false;
			}
			if (std::clamp<unsigned char>(cmd.hideSpotID.value(), 0, areaIt->hideSpotData.first) != cmd.hideSpotID.value()) {
				std::cerr << "Hide spot index parameter is out of range.\n";
				std::filesystem::remove(TempPath);
				return false;
			}
			// Remove hide spot.
			if (areaIt->hideSpotData.second.erase(areaIt->hideSpotData.second.begin() + cmd.hideSpotID.value()) == areaIt->hideSpotData.second.cend()) {
				std::cerr << "fatal: Could not locate.\n";
			}
			areaIt->hideSpotData.first--;
		}

		case TargetType::CONNECTION:
			if (!cmd.connectionIndex.has_value()) {
				std::cerr << "fatal: connection index not defined.\n";
				return false;
			}
			if (!cmd.direc.has_value())
			{
				std::cerr << "fatal: direction not defined.\n";
				return false;
			}
			// Remove connection.
			areaIt->connectionData[(unsigned char)cmd.direc.value()].second.erase(areaIt->connectionData[(unsigned char)cmd.direc.value()].second.begin() + cmd.connectionIndex.value());
			areaIt->connectionData[(unsigned char)cmd.direc.value()].first--;
			break;
		default:
		break;
	}
	// Ensure the NAV data will be properly written.
	if (!inFile.WriteData(tmpfile)) {
		std::cerr << "Failed to write area data to temporary file!\n";
		std::filesystem::remove(TempPath);
		return false;
	}
	tmpfile.pubseekpos(0u, std::ios_base::in | std::ios_base::out);
	if (!inFile.ReadData(tmpfile)) {
		std::cerr << "Failed to read area data to temporary file!\n";
		std::filesystem::remove(TempPath);
		return false;
	}
	// Clear. Write to inFile.
	inBuf.pubseekpos(0u);
	if (!inFile.WriteData(inBuf)) {
		std::cerr << "Failed to read area data to input file!\n";
		std::filesystem::remove(TempPath);
		return false;
	}
	std::filesystem::remove(TempPath);
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
				// Get hide spot data.
				NavHideSpot& hideSpotRef = targetArea.hideSpotData.second.at(cmd.hideSpotID.value());
				// Output ID.
				std::cout << "Hide Spot #"<<std::to_string(cmd.hideSpotID.value())<<" of Area #"<<targetArea.ID << ":\n\tID: " << std::to_string(hideSpotRef.ID) << '\n';
				std::cout << std::setw(4) << "\tPosition: ";
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
				if (!cmd.connectionIndex.has_value()) {
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
				if (targetArea.connectionData.at((char)cmd.direc.value()).first <= cmd.connectionIndex.value()) {
					std::cout << "Connection #"<< cmd.connectionIndex.value() << " is out of range!\n";
					return false;
				}
				// Read.
				NavConnection& ref = targetArea.connectionData.at((unsigned char)cmd.direc.value()).second.at(cmd.connectionIndex.value());
				// Output
				std::cout << directionToStr[cmd.direc.value()] << " connection #" << cmd.connectionIndex.value()
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
					if (targetArea.encounterPathCount <= cmd.encounterPathIndex.value()) {
						std::cerr << "Requested encounter path is out of range.\n";
						return false;
					}
					std::cout << "Encounter Path #" << cmd.encounterPathIndex.value() << " of area #" << targetArea.ID <<":\n";
					targetArea.encounterPaths.value().at(cmd.encounterPathIndex.value()).Output(std::cout);
					std::cout << '\n';
				}
				// Output encounter spot data.
				else if (cmd.target == TargetType::ENCOUNTER_SPOT) {
					if (!cmd.encounterSpotID.has_value()) {
						std::cerr << "FATAL: Encounter Spot ID not set!\n";
						return false;
					}
					if (targetArea.encounterPathCount <= cmd.encounterPathIndex.value()) {
						std::cerr << "The encounter path ID of the requested encounter spot is out of range.\n";
						return false;
					}
					else {
						if (targetArea.encounterPaths.value().at(cmd.encounterPathIndex.value()).spotCount <= cmd.encounterSpotID) {
							std::cerr << "Requested Encounter Spot of Area #"<<targetArea.ID<<":Encounter Path #"<<cmd.encounterPathIndex.value()<<" is out of range.\n";
							return false;
						}
						std::cout << "Encounter Spot #" << cmd.encounterPathIndex.value() << " of area #" << targetArea.ID;
						NavEncounterSpot& spotRef = targetArea.encounterPaths.value().at(cmd.encounterPathIndex.value()).spotContainer.at(cmd.encounterSpotID.value());
						std::cout << ":\n\tOrder ID: " << spotRef.OrderID
						<< "\n\tParametric Distance: " << spotRef.ParametricDistance
						<< '\n';
					}
				}
			}
			break;

		case TargetType::APPROACH_SPOT:
			{
				// Check version.
				if (inFile.GetMajorVersion() >= 15) {
					std::cout << "This file is too new to contain approach spots.\n";
					return false;
				}
				else if (!cmd.approachSpotIndex.has_value()) {
					std::cerr << "fatal: approach spot ID not defined!\n";
					return false;
				}
				if (!targetArea.approachSpotData.has_value()) {
					std::cerr << "fatal: approach spot data not defined.\n";
					return false;
				}
				NavApproachSpot& aSpotRef = targetArea.approachSpotData.value().at(cmd.approachSpotIndex.value());

				// Output.
				std::cout << "Approach Spot ["<<std::to_string(cmd.approachSpotIndex.value())
				<<"]:\n\tApproach Type: " << std::to_string(aSpotRef.approachType)
				<< "\n\tApproach Method:" << std::to_string(aSpotRef.approachHow)
				<< "\n\tPrev ID: " << std::to_string(aSpotRef.approachPrevId)
				<< "\n\tDestination: " << std::to_string(aSpotRef.approachHereId)
				<< "\n\tNext ID: " << std::to_string(aSpotRef.approachNextId);
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
	// Remove temporary files.
	std::filesystem::remove_all(std::filesystem::temp_directory_path().append("nav/"));
	exit(EXIT_SUCCESS);
}
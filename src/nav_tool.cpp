// #define NDEBUG 
// ^ Uncomment this line for release builds.
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
#include <cassert>
#include "toml++/toml.hpp"
#include "utils.hpp"
#include "property_func_map.hpp"
#include "nav_tool.hpp"
#include "test_automation.hpp"

#define NDEBUG
// Default TOML config.
std::string defaultConf = R"(
[map]
NAV_MESH_MAGIC_NUMBER = "NAV_MESH_MAGIC_NUMBER"
NAV_MESH_VERSION = "NAV_MESH_VERSION"
# Area ID.
area_id = "NAV_AREA_ID"
NAV_AREA_ID = "NAV_AREA_ID"
# Attributes
NAV_AREA_ATTRIBUTE_FLAG = "NAV_AREA_ATTRIBUTE_FLAG"
attributes = "NAV_AREA_ATTRIBUTE_FLAG" # Attribute Flag
# North-West Corner vector.
NAV_AREA_NORTHWEST_CORNER = "NAV_AREA_NORTHWEST_CORNER"
nwCorner = "NAV_AREA_NORTHWEST_CORNER"
# South-East Corner vector.
NAV_AREA_SOUTHWEST_CORNER = "NAV_AREA_SOUTHWEST_CORNER"
seCorner = "NAV_AREA_SOUTHWEST_CORNER"
# NorthEastZ
NAV_AREA_NORTHEAST_Z = "NAV_AREA_NORTHEAST_Z"
NorthEastZ = "NAV_AREA_NORTHEAST_Z"
# SouthEastZ
NAV_AREA_SOUTHEAST_Z = "NAV_AREA_SOUTHEAST_Z"
SouthEastZ = "NAV_AREA_SOUTHEAST_Z"

# InheritVisibilityFromAreaID
NAV_AREA_VIS_INHERITANCE = "NAV_AREA_VIS_INHERITANCE"
vis_inherit = "NAV_AREA_VIS_INHERITANCE"

# Hide Spot
NAV_HIDE_SPOT_ID = "NAV_HIDE_SPOT_ID"
hide_spot_id = "NAV_HIDE_SPOT_ID"

# Encounter paths
NAV_ENCOUNTER_PATH_ENTRY_AREA_ID = "NAV_ENCOUNTER_PATH_ENTRY_AREA_ID"
entry_area_id = "NAV_ENCOUNTER_PATH_ENTRY_AREA_ID"

NAV_ENCOUNTER_PATH_ENTRY_DIRECTION = "NAV_ENCOUNTER_PATH_ENTRY_DIRECTION"
entry_direction = "NAV_ENCOUNTER_PATH_ENTRY_DIRECTION"

[map.nav_version]
	# Specific maps for Team Fortress 2
	[map.nav_version.16]
	# Subversion
	NAV_MESH_SUBVERSION = "NAV_MESH_SUBVERSION"
	subversion = "NAV_MESH_SUBVERSION"
	# Places
	NAV_AREA_PLACE_ID = "NAV_AREA_PLACE_ID"
	place_id = "NAV_AREA_PLACE_ID"
	# Light Intensity
	NAV_AREA_LIGHT_INTENSITY = "NAV_AREA_LIGHT_INTENSITY"
	light_intensities = "NAV_AREA_LIGHT_INTENSITY")";

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
		if (!DispatchCommand(cmd)) {
			exit(EXIT_FAILURE);
		}
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
					std::clog << "Missing file path.\n";
					return {};
				}
				// Validate path.
				if (!std::filesystem::exists(argv[argit + 1])) {
					std::clog << "File \'"<<argv[argit + 1] <<"\' does not exist.\n";
					return {};
				}
				else if (std::filesystem::is_directory(argv[argit + 1])) {
					std::clog << "Path to file is a directory.\n";
					return {};
				}
				// Store path.
				cmd.file.emplace(argv[argit + 1]);
				argit += 2;
			}
			break;
		// Target data is an area.
		case TargetType::AREA:
		case TargetType::LADDER:
			if (cmd.file.has_value())
			{
				if (argit + 1 >= argc) {
					std::clog << "Missing area ID or Index.\n";
					return {};
				}
				auto ret = StrToIndex(argv[argit + 1]);
				// It's invalid.
				if (!ret.has_value()) {
					std::clog << "Invalid value \'"<<argv[argit + 1]<<"\'!\n";
					return {};
				}
				cmd.areaLocParam = ret;
			}
			else {
				std::clog << "Missing file path to NAV.\n";
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
			argit += 2u;
			break;
		case TargetType::ENCOUNTER_PATH:
			if (argit + 1 >= argc) {
				std::clog << "Missing encounter path index.\n";
				return {};
			}
			cmd.encounterPathIndex = std::stoul(argv[argit + 1]);
			argit += 2u;
			break;
		case TargetType::ENCOUNTER_SPOT:
			// Ensure encounter path is specified
			if (cmd.target == TargetType::ENCOUNTER_PATH) {
				if (argit + 1 >= argc) {
					std::clog << "Missing encounter spot Index.\n";
					return {};
				}
				cmd.encounterSpotID = std::stoul(argv[argit + 1]);
			}
			else {
				std::clog << "Encounter path must be specified!\n";
				return {};
			}
			argit += 2u;
			break;
		// Connection
		case TargetType::CONNECTION:
			if (argit + 1 >= argc) {
				std::clog << "Missing direction parameter.\n";
				return {};
			}
			else if (argit + 2 >= argc) {
				std::clog << "Missing connection index.\n";
				return {};
			}
			{
				Direction direc;
				// Parse direction as text or integer.
				if (std::regex_match(argv[argit + 1], std::regex("\\d+"))) {
					// Clamp value.
					direc = static_cast<Direction>(std::stoi(argv[argit + 1]));
					if (std::clamp(direc, Direction::North, Direction::West) != direc) {
						std::clog << "warning: Direction value out of range. Clamping direction value!\n";
						direc = std::clamp(direc, Direction::North, Direction::West);
					}
				}
				else {
					auto strDirecIt = strToDirection.find(argv[argit + 1]);
					if (strDirecIt == strToDirection.cend()) {
						std::clog << "Invalid direction specified!\n";
						return {};
					}
					else direc = strDirecIt->second;
				}
				// Check connection index.
				std::string buf = argv[argit + 2];
				if (!std::all_of(buf.cbegin(), buf.cend(), isxdigit)) {
					std::clog << "Invalid connection index.\n";
					return {};
				}
				cmd.connectionIndex = std::make_pair(direc, std::stoul(buf));
			}
			argit += 3u;
			break;
		case TargetType::APPROACH_SPOT:
			if (std::regex_match(argv[argit + 1], std::regex("\\d+"))) cmd.approachSpotIndex;
			break;
		case TargetType::INVALID:
		default:
			std::clog << "Invalid target \'"<<argv[argit]<<"\'!\n";
			return {};
			break;
		}
		cmd.target = t;
		if (argit < argc) argbuf = argv[argit];
	}
	// Area is unspecified.
	if (!cmd.areaLocParam.has_value() && cmd.target != TargetType::FILE)
	{
		std::clog << "Area not specified!\n";
		return {};
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
		std::clog << "Expected command!\n";
		return {};
	}
	// We need a file unless if were running test command.
	if (!cmd.file.has_value() && cmd.cmdType != ActionType::TEST) {
		
		std::clog << "Expected 'file' argument. Got '" << argv[1] << "'\n";
		return {};
	}
	// Successfully parsed command line.
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
			std::clog << "Failed to parse input file. Input file could potentially be corrupt!\n";
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
		std::clog << "Invalid command!\n";
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
		{
			auto ladderIt = inFile.ladders.end();
			inFile.GetLadderCount();
		}
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

		// Could not find area iterator.
		if (areaIt == inFile.areas.value().end())
		{
			std::clog << "Could not find area.\n";
			return false;
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
			}
			// It's an index. Just use the area count.
			else areaIt->ID = ++inFile.GetAreaCount();
			// Set blank coords.
			areaIt->nwCorner = {0.0f, 0.0f, 0.0f};
			areaIt->seCorner = {0.0f, 0.0f, 0.0f};
			// Fill custom data.
			areaIt->customDataSize = getCustomDataSize(inFile.GetMajorVersion(), inFile.GetMinorVersion());
			areaIt->customData.resize(areaIt->customDataSize, 0);
			break;
		// creating hide spot.
		case TargetType::HIDE_SPOT:
		{
			// Hide spot index *should* be defined if this case is running.
			assert(cmd.hideSpotID.has_value());
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
				if (!cmd.connectionIndex.has_value()) {
					std::clog << "Connection index not defined.\n";
					return false;
				}
				
				// Make connection and increment connection count.
				areaIt->connectionData.at((unsigned char)cmd.connectionIndex.value().first).second.emplace_back();
				areaIt->connectionData.at((unsigned char)cmd.connectionIndex.value().first).first++;
			}
			break;
		
		// Create encounter path.
		case TargetType::ENCOUNTER_PATH:
		{
			if (!cmd.encounterPathIndex.has_value()) {
				std::clog << "Encounter path index is undefined.\n";
				return false;
			}
			if (!areaIt->encounterPaths.has_value()) {
				std::clog << "Area has no encounter paths.\n";
				return false;
			}
			// Create encounter path.
			areaIt->encounterPaths.value().emplace(areaIt->encounterPaths.value().begin() + cmd.encounterPathIndex.value());
			// Increment count.
			areaIt->encounterPathCount++;
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
			auto ePathIt = areaIt->encounterPaths.value().begin() + cmd.encounterPathIndex.value();
			// Increment counter.
			ePathIt->spotCount++;
			// Create encounter spot.
			ePathIt->spotContainer.emplace(ePathIt->spotContainer.begin() + cmd.encounterSpotID.value());
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
	assert(cmd.areaLocParam.has_value());
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
		// Sort areas.
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
			std::clog << "Specified area index is out of range." << std::endl;
			std::filesystem::remove(TempPath);
			return false;
		}
		areaIt = inFile.areas.value().begin() + cmd.areaLocParam.value().second;
	}
	// Verify that the area iterator is actually valid.
	if (areaIt >= inFile.areas.value().end())
	{
		std::clog << "Could not find area.\n";
		return false;
	}
	// Start editing the target data.
	std::stringstream dataBuf; // To help with setting binary data.
	switch (cmd.target) {
		case TargetType::AREA:
			{
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
			// Are there hiding spots in the first place?
			if (areaIt->hideSpotData.first == 0)
			{
				std::clog << "Area has no hide spots.\n";
				return false;
			}
			// Should not happen.
			assert(cmd.hideSpotID.has_value());
			// Is it in range.
			if (std::clamp<unsigned char>(cmd.hideSpotID.value(), 0, areaIt->hideSpotData.first - 1) != cmd.hideSpotID.value()) {
				std::clog << "Hide spot index parameter is out of range.\n";
				std::filesystem::remove(TempPath);
				return false;
			}
			// New hide spot.
			auto hSpotIt = areaIt->hideSpotData.second.begin() + cmd.hideSpotID.value();
			// Validate iterator.
			assert(hSpotIt < areaIt->hideSpotData.second.end());
			
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
		// Editing encounter path.
		case TargetType::ENCOUNTER_PATH:
			{
				if (areaIt->encounterPathCount == 0) {
					std::clog << "Area has no encounter paths\n";
					return false;
				}
				// encounterPathIndex should already be defined.
				assert(cmd.encounterPathIndex.has_value());
				// Validate encounter path index.
				if (std::clamp(cmd.encounterPathIndex.value(), 0u, areaIt->encounterPathCount - 1) != cmd.encounterPathIndex.value())
				{
					std::clog << "Invalid encounter path index.\n";
					return false;
				}
				auto ePathIt = areaIt->encounterPaths.value().begin() + cmd.encounterPathIndex.value();
				// Iterator should never be invalid since the index will be kept in check.
				assert(ePathIt < areaIt->encounterPaths.value().end());
				// Parse.
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
						if (!ePathIt->WriteData(*dataBuf.rdbuf())) {
							std::clog << "Failed to store area data. Exiting.\n";
							return false;
						}
						// Make a change.
						dataBuf.seekp(std::stoull(cmd.actionParams.at(i), nullptr, 0u));
						unsigned char Val = std::stoi(cmd.actionParams.at(i + 1), nullptr, 0u);
						dataBuf.put(Val);
						// Read data from buffer.
						if (!ePathIt->ReadData(*dataBuf.rdbuf())) {
							std::clog << "Failed to get edited area data. Exiting.\n";
							return false;
						}
						// Iterate.
						i += 2;
					}
					else if (programConfig["map"]["nav_version"][std::to_string(inFile.GetMajorVersion())][cmd.actionParams.at(i)]) {
						const std::string propStr = programConfig["map"]["nav_version"][std::to_string(inFile.GetMajorVersion())][cmd.actionParams.at(i)].as_string()->value_exact<std::string>().value_or("");
						size_t arg_count = strEPathEditMethodMap.at(propStr)(std::ref(*ePathIt), cmd.actionParams.begin() + i + 1, cmd.actionParams.end());
						// Fail.
						if (arg_count == 0) {
							return false;
						}
						else i += arg_count + 1;
					}
					// Is it a string? Try to parse it as an alias.
					else if (programConfig["map"][cmd.actionParams.at(i)]) {
						const std::string propStr = programConfig["map"][cmd.actionParams.at(i)].as_string()->value_exact<std::string>().value_or("");
						size_t arg_count = strEPathEditMethodMap.at(propStr)(std::ref(*ePathIt), cmd.actionParams.begin() + i + 1, cmd.actionParams.end());
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
		// Editing encounter spot.
		case TargetType::ENCOUNTER_SPOT:
			{
				if (!cmd.encounterPathIndex.has_value())
				{
					std::clog << "Encounter path index is undefined." << std::endl;
					return false;
				}
				if (!cmd.encounterSpotID.has_value())
				{
					std::clog << "Encounter spot index is undefined." << std::endl;
					return false;
				}
				// Validate encounter path index.
				if (std::clamp(cmd.encounterPathIndex.value(), 0u, areaIt->encounterPathCount - 1) != cmd.encounterPathIndex.value())
				{
					std::clog << "Invalid encounter path index.\n";
					return false;
				}
				// Validate encounter spot index.
				if (std::clamp(cmd.encounterSpotID.value(), static_cast<unsigned char>(0), static_cast<unsigned char>(areaIt->encounterPaths.value().at(cmd.encounterPathIndex.value()).spotCount - 1)) != cmd.encounterSpotID.value())
				{
					std::clog << "Invalid encounter spot index.\n";
					return false;
				}
				
				auto eSpotIt = areaIt->encounterPaths.value().at(cmd.encounterPathIndex.value()).spotContainer.begin() + cmd.encounterSpotID.value();
				// Validate iterator.
				if (eSpotIt >= areaIt->encounterPaths.value().at(cmd.encounterPathIndex.value()).spotContainer.end()) {
					std::clog << "Bad iterator to encounter spot\n";
					return false;
				}
				// Parse arguments.
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
						dataBuf.seekg(0u);
						dataBuf.seekp(0u);
						// Write data to buffer.
						if (!eSpotIt->WriteData(*dataBuf.rdbuf())) {
							std::clog << "Failed to store area data. Exiting.\n";
							return false;
						}
						// Make a change.
						dataBuf.seekp(std::stoull(cmd.actionParams.at(i), nullptr, 0u));
						unsigned char Val = std::stoi(cmd.actionParams.at(i + 1), nullptr, 0u);
						dataBuf.put(Val);
						// Read data from buffer.
						if (!eSpotIt->ReadData(*dataBuf.rdbuf())) {
							std::clog << "Failed to get edited area data. Exiting.\n";
							return false;
						}
						// Clear buffer.
						dataBuf.clear();
						// Iterate.
						i += 2;
					}
					// NAV version-specific alias to property.
					else if (programConfig["map"]["nav_version"][std::to_string(inFile.GetMajorVersion())][cmd.actionParams.at(i)]) {
						const std::string propStr = programConfig["map"]["nav_version"][std::to_string(inFile.GetMajorVersion())][cmd.actionParams.at(i)].as_string()->value_exact<std::string>().value_or("");
						size_t arg_count = strESpotEditMethodMap.at(propStr)(std::ref(*eSpotIt), cmd.actionParams.begin() + i + 1, cmd.actionParams.end());
						// Fail.
						if (arg_count == 0) {
							return false;
						}
						else i += arg_count + 1;
					}
					// Is it a string? Try to parse it as an alias.
					else if (programConfig["map"][cmd.actionParams.at(i)]) {
						const std::string propStr = programConfig["map"][cmd.actionParams.at(i)].as_string()->value_exact<std::string>().value_or("");
						size_t arg_count = strESpotEditMethodMap.at(propStr)(std::ref(*eSpotIt), cmd.actionParams.begin() + i + 1, cmd.actionParams.end());
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
		// Edit Connection.
		case TargetType::CONNECTION:
			{
				if (!cmd.connectionIndex.has_value()) {
					std::clog << "Connection index parameter is undefined." << std::endl;
					return false;
				}
				// Clamp connection index.
				if (std::clamp(cmd.connectionIndex.value().second, 0u, areaIt->connectionData.at((unsigned char)cmd.connectionIndex.value().first).first) != cmd.connectionIndex.value().second) {
					std::clog << "Connection index is out of range." << std::endl;
					return false;
				}
				// Iterator to connection.
				auto connectionIt = areaIt->connectionData.at((unsigned char)cmd.connectionIndex.value().first).second.begin() + cmd.connectionIndex.value().second;
				// Validate Iterator.
				if (connectionIt >= areaIt->connectionData.at(static_cast<unsigned char>(cmd.connectionIndex.value().first)).second.end())
				{
					std::clog << "Cannot find connection." << std::endl;
					return false;
				}
				
				// Parse arguments.
				for (size_t i = 0; i < cmd.actionParams.size(); i++)
				{
					// 2 integer values, so it is offset, value.
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
						dataBuf.seekg(0u);
						dataBuf.seekp(0u);
						// Write data to buffer.
						if (!connectionIt->WriteData(*dataBuf.rdbuf())) {
							std::clog << "Failed to store area data. Exiting.\n";
							return false;
						}
						// Make a change.
						dataBuf.seekp(std::stoull(cmd.actionParams.at(i), nullptr, 0u));
						unsigned char Val = std::stoi(cmd.actionParams.at(i + 1), nullptr, 0u);
						dataBuf.put(Val);
						// Read data from buffer.
						if (!connectionIt->ReadData(*dataBuf.rdbuf())) {
							std::clog << "Failed to get edited area data. Exiting.\n";
							return false;
						}
						// Clear buffer.
						dataBuf.clear();
						// Iterate.
						i += 2;
					}
					// NAV version-specific alias to property.
					else if (programConfig["map"]["nav_version"][std::to_string(inFile.GetMajorVersion())][cmd.actionParams.at(i)]) {
						const std::string propStr = programConfig["map"]["nav_version"][std::to_string(inFile.GetMajorVersion())][cmd.actionParams.at(i)].as_string()->value_exact<std::string>().value_or("");
						size_t arg_count = strToConnectionEditMethodMap.at(propStr)(std::ref(*connectionIt), cmd.actionParams.begin() + i + 1, cmd.actionParams.end());
						// Fail.
						if (arg_count == 0) {
							return false;
						}
						else i += arg_count + 1;
					}
					// Is it a string? Try to parse it as an alias.
					else if (programConfig["map"][cmd.actionParams.at(i)]) {
						const std::string propStr = programConfig["map"][cmd.actionParams.at(i)].as_string()->value_exact<std::string>().value_or("");
						size_t arg_count = strToConnectionEditMethodMap.at(propStr)(std::ref(*connectionIt), cmd.actionParams.begin() + i + 1, cmd.actionParams.end());
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
		// Editing approach spot.
		case TargetType::APPROACH_SPOT:

			break;
		//
		default:
			std::clog << "Can't handle this type of data yet." << std::endl;
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
		std::clog << "Could not create temporary file buffer.\n";
		return false;
	}
	std::filebuf inBuf;
	if (!inBuf.open(inFile.GetFilePath(), std::ios_base::in | std::ios_base::out | std::ios_base::binary)) {
		std::clog << "fatal: Failed to open buffer.\n";
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
			// TODO:
			return true;
		break;
	default:
		break;
	}
	bool Valid = true;
	// Iterator to specified area.
	auto areaIt = inFile.areas.value().end();
	// This should be defined.
	assert(cmd.areaLocParam.has_value());
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
				std::clog << "Requested ID is outside the found ID range #"<<std::to_string(inFile.areas.value().front().ID)<<" to #"<<std::to_string(inFile.areas.value().back().ID)<<".\n";
				std::filesystem::remove(TempPath);
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
		if (std::clamp<unsigned int>(cmd.areaLocParam.value().second, 0u, inFile.areas.value().size()) != cmd.areaLocParam.value().second) {
			std::clog << "Specified area index is out of range.\n";
			std::filesystem::remove(TempPath);
			return false;
		}
		// Go to the area where the index points to.
		areaIt = inFile.areas.value().begin() + cmd.areaLocParam.value().second;
	}
	// Did we find the specified area.
	if (areaIt >= inFile.areas.value().end()) {
		std::clog << "Could not find area.\n";
		std::filesystem::remove(TempPath);
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
			// HideSpotID should hopefully be defined here.
			// The target wouldn't be a hide spot if the parameters did not target hide spots.
			assert(cmd.hideSpotID.has_value());
			// Clamp
			if (std::clamp<unsigned char>(cmd.hideSpotID.value(), 0, areaIt->hideSpotData.first) != cmd.hideSpotID.value()) {
				std::clog << "Hide spot index parameter is out of range.\n";
				Valid = false;
				break;
			}
			// Remove hide spot.
			if (areaIt->hideSpotData.second.erase(areaIt->hideSpotData.second.begin() + cmd.hideSpotID.value()) == areaIt->hideSpotData.second.cend()) {
				std::clog << "fatal: Could not locate.\n";
				return false;
			}
			areaIt->hideSpotData.first--;
		}

		case TargetType::CONNECTION:
			// connectionIndex should be defined if the program has managed to get here AND the command is actually targetting a connection.
			assert(cmd.connectionIndex.has_value());
			//
			if (areaIt->connectionData.at(static_cast<unsigned char>(cmd.connectionIndex.value().first)).first < 1)
			{
				std::clog << "No connections found for direction \'"<<static_cast<unsigned char>(cmd.connectionIndex.value().first)<<"\'\n";
				Valid = false;
				break;
			}
			// Validate.
			if (std::clamp(cmd.connectionIndex.value().second, 0u, areaIt->connectionData.at(static_cast<unsigned char>(cmd.connectionIndex.value().first)).first) != cmd.connectionIndex.value().second)
			{
				std::clog << "Connection index is out of range.\n";
				Valid = false;
				break;
			}
			// Remove connection.
			areaIt->connectionData.at(static_cast<unsigned char>(cmd.connectionIndex.value().first)).second.erase(areaIt->connectionData[(unsigned char)cmd.connectionIndex.value().first].second.begin() + cmd.connectionIndex.value().second);
			areaIt->connectionData.at(static_cast<unsigned char>(cmd.connectionIndex.value().first)).first--;
			break;
		default:
		break;
	}
	// Ensure the NAV data will be properly written.
	if (Valid) 
	{
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
		
	}
	std::filesystem::remove(TempPath);
	return Valid;
}

bool NavTool::ActionInfo(ToolCmd& cmd)
{
	// Command should not be invalid at this point.
	assert(cmd.target != TargetType::INVALID);

	if (cmd.target == TargetType::FILE)
	{
		inFile.OutputData(std::cout);
		return true;
	}

	// Get info of area.
	if (!cmd.areaLocParam.has_value()) {
		#ifndef NDEBUG
		std::clog << "NavTool::DispatchCommand(ToolCmd&): FATAL: Area not specified!\n";
		#endif
		return false;
	}
	// No Areas so we can't do anything.
	if (inFile.GetAreaCount() < 1)
	{
		std::clog << "Input file has no areas.\n";
		return false;
	}
	
	std::optional<std::streampos> areaLoc;
	auto areaIt = inFile.areas.value().end();
	// Is ID. Search for area.
	if (cmd.areaLocParam.value().first == true) {
		// Areas are already sorted.
		if (std::is_sorted(inFile.areas.value().begin(), inFile.areas.value().end(), [](const NavArea& lhs, const NavArea& rhs) {
			return lhs.ID < rhs.ID;
		}))
		{
			// Parameter Area ID is out of range.
			if (std::clamp(cmd.areaLocParam.value().second, inFile.areas.value().front().ID, inFile.areas.value().back().ID) != cmd.areaLocParam.value().second) {
				std::clog << "Could not find area ID #"<<std::to_string(cmd.areaLocParam.value().second)<<".\n";
				return false;
			}
		}
		// areas container is not already sorted,
		else
		{
			std::vector<IntID> AreaIDContainer;
			std::transform(inFile.areas.value().begin(), inFile.areas.value().end(), AreaIDContainer.begin(), [](const NavArea& area) {
				return area.ID;
			});
			// Sort with less than.
			std::sort(AreaIDContainer.begin(), AreaIDContainer.end());
			// Is ID in range?
			if (std::clamp(cmd.areaLocParam.value().second, AreaIDContainer.front(), AreaIDContainer.back()) != cmd.areaLocParam.value().second)
			{
				std::clog << "Could not find area ID #"<<std::to_string(cmd.areaLocParam.value().second)<<".\n";
				return false;
			}
		}
		// Area ID parameter is in possible ID range. Find.
		size_t sum = std::accumulate(inFile.areas.value().begin(), inFile.areas.value().end(), 0u, [](unsigned int lhs, const NavArea& rhs) -> size_t {
				return static_cast<size_t>(lhs + rhs.ID);
			});
		if (cmd.areaLocParam.value().second == sum / inFile.areas.value().size() )
			areaIt = std::find_if(inFile.areas.value().begin(), inFile.areas.value().end(), [&cmd](const NavArea& area) -> bool {
				return cmd.areaLocParam.value().second == area.ID;
			});
		else if (cmd.areaLocParam.value().second > sum / inFile.areas.value().size())
		{
			
			areaIt = std::find_if(inFile.areas.value().rbegin(), inFile.areas.value().rend(), [&cmd](const NavArea& area) {
				return cmd.areaLocParam.value().second == area.ID;
			}).base();
		}
		else areaIt = std::find_if(inFile.areas.value().begin(), inFile.areas.value().end(), [&cmd](const NavArea& area) {
			return cmd.areaLocParam.value().second == area.ID;
		});
	}
	// Area locator parameter should be interpeted as an index.
	else {
		// Clamp index and ensure the area index parameter is within the container range.
		if (std::clamp(cmd.areaLocParam.value().second, 0u, inFile.GetAreaCount() - 1) != cmd.areaLocParam.value().second) {
			std::clog << "Area index parameter is out of range.\n";
			return false;
		}
		areaIt = inFile.areas.value().begin() + cmd.areaLocParam.value().second;
	}
	std::filebuf buf;
	// File path verification is done in command line parsing.
	assert(buf.open(inFile.GetFilePath(), std::ios_base::in | std::ios_base::binary));
	
	// Output
	switch (cmd.target)
	{
	case TargetType::AREA:
		{
			areaIt->OutputData(std::cout);
			// Also output custom data if possible.
			std::cout << "\tCustom Data: ";
			switch (GetAsEngineVersion(inFile.GetMajorVersion(), inFile.GetMinorVersion()))
			{
			// Get TFAttributes flag.
			case TEAM_FORTRESS_2:
				{
					unsigned int TFAttributes;
					std::copy(areaIt->customData.begin(), areaIt->customData.begin() + 3, &TFAttributes);
					std::cout << "(Team Fortress 2)\n\t\tTFAttributes Flag: " << std::hex << TFAttributes << '\n';
				}
				break;
			
			case EngineVersion::UNKNOWN:
				std::cout << "(Unkown custom data.)\n";
				break;
			default:
				break;
			}
		}
		break;
	// Output hide spot data.
	case TargetType::HIDE_SPOT:
		{
			// hideSpotID should be defined in this case.
			assert(cmd.hideSpotID.has_value());
			// Empty hide spot data.
			if (areaIt->hideSpotData.first < 1) {
				std::clog << "Area #" << areaIt->ID << " has 0 hide spots.\n";
				return false;
			}
			// hideSpot ID is higher than hide spot count.
			if (std::clamp(cmd.hideSpotID.value(), static_cast<unsigned char>(0u), areaIt->hideSpotData.first) != cmd.hideSpotID.value()) {
				std::clog << "Hide spot index \'" << std::to_string(cmd.hideSpotID.value()) << "\' is out of range.\n";
				return false;
			}
			// Get hide spot data.
			auto hSpotIt = areaIt->hideSpotData.second.begin() + cmd.hideSpotID.value();
			// Output ID.
			std::cout << "Hide Spot #"<<std::to_string(cmd.hideSpotID.value())<<" of Area #"<<areaIt->ID << ":\n\tID: " << std::to_string(hSpotIt->ID) << '\n';
			std::cout << std::setw(4) << "\tPosition: ";
			// Output hide spot position
			std::copy(hSpotIt->position.cbegin(), hSpotIt->position.cend(), std::ostream_iterator<float>(std::cout, ", "));
			// Output hide spot attribute.
			std::cout << "\n\tAttributes: " << std::hex << std::showbase << hSpotIt->Attributes << '\n';
		}
		break;
	case TargetType::CONNECTION:
		{
			// Connection index should be defined.
			assert(cmd.connectionIndex.has_value());
			// Proper direction value should already be enforced by ParseCommandLine().
			assert(std::clamp(cmd.connectionIndex.value().first, Direction::North, Direction::West) == cmd.connectionIndex.value().first);
			// Is there a connection to begin with?
			if (areaIt->connectionData.at(static_cast<unsigned char>(cmd.connectionIndex.value().first)).first < 1)
			{
				std::clog << "Area has no direction \'" << static_cast<unsigned char>(cmd.connectionIndex.value().first) << "\' connections.\n";
				return false;
			}
			// Keep connection index in range.
			if (std::clamp(cmd.connectionIndex.value().second, 0u, areaIt->connectionData.at(static_cast<unsigned char>(cmd.connectionIndex.value().first)).first - 1) != cmd.connectionIndex.value().second) {
				std::cout << "Connection index \'"<< cmd.connectionIndex.value().second<< "\' is out of range!\n";
				return false;
			}
			// Get iterator.
			auto connectionIt = areaIt->connectionData.at(static_cast<unsigned char>(cmd.connectionIndex.value().first)).second.begin() + cmd.connectionIndex.value().second;
			// Iterator SHOULD be valid now.
			assert(connectionIt < areaIt->connectionData.at(static_cast<unsigned char>(cmd.connectionIndex.value().first)).second.end());
			// Output connection data.
			std::cout << directionToStr[cmd.connectionIndex.value().first] << " Connection " << cmd.connectionIndex.value().second
			<< ":\n\tTarget Area ID: " << connectionIt->TargetAreaID << '\n';
		}
		break;
	// Get encounter path/spot data.
	case TargetType::ENCOUNTER_PATH:
	case TargetType::ENCOUNTER_SPOT:
		{
			// Area has no encounter paths
			if (areaIt->encounterPathCount < 1) {
				std::cout << "Area #" << areaIt->ID << " has 0 encounter paths.\n";
				return false;
			}
			// encounterPathIndex should already be defined. Checks for lack of the encounter path locator parameter is done in ParseCommandLine()
			assert(cmd.encounterPathIndex.has_value());
			// Look for encounter path.
			if (cmd.target == TargetType::ENCOUNTER_PATH)
			{
				if (std::max(cmd.encounterPathIndex.value(), areaIt->encounterPathCount - 1) == cmd.encounterPathIndex.value()) {
					std::cerr << "Requested encounter path is out of range.\n";
					return false;
				}
				std::cout << "Encounter Path #" << cmd.encounterPathIndex.value() << " of area #" << areaIt->ID <<":\n";
				areaIt->encounterPaths.value().at(cmd.encounterPathIndex.value()).Output(std::cout);
				std::cout << '\n';
			}
			// Output encounter spot data.
			else if (cmd.target == TargetType::ENCOUNTER_SPOT) {
				// encounterSpotID should be defined.
				assert(cmd.encounterSpotID.has_value());
				// Keep in range.
				if (std::max(areaIt->encounterPathCount - 1, cmd.encounterPathIndex.value()) == cmd.encounterPathIndex.value()) {
					std::clog << "The encounter path ID of the requested encounter spot is out of range.\n";
					return false;
				}
				else {
					if (areaIt->encounterPaths.value().at(cmd.encounterPathIndex.value()).spotCount <= cmd.encounterSpotID) {
						std::cerr << "Requested Encounter Spot of Area #"<<areaIt->ID<<":Encounter Path #"<<cmd.encounterPathIndex.value()<<" is out of range.\n";
						return false;
					}
					std::cout << "Encounter Spot #" << cmd.encounterPathIndex.value();
					auto eSpotIt = areaIt->encounterPaths.value().at(cmd.encounterPathIndex.value()).spotContainer.begin() + cmd.encounterSpotID.value();
					std::cout << ":\n\tOrder ID: " << eSpotIt->OrderID
					<< "\n\tParametric Distance: " << eSpotIt->ParametricDistance
					<< '\n';
				}
			}
		}
		break;

	case TargetType::APPROACH_SPOT:
		{
			// Check version.
			if (inFile.GetMajorVersion() >= 15) {
				std::clog << "This file is too new to contain approach spots.\n";
				return false;
			}
			// Ensure the area has an approach spot.
			if (areaIt->approachSpotCount < 1)
			{
				std::clog << "Area has no approach spots.\n";
				return false;
			}
			// Approach Spot index should be defined.
			assert(cmd.approachSpotIndex.has_value());
			// Clamp approach spot.
			if (std::max(cmd.approachSpotIndex.value(), static_cast<unsigned char>(areaIt->approachSpotCount - 1)) != cmd.approachSpotIndex.value()) {
				std::clog << "Approach spot index is out of range.\n";
				return false;
			}
			//
			auto aSpotIt = areaIt->approachSpotData.value().begin() + cmd.approachSpotIndex.value();
			// Approach spot iterator should be valid here.
			assert(aSpotIt < areaIt->approachSpotData.value().end());
			// Output.
			std::cout << "Approach Spot ["<<std::to_string(cmd.approachSpotIndex.value())
			<<"]:\n\tApproach Type: " << std::to_string(aSpotIt->approachType)
			<< "\n\tApproach Method:" << std::to_string(aSpotIt->approachHow)
			<< "\n\tPrev ID: " << std::to_string(aSpotIt->approachPrevId)
			<< "\n\tDestination: " << std::to_string(aSpotIt->approachHereId)
			<< "\n\tNext ID: " << std::to_string(aSpotIt->approachNextId);
		}
		break;
	case TargetType::LADDER:
		// TODO:
		break;
	default:
		break;
	}
	return true;
}

int main(int argc, char **argv) {
	NavTool navApp(argc, argv);
	// Remove temporary files.
	std::filesystem::remove_all(std::filesystem::temp_directory_path().append("nav/"));
	// Flush.
	std::clog.flush();
	std::cout.flush();
	exit(EXIT_SUCCESS);
}
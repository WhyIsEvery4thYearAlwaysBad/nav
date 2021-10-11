#ifndef NAV_FILE_HPP
#define NAV_FILE_HPP
#include <fstream>
#include <string>
#include <deque>
#include <filesystem>
#include <span>
#include "nav_place.hpp"
#include "nav_area.hpp"
class NavFile {
	private:
		// Header info
		unsigned int MagicNumber, MajorVersion; 
		std::optional<unsigned int> MinorVersion /* Included after version 10. */, BSPSize /* Included after version 4 */; 
		std::optional<bool> isAnalyzed; // Introducted in major version 14.
		unsigned short PlaceCount;
		std::optional<bool> hasUnnamedAreas; // Doesn't exist prior to version 14.
		unsigned int AreaCount = 0u, LadderCount = 0u;
		
		std::filesystem::path FilePath; // File path.
		std::streampos AreaDataLoc = -1; // The starting location of area data.
		std::streampos LadderDataLoc = -1; // Ladder Data location.
		std::deque<std::string> PlaceNames;
	public:
		std::optional<std::vector<NavArea> > areas;
		NavFile();
		NavFile(const std::filesystem::path& path);
		~NavFile();
		
		// Accessor functions.
		const std::filesystem::path& GetFilePath();
		unsigned int& GetMagicNumber();
		unsigned int& GetMajorVersion(); 
		std::optional<unsigned int>& GetMinorVersion();
		std::optional<unsigned int>& GetBSPSize(); 
		unsigned short GetPlaceCount();
		std::optional<bool> IsAnalyzed(), GetHasUnnamedAreas();
		unsigned int& GetAreaCount();
		unsigned int GetLadderCount();
		const std::streampos& GetAreaDataLoc();
		const std::streampos& GetLadderDataLoc();

		/* Write header info.
		   Returns true on success, false on failure. */
		bool WriteData(std::streambuf& buf);
		/* Read header info.
		   Writes data to areaContainer.
		   Returns true on success, false on failure. */
		bool ReadData(std::streambuf& buf);
		// Output data.
		void OutputData(std::ostream& ostream);
		// Get game version.
		EngineVersion GetEngineVersion();

		bool IsValidFile();
		// Travel through data of an area.
		// Returns data length of an area (at specified file position).
		std::optional<size_t> TraverseNavAreaData(const std::streampos& pos);
		/// Travel through *custom* data of an area.
		// Returns data length of an area (at current file position).
		std::optional<size_t> GetAreaCustomDataSize();

		// Find an area with ID.
		// Retunrs the stream position if successful.
		std::optional<std::streampos> FindArea(const unsigned int& ID);
};
#endif
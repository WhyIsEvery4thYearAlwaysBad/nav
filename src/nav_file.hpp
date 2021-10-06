#ifndef NAV_FILE_HPP
#define NAV_FILE_HPP
#include <fstream>
#include <string>
#include <deque>
#include <filesystem>
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
		unsigned int AreaCount, LadderCount;
		
		std::filesystem::path FilePath; // File path.
		std::streampos AreaDataLoc = -1; // The starting location of area data.
		std::streampos LadderDataLoc = -1; // Ladder Data location.
	public:
		NavFile();
		NavFile(const std::string& path);
		~NavFile();
		
		// Accessor functions.
		const std::filesystem::path& GetFilePath();
		unsigned int GetMagicNumber();
		unsigned int GetMajorVersion(); 
		std::optional<unsigned int>& GetMinorVersion();
		std::optional<unsigned int>& GetBSPSize(); 
		unsigned short GetPlaceCount();
		std::optional<bool> IsAnalyzed(), GetHasUnnamedAreas();
		unsigned int GetAreaCount();
		unsigned int GetLadderCount();
		const std::streampos& GetAreaDataLoc();
		const std::streampos& GetLadderDataLoc();

		// Output data.
		void OutputData(std::ostream& ostream);
		// Get game version.
		EngineVersion GetEngineVersion();

		// Fill meta data from file stream.
		void FillMetaDataFromFile();
		bool IsValidFile();
		// Travel through data of an area.
		// Returns data length of an area (at specified file position).
		std::optional<size_t> TraverseNavAreaData(const std::streampos& pos);
		/// Travel through *custom* data of an area.
		// Returns data length of an area (at current file position).
		std::optional<size_t> GetAreaCustomDataSize();
		/// Get *custom* data of an area.
		// Returns data length of an area (at specified file position).
		NavCustomData GetNavAreaCustomData(const std::streampos& pos);

		// Find an area with ID.
		std::optional<std::streampos> FindArea(const unsigned int& ID);
};
#endif
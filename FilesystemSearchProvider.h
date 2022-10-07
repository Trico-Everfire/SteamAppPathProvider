//
// Authors: Trico Everfire, Scell555.
// Special thanks to Scell555 and the Momentum Mod team
// for providing a lot of the code, originally intended to
// be used in their 2013SDK fork, but reworked to be standalone.
//

#ifndef SAPP_FILESYSTEMSEARCHPROVIDER_H
#define SAPP_FILESYSTEMSEARCHPROVIDER_H

#include "KeyValue.h"

#include <filesystem>
#include <fmt/core.h>
#include <fstream>
#include <iostream>
#include <vector>

//We need to define on if we're running Posix.
//And the correct version of GNUC.
#if defined( __GNUC__ ) && !defined(_WIN32) && !defined(POSIX)
#if __GNUC__ < 4
#error "SAPP requires GCC 4.X or greater."
#endif
#define POSIX 1
#endif


// windows and POSIX have different
//  path separators, windows uses \
//whilst POSIX uses /

#ifdef _WIN32
#define CORRECT_PATH_SEPARATOR	 '\\'
#define INCORRECT_PATH_SEPARATOR '/'
#elif POSIX
#define CORRECT_PATH_SEPARATOR	 '/'
#define INCORRECT_PATH_SEPARATOR '\\'
#endif

namespace fs = std::filesystem;
typedef uint32_t AppId_t, uint32;

class ISteamSearchProvider
{
	// we have a class with the virtual const functions.
	// these functions are based on Valve's Steam API.
	// which stubbed the original implementation to prevent.
	// applications from using it.
	// this reimplements those functionality without relying
	// on the Steam API.
	//(this also means that this can be used without the need for
	// steam to be running, unlike the actual Steam API binaries.)
public:

	virtual ~ISteamSearchProvider() = default;

	virtual bool Available() const = 0;

	virtual bool BIsAppInstalled( AppId_t appID ) const = 0;

	virtual uint32 GetNumInstalledApps() const = 0;

	virtual uint32 GetInstalledApps( AppId_t *pvecAppID, uint32 unMaxAppIDs ) const = 0;

	virtual uint32 GetAppInstallDir( AppId_t appID, char *pchFolder, uint32 cchFolderBufferSize ) const = 0;
};

class CFileSystemSearchProvider final : public ISteamSearchProvider
{
	// File paths shouldn't be longer than 1048 characters.
	// You can bump this if it causes issues.
	uint32 MAX_PATH = 1048;

	// we use static (S) helper (H) functions to patch up
	// OS specific quirks related to path separators.
	static void S_HFixDoubleSlashes( char *pStr )
	{
		int len = strlen( pStr );

		// we check for double slashes. and patch them up.
		for ( int i = 1; i < len - 1; i++ )
		{
			if ( ( pStr[i] == CORRECT_PATH_SEPARATOR ) && ( pStr[i + 1] == CORRECT_PATH_SEPARATOR ) )
			{
				memmove( &pStr[i], &pStr[i + 1], len - i );
				--len;
			}
		}
	}

	static void S_HFixSlashes( char *pname, char separator = CORRECT_PATH_SEPARATOR )
	{
		// we iterate through the string until
		// we find either a correct path separator
		// or an incorrect path separator.
		// Why can't we find only incorrect file separators
		// and patch those up?

		while ( *pname )
		{
			if ( *pname == INCORRECT_PATH_SEPARATOR || *pname == CORRECT_PATH_SEPARATOR )
			{
				*pname = separator;
			}
			pname++;
		}
	}

	static void S_HAppendSlash( char *pStr, int strSize )
	{
		// we append a slash at the end of the string.
		int len = strlen( pStr );
		if ( len > 0 && !( pStr[len - 1] == INCORRECT_PATH_SEPARATOR || pStr[len - 1] == CORRECT_PATH_SEPARATOR ) )
		{
			if ( len + 1 >= strSize )
				// we throw an error if the length exceeds the size.
				throw( pStr );

			pStr[len] = CORRECT_PATH_SEPARATOR;
			pStr[len + 1] = 0;
		}
	}

	static auto S_HRead_File( std::string_view path ) -> std::string
	{
		// custom read file helper for getting file contents.
		constexpr auto read_size = std::size_t( 4096 );
		auto stream = std::ifstream( path.data() );
		stream.exceptions( std::ios_base::badbit );

		auto out = std::string();
		auto buf = std::string( read_size, '\0' );
		while ( stream.read( &buf[0], read_size ) )
		{
			out.append( buf, 0, stream.gcount() );
		}
		out.append( buf, 0, stream.gcount() );
		return out;
	}

public:
	CFileSystemSearchProvider()
	{
#ifdef WIN32
		// We get the location where Steam is installed.
		char steamLocation[MAX_PATH * 2];

		// we don't handle wine. But the logic for it is commented out.
		// If someone wishes to implement it, make a PR.
		const bool inWine = false; // IsRunningInWine();
		if ( !inWine )
		{
			HKEY steam;
			if ( RegOpenKeyExA( HKEY_LOCAL_MACHINE, R"(SOFTWARE\Valve\Steam)", 0, KEY_QUERY_VALUE | KEY_WOW64_32KEY, &steam ) != ERROR_SUCCESS )
				return;

			DWORD dwSize = sizeof( steamLocation );
			if ( RegQueryValueExA( steam, "InstallPath", nullptr, nullptr, (LPBYTE)steamLocation, &dwSize ) != ERROR_SUCCESS )
				return;

			RegCloseKey( steam );
		}
		// Unimplemented wine logic.
		// Make sure you implement a wine detector
		// if you wish to use it. (read comment at the top.)
//		else
//		{
//			namespace fs = std::filesystem;
//			const char *pHome = l_getenv( "HOME" );
//			if ( !pHome )
//				return;
//			sprintf( steamLocation, "Z:%s/.steam/steam", pHome );
//			S_HFixSlashes( steamLocation );
//
//			std::error_code c;
//			if ( !fs::exists( steamLocation, c ) )
//			{
//				steamLocation[0] = '\0';
//				fs::path d{ "cwd\\steamclient64.dll" };
//				for ( const auto &e : fs::directory_iterator( R"(Z:\proc\)" ) )
//				{
//					if ( fs::exists( e / d, c ) )
//					{
//						c.clear();
//						const auto s = fs::read_symlink( e.path() / "cwd", c );
//						if ( c )
//							continue;
//						V_strcpy_safe( steamLocation, s.string().c_str() );
//						break;
//					}
//				}
//				if ( steamLocation[0] == '\0' )
//					return;
//			}
//		}
#else
		// We get the location where Steam is installed.
		char steamLocation[MAX_PATH * 2];
		{
			const char *pHome = getenv( "HOME" );
			sprintf( steamLocation, "%s/.steam/steam", pHome );
			S_HFixDoubleSlashes( steamLocation );
		}

		// If we cannot find the location/the location does not exist.
		// We'll search for the actual location, which takes longer than
		// assuming the default install location, which is usually correct.
		std::error_code c;
		if ( !fs::exists( fs::path( steamLocation ), c ) )
		{
			steamLocation[0] = '\0';
			fs::path d { "cwd/steamclient64.dll" };
			for ( const auto &e : fs::directory_iterator( "/proc/" ) )
			{
				if ( fs::exists( e / d, c ) )
				{
					c.clear();
					const auto s = fs::read_symlink( e.path() / "cwd", c );
					if ( c )
						continue;
					strcpy( steamLocation, s.string().c_str() );
					break;
				}
			}
			if ( steamLocation[0] == '\0' )
				return;
		}
#endif

		// after getting the steam location, we need the libraryfolders.vdf
		// they hold the information on what drives are used to store games
		// you've installed.
		strcat( steamLocation, R"(\steamapps\libraryfolders.vdf)" );

		// we fix any mess-ups with folder paths, as they are OS specific and
		// can break if not accounted for.
		S_HFixDoubleSlashes( steamLocation );
		S_HFixSlashes( steamLocation );

		// We now read libraryfolders.vdf Which is in KV1 file format.
		// SpeedyKeyV will handle all the heavy lifting and makes it easy
		// for us to read this file.
		std::string file = S_HRead_File( steamLocation );
		KeyValueRoot libFolder = KeyValueRoot( file.c_str() );

		// first we check for errors, any parsing errors and we won't be able
		// to use the data in the file and are forced to return.
		if ( !libFolder.IsValid() )
			return;

		// We solidify the file, as we only need to read from it.
		// This makes it faster, and reduces memory usage.
		libFolder.Solidify();

		// We store the drive paths in a vector.
		std::vector<char *> libFolders;

		// We then check for the "path" variable. This variable tells us
		// the location our games are installed per drive.
		for ( int i = 0; i < libFolder["libraryfolders"].ChildCount(); i++ )
		{
			KeyValue &folder = libFolder["libraryfolders"].Get( i );
			const char *name = folder.Key().string;
			if ( !strcmp( name, "TimeNextStatsReport" ) || !strcmp( name, "ContentStatsID" ) )
				continue;
			// if path is found, we push the string value into libFolders.
			// if the user happens to use the old libraryfolders.vdf format
			//(which happens to not have path) we push the entire kv result
			// into libFolders because that's the only thing containing inside.
			//(this is untested with the old format, can someone verify?)
			if ( char *f = folder["path"].Value().string )
				libFolders.push_back( f );
			else if ( auto p = folder.ToString() )
				libFolders.push_back( p );
		}

		// we add /steamapps/ and fix any mistakes if there ever were any.
		for ( auto &folder : libFolders )
		{
			S_HAppendSlash( folder, MAX_PATH - strlen( folder ) - 1 );
			strncat( folder, "steamapps", MAX_PATH - strlen( folder ) - 10 );
			S_HFixSlashes( folder );
		}

// more unimplemented wine logic.
// read the comment at the top of the file
// if you wish to implement wine compatibility.
#ifdef WIN32
		if ( inWine )
		{
			char z[MAX_PATH];
			for ( int i = 1; i < libFolders.size(); ++i ) // skip main steam install dir
			{
				// this is probably wrong. but we don't support
				// wine anyway.
				auto &folder = libFolders[i];
				strncpy( z, "Z:", strlen( folder ) );
				strncat( z, folder, strlen( folder ) );
				S_HFixSlashes( z );
				strncpy( folder, z, strlen( z ) );
			}
		}
#endif

		// We now get the actual installed games and their Steam App ID (AppId_t)
		for ( const auto &folder : libFolders )
		{
			// folder becomes unavailable because of the directory iterator.
			// so we copy it into a temporary string buffer.
			char pathR[MAX_PATH];
			memcpy( pathR, folder, MAX_PATH );
			// we iterate through the entire directory in search of app manifest files. which hold the app id.
			// we also store the path to this file.
			for ( auto const &dir_entry : fs::directory_iterator { folder } )
			{
				// because folder has become unavailable.
				// and PathR would get written to every time.
				// we copy PathR into path to store instead.
				//(this is probably a terrible way of doing it.)
				char *path = new char[MAX_PATH];
				memcpy( path, pathR, MAX_PATH );
				auto strPath = dir_entry.path().string();
				auto indd = strPath.find( "appmanifest_" );
				auto indd2 = strPath.rfind( ".acf" );
				if ( ( indd <= strPath.length() ) && ( indd2 <= strPath.length() ) )
				{
					games.push_back( Game { path, static_cast<AppId_t>( atoi(
													  strPath.substr( ( indd + 12 ), indd2 - ( indd + 12 ) ).c_str() ) ) } );
				}
			}
		}
		// we then sort the games.
		// Is this even needed?
		// We don't really have a logical order when fetching.
		std::sort( games.begin(), games.end(), []( const Game &a, const Game &b )
				   {
					   return int( a.appid ) < int( b.appid );
				   } );
	}

	bool Available() const override
	{
		// If the games vector is empty,
		// something went wrong during
		// the filling process and therefore
		// returns empty.
		// This will also happen on a fresh steam
		// install where no games have been installed.
		return !games.empty();
	}

	bool BIsSourceGame( AppId_t appID )
	{
		//We get the game install path and check it recursively until
		//we find a gameinfo.txt file or we'll return false if it isn't
		//there.
		char dirPath[MAX_PATH];
		GetAppInstallDir(appID, dirPath, MAX_PATH);
		for ( auto const &dir_entry : fs::recursive_directory_iterator { fs::path(dirPath) } )
		{
			if(!dir_entry.is_directory() &&  dir_entry.path().string().find("gameinfo.txt") != std::string::npos )
				return true;
		}
		return false;
	}

	bool BIsAppInstalled( AppId_t appID ) const override
	{
		// we check if the game exists and will return
		// true or false depending on if it does.
		const auto game = std::find_if( games.begin(), games.end(), [appID]( const Game &g ){ return g.appid == appID; } );
		return game != games.end();
	}

	uint32 GetAppInstallDir( AppId_t appID, char *pchFolder, uint32 cchFolderBufferSize ) const override
	{
		// we check if the game exists and will return
		// if it doesn't. We can't call BIsAppInstalled because
		// we need the iterator result.
		const auto game = std::find_if( games.begin(), games.end(), [appID]( const Game &g )
										{
											return g.appid == appID;
										} );
		if ( games.end() == game )
			return 0;

		// we then format the path,
		// correct file separator,
		//  and appID into the correct places to get the app manifest.
		auto formattedName = fmt::format( "{0}{1}appmanifest_{2}.acf", game->library, CORRECT_PATH_SEPARATOR, appID );
		auto fileContents = S_HRead_File( formattedName );

		// we then parse it with SpeedyKeyV.
		KeyValueRoot file = KeyValueRoot( fileContents.c_str() );

		// first we check for errors, any parsing errors and we won't be able
		// to use the data in the file and are forced to return.
		if ( !file.IsValid() )
			return 0;

		// we solidify the file, as we only need to read from it.
		// this makes it faster, and reduces memory usage.
		file.Solidify();

		// we get the install directory.
		// Append /common/ to it.
		// And then append the install directory to it afterwards.
		auto fileValue = file["AppState"]["installdir"].Value().string;
		strcpy( pchFolder, game->library );
		S_HAppendSlash( pchFolder, cchFolderBufferSize - 1 );
		strncat( pchFolder, "common", cchFolderBufferSize - 7 );
		S_HAppendSlash( pchFolder, cchFolderBufferSize - 8 );
		strncat( pchFolder, fileValue, cchFolderBufferSize - strlen( game->library ) - 8 );
		S_HAppendSlash( pchFolder, cchFolderBufferSize - strlen( game->library ) - 9 );
		S_HFixSlashes( pchFolder );
		return 1; // fix if need len
	}

	uint32 GetNumInstalledApps() const override
	{
		// we return the amount of files inside, which equals to the amount of games installed.
		return games.size();
	}

	uint32 GetInstalledApps( AppId_t *pvecAppID, uint32 unMaxAppIDs ) const override
	{
		// this fills an array of appids.
		// we will fill it with all appids stored in the games vector.
		// unless unMaxAppIDs is smaller than the games vector.
		// this is terrible. But it's the way the valve function
		// works so we'll do it as well.
		for ( int i = 0; i < unMaxAppIDs && i < games.size(); i++ )
			pvecAppID[i] = games.at( i ).appid;
		return unMaxAppIDs <= games.size() ? unMaxAppIDs : games.size();
	}

	// custom destructor to dispose of the memory we use.
	~CFileSystemSearchProvider()
	{
		// we own the char array, and it will remain in memory until destroyed.
		std::destroy( games.begin(), games.end() );
	}

private:
	// custom struct to store the appids and library paths.
	// as well as the games vector.
	struct Game
	{
		const char *library;
		AppId_t appid;
	};
	std::vector<Game> games;
};

#endif // SAPP_FILESYSTEMSEARCHPROVIDER_H

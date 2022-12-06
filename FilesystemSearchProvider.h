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

// We need to define on if we're running Posix.
// And the correct version of GNUC.
#if defined( __GNUC__ ) && !defined( _WIN32 ) && !defined( POSIX )
#if __GNUC__ < 4
#error "SAPP requires GCC 4.X or greater."
#endif
#define POSIX 1
#endif

// windows and POSIX have different
//           path separators, windows uses \
//whilst POSIX uses /

#ifdef _WIN32
#include <windows.h>
#define CORRECT_PATH_SEPARATOR	 '\\'
#define INCORRECT_PATH_SEPARATOR '/'
#elif POSIX
#define CORRECT_PATH_SEPARATOR	 '/'
#define INCORRECT_PATH_SEPARATOR '\\'
#endif

#ifdef _WIN32
#define CORRECT_PATH_SEPARATOR_S   "\\"
#define INCORRECT_PATH_SEPARATOR_S "/"
#elif POSIX
#define CORRECT_PATH_SEPARATOR_S   "/"
#define INCORRECT_PATH_SEPARATOR_S "\\"
#endif

namespace fs = std::filesystem;
typedef uint32_t AppId_t, uint32;

// we implement our own strlcpy.
class ISteamSearchProvider
{
protected:
	auto sapp_strlcpy( char *dst, char *src, size_t n )
	{
		int len = strlen( src );
		assert( len < n );
		auto byte = memcpy( dst, src, n );
		dst[len] = '\0';
		return byte;
	}

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

	// Game class (thanks Scell555) retains it's own memory
	// and deletes it when it's destructor is called.
	class Game
	{
	public:
		~Game()
		{
			free( gameName );
			free( library );
			free( installDir );
			free( icon );
		}

		Game( const Game &game )
		{
			gameName = strdup( game.gameName );
			library = strdup( game.library );
			installDir = strdup( game.installDir );
			icon = strdup( game.icon );
			appid = game.appid;
		}

		Game( Game &&game )
		{
			gameName = std::exchange( game.gameName, nullptr );
			library = std::exchange( game.library, nullptr );
			installDir = std::exchange( game.installDir, nullptr );
			icon = std::exchange( game.icon, nullptr );
			appid = std::exchange( game.appid, 0 );
		}

		Game &operator=( const Game &game )
		{
			if ( &game == this )
				return *this;
			gameName = strdup( game.gameName );
			library = strdup( game.library );
			installDir = strdup( game.installDir );
			icon = strdup( game.icon );
			appid = game.appid;
			return *this;
		}

		Game &operator=( Game &&game )
		{
			if ( &game == this )
				return *this;
			std::swap( gameName, game.gameName );
			std::swap( library, game.library );
			std::swap( installDir, game.installDir );
			std::swap( icon, game.icon );
			std::swap( appid, game.appid );
			return *this;
		}

		Game( const char *vGameName, const char *vLibrary, const char *vInstallDir, const char *vIcon, AppId_t vAppid )
		{
			gameName = strdup( vGameName );
			library = strdup( vLibrary );
			installDir = strdup( vInstallDir );
			icon = strdup( vIcon );
			appid = vAppid;
		}

		char *gameName;
		char *library;
		char *installDir;
		char *icon;
		AppId_t appid;
	};

	virtual bool Available() const = 0;

	virtual bool BIsAppInstalled( AppId_t appID ) const = 0;

	virtual uint32 GetNumInstalledApps() const = 0;

	virtual bool BIsSourceGame( AppId_t appID ) const = 0;

	virtual uint32 GetInstalledApps( AppId_t *pvecAppID, uint32 unMaxAppIDs ) const = 0;

	virtual AppId_t *GetInstalledAppsEX() const = 0;

	virtual uint32 GetAppInstallDir( AppId_t appID, char *pchFolder, uint32 cchFolderBufferSize ) const = 0;

	virtual Game *GetAppInstallDirEX( AppId_t appID ) const = 0;
};

class CFileSystemSearchProvider final : public ISteamSearchProvider
{
	// File paths shouldn't be longer than 1048 characters.
	// You can bump this if it causes issues.
	uint32 SAPP_MAX_PATH = 1048;
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

	static void S_HFixDoubleSlashes( std::string &str )
	{
		int len = str.length();

		// we check for double slashes. and patch them up.
		for ( int i = 1; i < len - 1; i++ )
		{
			if ( ( str[i] == CORRECT_PATH_SEPARATOR ) && ( str[i + 1] == CORRECT_PATH_SEPARATOR ) )
			{
				memmove( &str[i], &str[i + 1], len - i );
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

	static void S_HFixSlashes( std::string &pname, char separator = CORRECT_PATH_SEPARATOR )
	{
		// we iterate through the string until
		// we find either a correct path separator
		// or an incorrect path separator.
		// Why can't we find only incorrect file separators
		// and patch those up?

		for ( char &rChar : pname )
		{
			if ( rChar == INCORRECT_PATH_SEPARATOR || rChar == CORRECT_PATH_SEPARATOR )
			{
				rChar = separator;
			}
		}
	}

	static void S_HAppendSlash( char *pStr, int strSize )
	{
		// we append a slash at the end of the string.
		int len = strlen( pStr );
		if ( len > 0 && !( pStr[len - 1] == INCORRECT_PATH_SEPARATOR || pStr[len - 1] == CORRECT_PATH_SEPARATOR ) )
		{
			assert( len + 1 < strSize );

			pStr[len] = CORRECT_PATH_SEPARATOR;
			pStr[len + 1] = 0;
		}
	}

	static void S_HAppendSlash( std::string &s )
	{
		s.append( CORRECT_PATH_SEPARATOR_S );
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
#ifdef WINE
		else
		{
			namespace fs = std::filesystem;
			const char *pHome = l_getenv( "HOME" );
			if ( !pHome )
				return;
			sprintf( steamLocation, "Z:%s/.steam/steam", pHome );
			S_HFixSlashes( steamLocation );

			std::error_code c;
			if ( !fs::exists( steamLocation, c ) )
			{
				steamLocation[0] = '\0';
				fs::path d { "cwd\\steamclient64.dll" };
				for ( const auto &e : fs::directory_iterator( R"(Z:\proc\)" ) )
				{
					if ( fs::exists( e / d, c ) )
					{
						c.clear();
						const auto s = fs::read_symlink( e.path() / "cwd", c );
						if ( c )
							continue;
						V_strcpy_safe( steamLocation, s.string().c_str() );
						break;
					}
				}
				if ( steamLocation[0] == '\0' )
					return;
			}
		}
#endif
#else
		// We get the location where Steam is installed.
		char steamLocation[SAPP_MAX_PATH * 2];
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

		// We get the location of the library cache location.
		std::string librarycache;
		librarycache.append( steamLocation );
		librarycache.append( CORRECT_PATH_SEPARATOR_S "appcache" CORRECT_PATH_SEPARATOR_S "librarycache" CORRECT_PATH_SEPARATOR_S );
		//		strcpy( librarycache, steamLocation );
		// strcat( librarycache, CORRECT_PATH_SEPARATOR_S "appcache" CORRECT_PATH_SEPARATOR_S "librarycache" CORRECT_PATH_SEPARATOR_S "\0" );

		// we fix any mess-ups with folder paths, as they are OS specific and
		// can break if not accounted for.
		S_HFixDoubleSlashes( librarycache );
		S_HFixSlashes( librarycache );

		// after getting the steam location, we need the libraryfolders.vdf
		// they hold the information on what drives are used to store games
		// you've installed.
		strcat( steamLocation, R"(\steamapps\libraryfolders.vdf)"
							   "\0" );

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

		// We then check for the "path" variable. This variable tells us
		// the location our games are installed per drive.
		auto &libKeyValue = libFolder["libraryfolders"];
		for ( int i = 0; i < libKeyValue.ChildCount(); i++ )
		{
			KeyValue &folder = libKeyValue.Get( i );
			const char *name = folder.Key().string;
			if ( !strcmp( name, "TimeNextStatsReport" ) || !strcmp( name, "ContentStatsID" ) )
				continue;
			// We get the path to the drive locations where
			// steam games are installed.
			// There are 2 formats for this.
			// The old format, which is a string.
			// And the new format, which is a KV object,
			// to where the "path" key holds the path.
			auto pathValue = folder["path"].Value();
			if ( !pathValue.string )
				pathValue = folder.Value();
			if ( !pathValue.string )
				continue;

			// we add /steamapps and fix any mistakes if there ever were any.
			std::string pathString = std::string( pathValue.string );
			pathString.append( CORRECT_PATH_SEPARATOR_S "steamapps" );
			S_HFixSlashes( pathString );

			// more unimplemented wine logic.
			// read the comment at the top of the file
			// if you wish to implement wine compatibility.
#ifdef WINE
			if ( inWine )
			{
				// this is invalid.but we don't support wine anyway. char z[MAX_PATH];
				for ( int i = 1; i < libFolders.size(); ++i ) // skip main steam install dir
				{
					auto &folder = libFolders[i];
					strncpy( z, "Z:", strlen( MAX_PATH ) );
					strncat( z, folder, strlen( MAX_PATH ) );
					S_HFixSlashes( z );
					strncpy( folder, z, strlen( z ) );
				}
			}
#endif

			// When a drive is removed but previously mounted on Steam
			// it'll create a invalid drive mount, we check if this is
			// the case by checking if the path leads anywhere.
			if ( !fs::exists( ( pathString ) ) )
				continue;

			// We iterate through the entire directory in search of app manifest files. which hold the app id.
			// We also store the path to this file.
			// We now get the actual installed games and their Steam App ID (AppId_t)
			for ( auto const &dir_entry : fs::directory_iterator( ( pathString ), fs::directory_options::skip_permission_denied ) )
			{
				// pathString falls out of scope the moment the constructor ends.
				// So we put it onto the heap and store that in Games.
				// This'll later be destroyed by the Game class's destructor.
				auto strPath = dir_entry.path().string();

				if ( !fs::exists( ( strPath ) ) )
					continue;

				auto indd = strPath.find( "appmanifest_" );
				auto indd2 = strPath.rfind( ".acf" );
				if ( ( indd <= strPath.length() ) && ( indd2 <= strPath.length() ) )
				{
					// We read the app manifest and get 4 components:
					// The game's name.
					// The game's appid.
					// The game's install directory.
					// Additionally, we store the library path to where
					// the game is housed, as well as the path to
					// the game's icon, which is stored in the
					// app cache used by steam.
					auto pathFile = S_HRead_File( std::string( strPath ) );
					KeyValueRoot appManifest = KeyValueRoot( pathFile.c_str() );

					if ( !appManifest.IsValid() )
						return;

					KeyValue &appState = appManifest["AppState"];

					if ( appState["name"].Value().string == nullptr )
						continue;

					// We don't own any of these, but Game will copy them
					// and claim ownership of the copy until destroyed.
					auto keyName = appState["name"].Value().string;
					auto keyInstallDir = appState["installdir"].Value().string;
					auto appid = appState["appid"].Value().string;

					std::string icon( librarycache );
					icon.append( appid );
					icon.append( "_icon.jpg" );

					games.push_back( Game { keyName, pathString.c_str(), keyInstallDir, icon.c_str(), static_cast<AppId_t>( atoi( appid ) ) } );
				}
			}
		}

		// We then sort the games.
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

	bool BIsSourceGame( AppId_t appID ) const override
	{
		// We get the game install path and check it recursively until
		// we find a gameinfo.txt file or we'll return false if it isn't
		// there.

		if ( !BIsAppInstalled( appID ) )
			return false;

		char *dirPath = new char[SAPP_MAX_PATH];
		GetAppInstallDir( appID, dirPath, SAPP_MAX_PATH );

		for ( auto const &dir_entry : std::filesystem::recursive_directory_iterator { std::string( dirPath ), std::filesystem::directory_options::skip_permission_denied } )
		{
			if ( !dir_entry.is_directory() && dir_entry.path().string().find( "gameinfo.txt" ) != std::string::npos )
			{
				delete[] dirPath;
				return true;
			}
		}
		delete[] dirPath;
		return false;
	}

	bool BIsAppInstalled( AppId_t appID ) const override
	{
		// we check if the game exists and will return
		// true or false depending on if it does.
		const auto game = std::find_if( games.begin(), games.end(), [appID]( const Game &g )
										{
											return g.appid == appID;
										} );
		return game != games.end();
	}

	uint32 GetAppInstallDir( AppId_t appID, char *pchFolder, uint32 cchFolderBufferSize ) const override
	{
		// We check if the game exists and will return
		// if it doesn't. We can't call BIsAppInstalled because
		// we need the iterator result.
		const auto game = std::find_if( games.begin(), games.end(), [appID]( const Game &g )
										{
											return g.appid == appID;
										} );
		if ( games.end() == game )
			return false;

		strncpy( pchFolder, game->library, cchFolderBufferSize );
		strncat( pchFolder, CORRECT_PATH_SEPARATOR_S "common" CORRECT_PATH_SEPARATOR_S, cchFolderBufferSize - 8 );
		strncat( pchFolder, game->installDir, cchFolderBufferSize - strlen( game->installDir ) - 8 );
		S_HAppendSlash( pchFolder, cchFolderBufferSize - strlen( game->installDir ) - 9 );
		S_HFixSlashes( pchFolder );
		return true; // fix if need len
	}

	Game *GetAppInstallDirEX( AppId_t appID ) const override
	{
		// We check if the game exists and will return
		// if it doesn't. We can't call BIsAppInstalled because
		// we need the iterator result.
		const auto game = std::find_if( games.begin(), games.end(), [appID]( const Game &g )
										{
											return g.appid == appID;
										} );
		if ( games.end() == game )
			return nullptr;

		auto pgame = new Game( *game );
		return pgame; // fix if need len
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
		// Use GetInstalledAppsEX for more modern implementation.
		for ( int i = 0; i < unMaxAppIDs && i < games.size(); i++ )
			pvecAppID[i] = games.at( i ).appid;
		return unMaxAppIDs <= games.size() ? unMaxAppIDs : games.size();
	}

	AppId_t *GetInstalledAppsEX() const override
	{
		// This returns a new array of appids.
		// The user owns this array and it's the length of the games vector.
		// you can use GetNumInstalledApps to get the length.
		AppId_t *pvecAppID = new AppId_t[GetNumInstalledApps()];
		for ( int i = 0; i < games.size(); i++ )
			pvecAppID[i] = games.at( i ).appid;
		return pvecAppID;
	}

private:
	// custom struct to store the appids and library paths.
	// as well as the games vector.
	std::vector<Game> games;
};

#endif // SAPP_FILESYSTEMSEARCHPROVIDER_H


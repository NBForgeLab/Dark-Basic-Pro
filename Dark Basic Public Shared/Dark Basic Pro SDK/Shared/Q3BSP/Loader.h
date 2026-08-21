
#ifndef _FILE_LOADER
#define _FILE_LOADER

#include <windows.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

inline bool strcpy2 ( char** dest, std::string_view src )
{
	if ( !dest )
		return false;

	delete [ ] *dest;

	auto buffer = std::make_unique_for_overwrite <char[]> ( src.size ( ) + 1 );
	std::memcpy ( buffer.get ( ), src.data ( ), src.size ( ) );
	buffer [ src.size ( ) ] = '\0';

	*dest = buffer.release ( );

	return true;
}

inline std::string ToLowerAscii ( std::string_view text )
{
	std::string out ( text );

	for ( char& c : out )
		c = static_cast <char> ( std::tolower ( static_cast <unsigned char> ( c ) ) );

	return out;
}

[[nodiscard]] inline bool EndsWithIgnoreCase ( std::string_view text, std::string_view suffix )
{
	return text.size ( ) >= suffix.size ( ) &&
		std::equal ( suffix.rbegin ( ), suffix.rend ( ), text.rbegin ( ), [ ] ( char a, char b )
	{
		return std::tolower ( static_cast <unsigned char> ( a ) ) == std::tolower ( static_cast <unsigned char> ( b ) );
	} );
}

bool IsDirectoryPath ( std::string_view path );

struct _externfiles
{
	int extmap;
	char extmap_name [ 256 ];
};

extern _externfiles externfiles;

#define EXT_QUAKE12 0x1
#define EXT_QUAKE3  0x2

#define SAFE_DELETE( p )       do { delete ( p );       ( p ) = nullptr; } while ( 0 )
#define SAFE_RELEASE( p )      do { if ( p ) ( p )->Release ( ); ( p ) = nullptr; } while ( 0 )
#define SAFE_DELETE_ARRAY( p ) do { delete [ ] ( p );   ( p ) = nullptr; } while ( 0 )

struct files_found
{
	struct Entry
	{
		std::string filename;
		std::string path;
		std::string fullname;
	};

	std::vector<Entry> entries;

	void Clear ( ) { entries.clear ( ); }
	void Release ( ) { Clear ( ); }

	[[nodiscard]] Entry& Add ( )
	{
		return entries.emplace_back ( );
	}

	bool AddFile ( std::string_view fullname );
	bool AddFile ( std::string_view fullname, std::string_view filename, std::string_view path );

	[[nodiscard]] bool ContainsFileCaseInsensitive ( std::string_view filename ) const;
};

class file_loader
{
	public:
		class Load
		{
			public:
				static bool Direct   ( std::string_view filename, byte** data, int* length );
				static bool From_ZIP ( std::string_view zipname, std::string_view filename, byte** data, int* length );
				static bool From_PK3 ( std::string_view pk3name, std::string_view filename, byte** data, int* length );
				static bool From_PAK ( std::string_view pakname, std::string_view filename, byte** data, int* length );
				static bool From_WAD ( std::string_view wadname, std::string_view filename, byte** data, int* length );
		};

		class Find
		{
			public:
				static bool File         ( std::string_view filename, std::string_view path, std::string* fullname );
				static bool Files        ( std::string_view wildcard, std::string_view path, files_found* files );
				static bool Files_in_PK3 ( std::string_view pk3name, std::string_view extension, files_found* files );
		};

		class GetLength
		{
			public:
				static bool File ( std::string_view filename, int* length );
				static int  File ( FILE* f );
		};

		class Q3A
		{
			public:
				static bool Add_PK3 ( std::string_view filename );
		};

		class Q2
		{
			public:
				static bool Add_WAD ( std::string_view filename );
		};
};

void SplitPath ( std::string_view full, std::string* filename, std::string* path );

extern files_found files_cache;
extern files_found Q3A_Resources;
extern files_found Q2_Resources;

#endif
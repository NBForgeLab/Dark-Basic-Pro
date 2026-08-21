#include "loader.h"
#include "unzip.h"

#include <filesystem>
#include <fstream>
#include <algorithm>
#include <unordered_map>

namespace fs = std::filesystem;

extern char* unzip_file ( const char* p, int& size );

files_found files_cache;
files_found Q3A_Resources;
files_found Q2_Resources;

_externfiles externfiles;

namespace
{

[[nodiscard]] bool ICaseEqual ( std::string_view a, std::string_view b )
{
	if ( a.size ( ) != b.size ( ) )
		return false;

	for ( std::size_t i = 0; i < a.size ( ); ++i )
	{
		if ( std::tolower ( static_cast <unsigned char> ( a [ i ] ) ) != std::tolower ( static_cast <unsigned char> ( b [ i ] ) ) )
			return false;
	}

	return true;
}

[[nodiscard]] bool ICaseSuffix ( std::string_view text, std::string_view suffix )
{
	return text.size ( ) >= suffix.size ( ) && ICaseEqual ( text.substr ( text.size ( ) - suffix.size ( ) ), suffix );
}

[[nodiscard]] int ICaseCompare ( std::string_view a, std::string_view b )
{
	const std::size_t count = ( std::min ) ( a.size ( ), b.size ( ) );

	for ( std::size_t i = 0; i < count; ++i )
	{
		const int ca = std::tolower ( static_cast <unsigned char> ( a [ i ] ) );
		const int cb = std::tolower ( static_cast <unsigned char> ( b [ i ] ) );

		if ( ca != cb )
			return ca < cb ? -1 : 1;
	}

	if ( a.size ( ) == b.size ( ) )
		return 0;

	return a.size ( ) < b.size ( ) ? -1 : 1;
}

[[nodiscard]] bool WildcardMatch ( std::string_view text, std::string_view pattern )
{
	std::size_t t = 0;
	std::size_t p = 0;
	std::size_t starP = std::string_view::npos;
	std::size_t starT = 0;

	while ( t < text.size ( ) )
	{
		if ( p < pattern.size ( ) && ( pattern [ p ] == '?' || std::tolower ( static_cast <unsigned char> ( pattern [ p ] ) ) == std::tolower ( static_cast <unsigned char> ( text [ t ] ) ) ) )
		{
			++p;
			++t;
		}
		else if ( p < pattern.size ( ) && pattern [ p ] == '*' )
		{
			starP = p++;
			starT = t;
		}
		else if ( starP != std::string_view::npos )
		{
			p   = starP + 1;
			t   = ++starT;
		}
		else
		{
			return false;
		}
	}

	while ( p < pattern.size ( ) && pattern [ p ] == '*' )
		++p;

	return p == pattern.size ( );
}

void ReadAll ( std::istream& stream, std::streamoff offset, int length, byte** data, int* lengthOut )
{
	auto buffer = std::make_unique <byte[]> ( length );

	stream.clear ( );
	stream.seekg ( offset, std::ios::beg );

	if ( !stream.read ( reinterpret_cast <char*> ( buffer.get ( ) ), length ) )
		throw std::runtime_error ( "short read" );

	*data     = buffer.release ( );
	*lengthOut = length;
}

#pragma pack(push)
#pragma pack(1)

struct zip_dir_t
{
	unsigned long   signature;
	unsigned short  disk;
	unsigned short  cdisk;
	unsigned short  count;
	unsigned short  total;
	unsigned long   size;
	unsigned long   offset;
	unsigned short  comment_len;
};

struct zip_file_t
{
	unsigned long   signature;
	unsigned short  made,
					extract,
					flag,
					method,
					time,
					date;
	unsigned long   crc,
					csize,
					size;
	unsigned short  name_len,
					extra_len,
					comment_len,
					disk,
					attr;
	unsigned long   eattr,
					offset;
};

struct zfile_entry_t
{
	char          name [ 256 ];
	unsigned long offset;
	unsigned long csize;
	unsigned long size;
};

#pragma pack(pop)

struct ZipCacheEntry
{
	fs::path                  archive;
	std::vector <zfile_entry_t> files;
};

std::unordered_map <std::string, ZipCacheEntry>& ZipCache ( )
{
	static std::unordered_map <std::string, ZipCacheEntry> cache;
	return cache;
}

[[nodiscard]] const ZipCacheEntry* OpenZipForRead ( std::string_view zipname )
{
	const std::string key ( zipname );

	if ( auto found = ZipCache ( ).find ( key ); found != ZipCache ( ).end ( ) )
		return &found->second;

	std::ifstream stream ( fs::path ( zipname ), std::ios::binary );

	if ( !stream )
		return nullptr;

	zip_dir_t dir {};

	stream.seekg ( -static_cast <std::streamoff> ( sizeof ( dir ) ), std::ios::end );

	if ( !stream.read ( reinterpret_cast <char*> ( &dir ), sizeof ( dir ) ) )
		return nullptr;

	std::vector <zfile_entry_t> files ( dir.count );
	zip_file_t header {};

	stream.seekg ( dir.offset, std::ios::beg );

	for ( unsigned short i = 0; i < dir.count; ++i )
	{
		if ( !stream.read ( reinterpret_cast <char*> ( &header ), sizeof ( header ) ) )
			return nullptr;

		char name [ 256 ] { };
		const int nameLen = header.name_len < 255 ? header.name_len : 255;

		if ( nameLen > 0 && !stream.read ( name, nameLen ) )
			return nullptr;

		stream.seekg ( header.extra_len + header.comment_len, std::ios::cur );

		zfile_entry_t entry {};
		strncpy_s ( entry.name, ToLowerAscii ( name ).c_str ( ), _TRUNCATE );

		entry.offset = header.offset;
		entry.size   = header.size;
		entry.csize  = header.csize;

		files [ i ] = entry;
	}

	return &( ZipCache ( ).emplace ( key, ZipCacheEntry { fs::path ( zipname ), std::move ( files ) } ).first->second );
}

bool LoadFromZipEntry ( const ZipCacheEntry& cache, const zfile_entry_t& entry, byte** data, int* length )
{
	if ( entry.size == 0 || entry.csize == 0 )
		return false;

	std::ifstream stream ( cache.archive, std::ios::binary );

	if ( !stream )
		return false;

	LF lf;

	stream.seekg ( entry.offset, std::ios::beg );

	if ( !stream.read ( reinterpret_cast <char*> ( &lf ), sizeof ( lf ) ) )
		return false;

	const int compressedSize = entry.csize + sizeof ( LF ) + lf.lf_fn_len + lf.lf_ef_len;

	auto raw = std::make_unique <char[]> ( compressedSize );

	stream.seekg ( -static_cast <std::streamoff> ( sizeof ( LF ) ), std::ios::cur );

	if ( !stream.read ( raw.get ( ), compressedSize ) )
		return false;

	int unzippedSize = 0;
	char* unzipped = unzip_file ( raw.get ( ), unzippedSize );

	if ( !unzipped || unzippedSize != static_cast <int> ( entry.size ) )
	{
		delete[] unzipped;
		return false;
	}

	if ( data )
		*data = reinterpret_cast <byte*> ( unzipped );

	if ( length )
		*length = unzippedSize;

	return true;
}

template <typename T>
bool ReadTrivial ( std::ifstream& stream, T& value )
{
	return static_cast <bool> ( stream.read ( reinterpret_cast <char*> ( &value ), sizeof ( value ) ) );
}

}

bool files_found::ContainsFileCaseInsensitive ( std::string_view filename ) const
{
	return std::any_of ( entries.begin ( ), entries.end ( ), [ & ] ( const Entry& entry )
	{
		return ICaseEqual ( entry.filename, filename );
	} );
}

bool files_found::AddFile ( std::string_view fullname )
{
	if ( fullname.empty ( ) )
		return false;

	Entry added;
	SplitPath ( fullname, &added.filename, &added.path );

	if ( ContainsFileCaseInsensitive ( added.filename ) )
		return false;

	added.fullname = fullname;
	entries.push_back ( std::move ( added ) );

	return true;
}

bool files_found::AddFile ( std::string_view fullname, std::string_view filename, std::string_view path )
{
	if ( filename.empty ( ) || ContainsFileCaseInsensitive ( filename ) )
		return false;

	Entry& added = Add ( );

	added.fullname = fullname;
	added.filename = filename;
	added.path     = path;

	return true;
}

void SplitPath ( std::string_view full, std::string* filename, std::string* path )
{
	const fs::path parsed ( full );

	if ( filename )
		*filename = parsed.filename ( ).string ( );

	if ( path )
		*path = parsed.parent_path ( ).string ( );
}

bool IsDirectoryPath ( std::string_view path )
{
	return fs::is_directory ( fs::path ( path ) );
}

bool file_loader::Load::Direct ( std::string_view filename, byte** data, int* length )
{
	std::ifstream stream ( fs::path ( filename ), std::ios::binary );

	if ( !stream )
		return false;

	stream.seekg ( 0, std::ios::end );

	const std::streamoff size = stream.tellg ( );

	if ( size <= 0 )
		return false;

	try
	{
		ReadAll ( stream, 0, static_cast <int> ( size ), data, length );
	}
	catch ( ... )
	{
		return false;
	}

	return true;
}

bool file_loader::Load::From_PAK ( std::string_view pakname, std::string_view filename, byte** data, int* length )
{
	if ( data )
		*data = nullptr;

	std::ifstream stream ( fs::path ( pakname ), std::ios::binary );

	if ( !stream )
		return false;

	struct PackHeader
	{
		char id [ 4 ];
		int  dirofs;
		int  dirlen;
	};

	struct PackFile
	{
		char name [ 56 ];
		int  filepos;
		int  filelen;
	};

	PackHeader header;

	if ( !ReadTrivial ( stream, header ) )
		return false;

	constexpr char packId [ 4 ] = { 'P', 'A', 'C', 'K' };

	if ( !std::equal ( std::begin ( header.id ), std::end ( header.id ), packId ) )
		return false;

	constexpr int maxFilesInPack = 32768;

	const int entryCount = ( std::min ) ( header.dirlen / static_cast <int> ( sizeof ( PackFile ) ), maxFilesInPack );

	std::vector <PackFile> entries ( entryCount );

	stream.seekg ( header.dirofs, std::ios::beg );

	if ( !stream.read ( reinterpret_cast <char*> ( entries.data ( ) ), static_cast <std::streamsize> ( entries.size ( ) * sizeof ( PackFile ) ) ) )
		return false;

	for ( const PackFile& entry : entries )
	{
		const std::string_view storedName ( entry.name );

		if ( storedName.size ( ) < filename.size ( ) )
			continue;

		if ( !ICaseSuffix ( storedName, filename ) )
			continue;

		if ( length )
			*length = entry.filelen;

		if ( data )
		{
			try
			{
				ReadAll ( stream, entry.filepos, entry.filelen, data, length );
			}
			catch ( ... )
			{
				return false;
			}
		}

		return true;
	}

	return false;
}

bool file_loader::Load::From_ZIP ( std::string_view zipname, std::string_view filename, byte** data, int* length )
{
	const ZipCacheEntry* cache = OpenZipForRead ( zipname );

	if ( !cache )
		return false;

	const std::string lowerName = ToLowerAscii ( filename );

	for ( const zfile_entry_t& entry : cache->files )
	{
		if ( ICaseSuffix ( entry.name, filename ) )
			return LoadFromZipEntry ( *cache, entry, data, length );
	}

	return false;
}

bool file_loader::Load::From_PK3 ( std::string_view pk3name, std::string_view filename, byte** data, int* length )
{
	return From_ZIP ( pk3name, filename, data, length );
}

bool file_loader::Load::From_WAD ( std::string_view wadname, std::string_view filename, byte** data, int* length )
{
	if ( data )
		*data = nullptr;

	std::ifstream stream ( fs::path ( wadname ), std::ios::binary );

	if ( !stream )
		return false;

	struct WadInfo
	{
		char id [ 4 ];
		int  numlumps;
		int  infotableofs;
	};

	struct LumpInfo
	{
		int  filepos;
		int  disksize;
		int  size;
		char type;
		char compression;
		char pad1;
		char pad2;
		char name [ 16 ];
	};

	WadInfo info;

	if ( !ReadTrivial ( stream, info ) )
		return false;

	const bool knownFormat =
		ICaseSuffix ( std::string_view ( info.id, 4 ), "WAD3" ) ||
		ICaseSuffix ( std::string_view ( info.id, 4 ), "WAD2" ) ||
		ICaseSuffix ( std::string_view ( info.id, 4 ), "2DAW" );

	if ( !knownFormat )
		return false;

	std::vector <LumpInfo> lumps ( info.numlumps );

	stream.seekg ( info.infotableofs, std::ios::beg );

	if ( !stream.read ( reinterpret_cast <char*> ( lumps.data ( ) ), static_cast <std::streamsize> ( lumps.size ( ) * sizeof ( LumpInfo ) ) ) )
		return false;

	std::string lumpName ( filename );

	if ( ICaseSuffix ( lumpName, ".wadtex" ) )
		lumpName.resize ( lumpName.size ( ) - 7 );

	for ( const LumpInfo& lump : lumps )
	{
		if ( !ICaseEqual ( lump.name, lumpName ) )
			continue;

		if ( length )
			*length = lump.disksize;

		if ( data )
		{
			try
			{
				ReadAll ( stream, lump.filepos, lump.disksize, data, length );
			}
			catch ( ... )
			{
				return false;
			}
		}

		return true;
	}

	return false;
}

bool file_loader::Find::File ( std::string_view filename, std::string_view path, std::string* fullname )
{
	const fs::path root = path.empty ( ) ? fs::current_path ( ) : fs::path ( path );

	std::error_code ec;

	for ( const fs::directory_entry& item : fs::recursive_directory_iterator ( root, fs::directory_options::skip_permission_denied, ec ) )
	{
		if ( ec )
			break;

		if ( !item.is_regular_file ( ec ) || ec )
			continue;

		if ( !ICaseEqual ( item.path ( ).filename ( ).string ( ), filename ) )
			continue;

		if ( fullname )
			*fullname = fs::absolute ( item.path ( ) ).string ( );

		return true;
	}

	return false;
}

bool file_loader::Find::Files ( std::string_view wildcard, std::string_view path, files_found* files )
{
	const fs::path root = path.empty ( ) ? fs::current_path ( ) : fs::path ( path );

	files->Clear ( );

	std::error_code ec;

	for ( const fs::directory_entry& item : fs::recursive_directory_iterator ( root, fs::directory_options::skip_permission_denied, ec ) )
	{
		if ( ec )
			break;

		if ( !item.is_regular_file ( ec ) || ec )
			continue;

		const std::string leaf = item.path ( ).filename ( ).string ( );

		if ( !WildcardMatch ( leaf, wildcard ) )
			continue;

		files_found::Entry& added = files->Add ( );

		added.filename = leaf;
		added.path     = item.path ( ).parent_path ( ).string ( );
		added.fullname = added.path + "/" + leaf;
	}

	return !files->entries.empty ( );
}

bool file_loader::Find::Files_in_PK3 ( std::string_view pk3name, std::string_view extension, files_found* files )
{
	const ZipCacheEntry* cache = OpenZipForRead ( pk3name );

	if ( !cache )
		return false;

	files->Clear ( );

	const std::string needle = ToLowerAscii ( extension );

	for ( const zfile_entry_t& entry : cache->files )
	{
		if ( std::string_view ( entry.name ).find ( needle ) == std::string_view::npos )
			continue;

		files_found::Entry& added = files->Add ( );

		SplitPath ( entry.name, &added.filename, &added.path );

		added.fullname = entry.name;
	}

	return true;
}

bool file_loader::GetLength::File ( std::string_view filename, int* length )
{
	std::ifstream stream ( fs::path ( filename ), std::ios::binary );

	if ( !stream )
		return false;

	stream.seekg ( 0, std::ios::end );

	*length = static_cast <int> ( stream.tellg ( ) );

	return true;
}

int file_loader::GetLength::File ( FILE* f )
{
	if ( !f )
		return -1;

	const long previous = ftell ( f );

	fseek ( f, 0, SEEK_END );

	const long size = ftell ( f );

	fseek ( f, previous, SEEK_SET );

	return static_cast <int> ( size );
}

bool file_loader::Q3A::Add_PK3 ( std::string_view filename )
{
	for ( const files_found::Entry& entry : Q3A_Resources.entries )
	{
		if ( ICaseEqual ( entry.fullname, filename ) )
			return true;
	}

	const fs::path candidate ( filename );

	if ( !fs::exists ( candidate ) )
		return false;

	files_found::Entry& added = Q3A_Resources.Add ( );

	SplitPath ( filename, &added.filename, &added.path );

	added.fullname = candidate.string ( );

	return true;
}

bool file_loader::Q2::Add_WAD ( std::string_view filename )
{
	for ( const files_found::Entry& entry : Q2_Resources.entries )
	{
		if ( ICaseEqual ( entry.fullname, filename ) )
			return true;
	}

	const fs::path candidate ( filename );

	if ( !fs::exists ( candidate ) )
		return false;

	files_found::Entry& added = Q2_Resources.Add ( );

	SplitPath ( filename, &added.filename, &added.path );

	added.fullname = candidate.string ( );

	return true;
}

//
// DBOFile Functions Implementation
//

//////////////////////////////////////////////////////////////////////////////////
// DBOFILE HEADER ////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////
#include "DBOFile.h"
#include "DBOBlock.h"
#include "direct.h"

DARKSDK_DLL int LoadDBOEx ( LPSTR pFilename, sObject** ppObject )
{
	DWORD dwBlockSize = 0;
	DWORD_PTR pDBOBlock = 0;

	if ( !DBOLoadBlockFile ( pFilename, &pDBOBlock, &dwBlockSize ) )
		return -1;

	if ( !ConstructObject ( ppObject, (LPSTR*)&pDBOBlock ) )
		return -2;

	return 1;
}

DARKSDK_DLL int SaveDBOEx ( LPSTR pFilename, sObject* pObject )
{
	// DBOBlock ptr
	DWORD dwBlockSize = 0;
	DWORD_PTR pDBOBlock = 0;

	// convert pObject to DBOBlock
	if ( !DBOConvertObjectToBlock ( pObject, &pDBOBlock, &dwBlockSize ) )
		return -2;
		
	// save DBOBlock to file
	if ( !DBOSaveBlockFile ( pFilename, pDBOBlock, dwBlockSize ) )
		return -3;

	// free block when done
	char* pDeletePtr = (char*)pDBOBlock;
	SAFE_DELETE(pDeletePtr);

	// okay
	return 1;
}
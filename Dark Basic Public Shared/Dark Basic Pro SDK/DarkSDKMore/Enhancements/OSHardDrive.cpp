#define _CRT_SECURE_NO_WARNINGS

#define DARKSDK	

#include "stdafx.h"
#include "winioctl.h"
#include <stdio.h>
#include "core.h"

#include <wincon.h>
#include <stdlib.h>
#include <time.h>
#include <Nb30.h>
#include "Enchancements.h"

typedef struct _ASTAT_
{
      ADAPTER_STATUS adapt;
      NAME_BUFFER    NameBuff [30];

}ASTAT, * PASTAT;
ASTAT Adapter;

void findMACaddress ( char* pMACAddress )
{
	if (!pMACAddress) return;
	char pLocalString [ 1024 ] = "001122334455";
	NCB Ncb;
	UCHAR uRetCode;
	LANA_ENUM lenum;

	memset( &Ncb, 0, sizeof(Ncb) );
	Ncb.ncb_command = NCBENUM;
	Ncb.ncb_buffer = (UCHAR *)&lenum;
	Ncb.ncb_length = sizeof(lenum);
	uRetCode = Netbios( &Ncb );

	if ( uRetCode == 0 )
	{
		for(int i=0; i < lenum.length ;i++)
		{
			memset( &Ncb, 0, sizeof(Ncb) );
			Ncb.ncb_command = NCBRESET;
			Ncb.ncb_lana_num = lenum.lana[i];
			uRetCode = Netbios( &Ncb );

			memset( &Ncb, 0, sizeof (Ncb) );
			Ncb.ncb_command = NCBASTAT;
			Ncb.ncb_lana_num = lenum.lana[i];
			strcpy_s( (char*)Ncb.ncb_callname, sizeof(Ncb.ncb_callname), "*               " );
			Ncb.ncb_buffer = (PUCHAR) &Adapter;
			Ncb.ncb_length = sizeof(Adapter);
			uRetCode = Netbios( &Ncb );

			if ( uRetCode == 0 )
			{
				sprintf_s( pLocalString, sizeof(pLocalString), "%02X%02X%02X%02X%02X%02X",
						Adapter.adapt.adapter_address[0],
						Adapter.adapt.adapter_address[1],
						Adapter.adapt.adapter_address[2],
						Adapter.adapt.adapter_address[3],
						Adapter.adapt.adapter_address[4],
						Adapter.adapt.adapter_address[5] );
				break;
			}
		}
	}

	// copy MAC address back
	strcpy_s ( pMACAddress, 256, pLocalString );
}

struct sHardDrive
{
	LARGE_INTEGER	liCylinderCount;
	DWORD			dwTracksPerCylinder; 
	DWORD			dwSectorsPerTrack; 
	DWORD			dwBytesPerSector; 

	ULONGLONG		ulTotalBytes;
	ULONGLONG		ulTotalMB;
	ULONGLONG		ulTotalGB;
};

#define MAX_HARD_DRIVE 24

sHardDrive	g_HardDrives      [ MAX_HARD_DRIVE ];
char		g_HardDiskLetters [ MAX_HARD_DRIVE ] [ 4 ] = 
														{
															"c:\\",	"d:\\",	"e:\\",	"f:\\",	"g:\\",	"h:\\",
															"i:\\",	"j:\\",	"k:\\",	"l:\\",	"m:\\",	"n:\\",
															"o:\\",	"p:\\",	"q:\\",	"r:\\",	"s:\\",	"t:\\",
															"u:\\",	"v:\\",	"w:\\",	"x:\\",	"y:\\",	"z:\\"
														};
int			g_iHardDriveCount = 0;

bool CheckHardDriveID ( int iID )
{
	if ( iID < 0 || iID >= MAX_HARD_DRIVE )
	{
		Error ( 7 );
		return false;
	}

	return true;
}

void HardDriveSetup ( void )
{
	// retrieve information about the hard drive

	// local variables
	int				iIndex          = 0;		// drive index
	HANDLE			hDevice         = NULL;		// handle to device
	DWORD			dwBytesReturned = 0;		// bytes returned from io control

	DISK_GEOMETRY	drive;						// drive structure
	char			szDrive [ 256 ];			// to store drive name

	// get a handle to the device
	hDevice = CreateFile ( "\\\\.\\PhysicalDrive0", 0, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL );

	// check handle is valid
	if ( hDevice == INVALID_HANDLE_VALUE )
	{
		return;
	}

	// loop round for all drives
	while ( hDevice != INVALID_HANDLE_VALUE )
	{
		// get information
		if ( !DeviceIoControl (
									hDevice,						// handle to device
									IOCTL_DISK_GET_DRIVE_GEOMETRY,	// control type
									NULL,							// no input data
									0,								// use 0 because we have no input data
									&drive,							// pointer to drive structure
									sizeof ( DISK_GEOMETRY ),		// size of data
									&dwBytesReturned,				// number of bytes returned
									( LPOVERLAPPED ) NULL			// ignored
							  ) )
									return;

		// calculate the disk size
		ULONGLONG TotalBytes = drive.Cylinders.QuadPart * ( ULONG ) drive.TracksPerCylinder * ( ULONG ) drive.SectorsPerTrack * ( ULONG ) drive.BytesPerSector;
		
		g_HardDrives [ g_iHardDriveCount ].liCylinderCount     = drive.Cylinders;
		g_HardDrives [ g_iHardDriveCount ].dwTracksPerCylinder = drive.TracksPerCylinder;
		g_HardDrives [ g_iHardDriveCount ].dwSectorsPerTrack   = drive.SectorsPerTrack;
		g_HardDrives [ g_iHardDriveCount ].dwBytesPerSector    = drive.BytesPerSector;
		g_HardDrives [ g_iHardDriveCount ].ulTotalBytes        = TotalBytes;
		g_HardDrives [ g_iHardDriveCount ].ulTotalMB           = TotalBytes / 1024 / 1024;
		g_HardDrives [ g_iHardDriveCount ].ulTotalGB           = TotalBytes / 1024 / 1024 / 1024;

		g_iHardDriveCount++;

		// close this handle
		CloseHandle ( hDevice );

		// set up string for next drive
		sprintf ( szDrive, "\\\\.\\PhysicalDrive%d", ++iIndex );

		// access the next drive
		hDevice = CreateFile ( szDrive, 0, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL );
	}
}

int GetDriveCount ( void )
{
	return g_iHardDriveCount;
}

int GetDriveCylinderCount ( int iID )
{
	if ( !CheckHardDriveID ( iID ) )
		return 0;

	return ( int ) g_HardDrives [ iID ].liCylinderCount.LowPart;
}

int GetDriveTrackCount ( int iID )
{
	if ( !CheckHardDriveID ( iID ) )
		return 0;

	return ( int ) g_HardDrives [ iID ].dwTracksPerCylinder;
}

int GetDriveSectorsPerTrack	( int iID )
{
	if ( !CheckHardDriveID ( iID ) )
		return 0;

	return ( int ) g_HardDrives [ iID ].dwSectorsPerTrack;
}

int GetDriveBytesPerSector ( int iID )
{
	if ( !CheckHardDriveID ( iID ) )
		return 0;

	return ( int ) g_HardDrives [ iID ].dwBytesPerSector;
}

int GetDriveTotalSize ( int iID, int iReturn )
{
	if ( !CheckHardDriveID ( iID ) )
		return 0;

	if ( iReturn == 0 )
	{
		return ( int ) g_HardDrives [ iID ].ulTotalBytes;
	}
	else if ( iReturn == 1 )
	{
		return ( int ) g_HardDrives [ iID ].ulTotalMB;
	}
	else
		return ( int ) g_HardDrives [ iID ].ulTotalGB;
}

int GetDriveSpace ( int iID, int iMode, int iReturn )
{
	if ( !CheckHardDriveID ( iID ) )
		return 0;

	// when iMode is 0 we're looking for used space, when it's 1
	// we're after free space
	
	ULARGE_INTEGER	ulAvailableToCaller,
					ulDisk,
					ulFree;

	// make sure we're dealing with a fixed drive
	if ( GetDriveType ( g_HardDiskLetters [ iID ] ) == DRIVE_FIXED )
	{
		// get the information we need
		if ( GetDiskFreeSpaceEx ( g_HardDiskLetters [ iID ], &ulAvailableToCaller, &ulDisk, &ulFree ) )
		{
			// set up used values and the final value we need
			ULONGLONG ulUsed = ulDisk.QuadPart - ulFree.QuadPart;
			ULONGLONG ulValue;

			// see if we're going to return free or used data
			if ( iMode == 0 )
				ulValue = ulFree.QuadPart;
			else
				ulValue = ulUsed;

			// convert to mb
			if ( iReturn == 1 )
				ulValue = ulValue / 1024 / 1024;
			
			// convert to gb
			if ( iReturn == 2 )
				ulValue = ulValue / 1024 / 1024 / 1024;

			// return and cast the value to integer
			return ( int ) ulValue;
		}
	}

	// invalid
	return -1;
}

int GetDriveUsedSpace ( int iID, int iReturn )
{
	if ( !CheckHardDriveID ( iID ) )
		return 0;

	return GetDriveSpace ( iID, 1, iReturn );
}

int GetDriveFreeSpace ( int iID, int iReturn )
{
	if ( !CheckHardDriveID ( iID ) )
		return 0;

	return GetDriveSpace ( iID, 0, iReturn );
}

int GetDriveFileLengthSupport ( int iID )
{
	if ( !CheckHardDriveID ( iID ) )
		return 0;

	DWORD dwMaxFileLen = 0;
			
	if ( GetVolumeInformation ( g_HardDiskLetters [ iID ], NULL, 0, NULL, &dwMaxFileLen, NULL, NULL, 0 ) )
		return ( int ) dwMaxFileLen;

	return -1;
}

static std::string ReadRegistryString ( LPCSTR PerfmonNamesKey, LPCSTR key )
{
	HKEY hKey = nullptr;
	if ( RegOpenKeyExA ( HKEY_LOCAL_MACHINE, PerfmonNamesKey, 0L, KEY_READ, &hKey ) != ERROR_SUCCESS )
		return {};

	char data [ 256 ] = {};
	DWORD size = sizeof(data) - 1;
	DWORD type = REG_SZ;
	LONG status = RegQueryValueExA ( hKey, key, nullptr, &type, reinterpret_cast<LPBYTE>(data), &size );
	RegCloseKey ( hKey );

	if ( status == ERROR_SUCCESS )
		return std::string(data);

	return {};
}

DWORD_PTR GetDriveSerial ( DWORD_PTR dwReturn, int iID, int iUniqueCode )
{
	char szSerialA [ 256 ] = "";
	char szSerialB [ 256 ] = "";

	if ( iUniqueCode == 1 )
	{
		std::string sCombined;
		sCombined += ReadRegistryString ( "HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0", "Identifier" );
		sCombined += ReadRegistryString ( "HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0", "ProcessorNameString" );
		sCombined += ReadRegistryString ( "HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0", "VendorIdentifier" );

		for ( int iPort = 0; iPort < 5; ++iPort )
		{
			for ( int iBus = 0; iBus < 8; ++iBus )
			{
				for ( int iTarget = 0; iTarget < 2; ++iTarget )
				{
					char pHDKey [ 1024 ];
					snprintf ( pHDKey, sizeof(pHDKey), "HARDWARE\\DEVICEMAP\\Scsi\\Scsi Port %d\\Scsi Bus %d\\Target ID %d\\Logical Unit Id 0", iPort, iBus, iTarget );
					sCombined += ReadRegistryString ( pHDKey, "Identifier" );
				}
			}
		}

		findMACaddress ( szSerialA );

		DWORD dwHugeNumber = 0x10000;
		for ( char ch : sCombined )
		{
			dwHugeNumber += static_cast<unsigned char>(ch);
		}

		snprintf ( szSerialB, sizeof(szSerialB), "%X%s", dwHugeNumber, szSerialA );
		if ( strlen(szSerialB) > 5 )
			szSerialB[5] = 0;
		strcat_s ( szSerialB, sizeof(szSerialB), szSerialA );
	}
	else
	{
		if ( !CheckHardDriveID ( iID ) )
			return 0;

		DWORD dwSerialNumber = 0;
		if ( GetVolumeInformationA ( g_HardDiskLetters [ iID ], nullptr, 0, &dwSerialNumber, nullptr, nullptr, nullptr, 0 ) )
		{
			sprintf_s ( szSerialA, sizeof(szSerialA), "%08X", dwSerialNumber );
			snprintf ( szSerialB, sizeof(szSerialB), "%.4s-%.4s", szSerialA, szSerialA + 4 );
		}
	}

	return reinterpret_cast<DWORD_PTR>(SetupString ( szSerialB ));
}

DWORD_PTR GetDriveSerial ( DWORD_PTR dwReturn, int iID )
{
	return GetDriveSerial ( dwReturn, iID, 0 );
}

DWORD_PTR GetDriveFileSystem ( DWORD_PTR dwReturn, int iID )
{
	if ( !CheckHardDriveID ( iID ) )
		return 0;

	char szFS         [ 256 ] = "";
	char szFileSystem [ 256 ] = "";
	
	if ( GetVolumeInformation ( g_HardDiskLetters [ iID ], NULL, 0, NULL, NULL, NULL, szFS, 256 ) )
		sprintf ( szFileSystem, "%s", szFS );
	
	return ( DWORD_PTR ) SetupString ( szFileSystem );
}

int GetCDCount ( void )
{
	int iCount = 0;

	for ( int iCounter = 0; iCounter < MAX_HARD_DRIVE; iCounter++ )
	{
		if ( GetDriveType ( g_HardDiskLetters [ iCounter] ) == DRIVE_CDROM )
			iCount++;
	}

	return iCount;
}

DWORD_PTR GetCDLetter ( DWORD_PTR dwReturn, int iNTHCDIndex )
{
	if ( !CheckHardDriveID ( iNTHCDIndex ) )
		return 0;

	// default is no letter
	char szLetter [ 4 ] = "";

	// lee - 070406 - u6rc7 - return CD letter corresponding to nth one found
	if ( iNTHCDIndex > 0 )
	{
		int iCountDown = iNTHCDIndex - 1;
		for ( int iCounter = 0; iCounter < MAX_HARD_DRIVE; iCounter++ )
		{
			if ( GetDriveType ( g_HardDiskLetters [ iCounter ] ) == DRIVE_CDROM )
			{
				if ( iCountDown==0 )
				{
					strcpy ( szLetter, g_HardDiskLetters [ iCounter ] );
					break;
				}
				else
					iCountDown--;
			}
		}
	}

	// return string
	return ( DWORD_PTR ) SetupString ( szLetter );
}

////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////

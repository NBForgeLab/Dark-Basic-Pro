#define _CRT_SECURE_NO_WARNINGS
#include <intrin.h>

#include "oscpuinfo.h"

CPUInfo::CPUInfo ( )
{
	if ( DoesCPUSupportCPUID ( ) )
	{
		RetrieveCPUIdentity ( );
		RetrieveCPUFeatures ( );

		if ( !RetrieveCPUClockSpeed ( ) )
			RetrieveClassicalCPUClockSpeed ( );

		if ( !RetrieveCPUCacheDetails ( ) )
			RetrieveClassicalCPUCacheDetails ( );

		if ( !RetrieveExtendedCPUIdentity ( ) )
			RetrieveClassicalCPUIdentity ( );
		
		RetrieveExtendedCPUFeatures   ( );
		RetrieveProcessorSerialNumber ( );
	}
}

CPUInfo::~CPUInfo ( )
{
}

char* CPUInfo::GetVendorString ( void )
{
	return ChipID.Vendor;
}

char* CPUInfo::GetVendorID ( void )
{
	switch ( ChipManufacturer )
	{
		case Intel:
			return "Intel Corporation";

		case AMD:
			return "Advanced Micro Devices";

		/*
		case NSC:
			return "National Semiconductor";
*/
  
		//case Cyrix:
		//	return "Cyrix Corp., VIA Inc.";

			/*
		case NexGen:
			return "NexGen Inc., Advanced Micro Devices";

		case IDT:
			return "IDT\\Centaur, Via Inc.";

		case UMC:
			return "United Microelectronics Corp.";

		case Rise:
			return "Rise";

		case Transmeta:
			return "Transmeta";
			*/

		default:
			return "Unknown Manufacturer";
	}
}

char* CPUInfo::GetTypeID ( void )
{
	char* szTypeID = new char [ 32 ];

	itoa ( ChipID.Type, szTypeID, 10 );

	return szTypeID;
}

char* CPUInfo::GetFamilyID ( void )
{
	char* szFamilyID = new char [ 32 ];

	itoa ( ChipID.Family, szFamilyID, 10 );

	return szFamilyID;
}

char* CPUInfo::GetModelID ( void )
{
	char* szModelID = new char [ 32 ];

	itoa ( ChipID.Model, szModelID, 10 );

	return szModelID;
}

char* CPUInfo::GetSteppingCode ( void )
{
	char* szSteppingCode = new char [ 32 ];

	itoa ( ChipID.Revision, szSteppingCode, 10 );

	return szSteppingCode;
}

char* CPUInfo::GetExtendedProcessorName ( void )
{
	return ChipID.ProcessorName;
}

char* CPUInfo::GetProcessorSerialNumber ( void )
{
	return ChipID.SerialNumber;
}

int CPUInfo::GetLogicalProcessorsPerPhysical ( void )
{
	return Features.ExtendedFeatures.LogicalProcessorsPerPhysical;
}

int CPUInfo::GetProcessorClockFrequency ( void )
{
	if ( Speed != NULL )
		return Speed->CPUSpeedInMHz;
	else 
		return -1;
}

int CPUInfo::GetProcessorAPICID ( void )
{
	return Features.ExtendedFeatures.APIC_ID;
}

int CPUInfo::GetProcessorCacheXSize ( DWORD dwCacheID )
{
	switch ( dwCacheID )
	{
		case L1CACHE_FEATURE:
			return Features.L1CacheSize;

		case L2CACHE_FEATURE:
			return Features.L2CacheSize;

		case L3CACHE_FEATURE:
			return Features.L3CacheSize;
	}

	return -1;
}

bool CPUInfo::DoesCPUSupportFeature ( DWORD dwFeature )
{
	bool bHasFeature = false;
	
	if ( ( ( dwFeature & MMX_FEATURE            ) != 0 ) && Features.HasMMX                                             ) bHasFeature = true;
	if ( ( ( dwFeature & MMX_PLUS_FEATURE       ) != 0 ) && Features.ExtendedFeatures.HasMMXPlus                        ) bHasFeature = true;
	if ( ( ( dwFeature & SSE_FEATURE            ) != 0 ) && Features.HasSSE                                             ) bHasFeature = true;
	if ( ( ( dwFeature & SSE_FP_FEATURE         ) != 0 ) && Features.HasSSEFP                                           ) bHasFeature = true;
	if ( ( ( dwFeature & SSE_MMX_FEATURE        ) != 0 ) && Features.ExtendedFeatures.HasSSEMMX                         ) bHasFeature = true;
	if ( ( ( dwFeature & SSE2_FEATURE           ) != 0 ) && Features.HasSSE2                                            ) bHasFeature = true;
	if ( ( ( dwFeature & AMD_3DNOW_FEATURE      ) != 0 ) && Features.ExtendedFeatures.Has3DNow                          ) bHasFeature = true;
	if ( ( ( dwFeature & AMD_3DNOW_PLUS_FEATURE ) != 0 ) && Features.ExtendedFeatures.Has3DNowPlus                      ) bHasFeature = true;
	if ( ( ( dwFeature & IA64_FEATURE           ) != 0 ) && Features.HasIA64                                            ) bHasFeature = true;
	if ( ( ( dwFeature & MP_CAPABLE             ) != 0 ) && Features.ExtendedFeatures.SupportsMP                        ) bHasFeature = true;
	if ( ( ( dwFeature & SERIALNUMBER_FEATURE   ) != 0 ) && Features.HasSerial                                          ) bHasFeature = true;
	if ( ( ( dwFeature & APIC_FEATURE           ) != 0 ) && Features.HasAPIC                                            ) bHasFeature = true;
	if ( ( ( dwFeature & CMOV_FEATURE           ) != 0 ) && Features.HasCMOV                                            ) bHasFeature = true;
	if ( ( ( dwFeature & MTRR_FEATURE           ) != 0 ) && Features.HasMTRR                                            ) bHasFeature = true;
	if ( ( ( dwFeature & L1CACHE_FEATURE        ) != 0 ) && ( Features.L1CacheSize != -1 )                              ) bHasFeature = true;
	if ( ( ( dwFeature & L2CACHE_FEATURE        ) != 0 ) && ( Features.L2CacheSize != -1 )                              ) bHasFeature = true;
	if ( ( ( dwFeature & L3CACHE_FEATURE        ) != 0 ) && ( Features.L3CacheSize != -1 )                              ) bHasFeature = true;
	if ( ( ( dwFeature & ACPI_FEATURE           ) != 0 ) && Features.HasACPI                                            ) bHasFeature = true;
	if ( ( ( dwFeature & THERMALMONITOR_FEATURE ) != 0 ) && Features.HasThermal                                         ) bHasFeature = true;
	if ( ( ( dwFeature & TEMPSENSEDIODE_FEATURE ) != 0 ) && Features.ExtendedFeatures.PowerManagement.HasTempSenseDiode ) bHasFeature = true;
	if ( ( ( dwFeature & FREQUENCYID_FEATURE    ) != 0 ) && Features.ExtendedFeatures.PowerManagement.HasFrequencyID    ) bHasFeature = true;
	if ( ( ( dwFeature & VOLTAGEID_FREQUENCY    ) != 0 ) && Features.ExtendedFeatures.PowerManagement.HasVoltageID      ) bHasFeature = true;

	return bHasFeature;
}

bool __cdecl CPUInfo::DoesCPUSupportCPUID ( void )
{
#if defined(_WIN64)
    return true;
#else
	int CPUIDPresent = 0;
    __try {
        _asm {
			push eax
			push ebx
			push ecx
			push edx
            mov eax, 0
			CPUID_INSTRUCTION
			pop edx
			pop ecx
			pop ebx
			pop eax
        }
    } __except ( 1 ) {
        CPUIDPresent = false;
		return false;
    }
	return ( CPUIDPresent == 0 ) ? true : false;
#endif
}

bool __cdecl CPUInfo::RetrieveCPUFeatures ( void )
{
	int CPUFeatures = 0;
	int CPUAdvanced = 0;

#if defined(_WIN64)
	int info[4] = {0};
	__cpuid(info, 1);
	CPUFeatures = info[3];
	CPUAdvanced = info[1];
#else
	__try {
		_asm {
			push eax
			push ebx
			push ecx
			push edx
			mov eax,1
			CPUID_INSTRUCTION
			mov CPUFeatures, edx
			mov CPUAdvanced, ebx
			pop edx
			pop ecx
			pop ebx
			pop eax
		}
	} __except ( 1 ) {
		return false;
	}
#endif

	Features.HasFPU     = ( ( CPUFeatures & 0x00000001 ) != 0 );
	Features.HasTSC     = ( ( CPUFeatures & 0x00000010 ) != 0 );
	Features.HasAPIC    = ( ( CPUFeatures & 0x00000200 ) != 0 );
	Features.HasMTRR    = ( ( CPUFeatures & 0x00001000 ) != 0 );
	Features.HasCMOV    = ( ( CPUFeatures & 0x00008000 ) != 0 );
	Features.HasSerial  = ( ( CPUFeatures & 0x00040000 ) != 0 );
	Features.HasACPI    = ( ( CPUFeatures & 0x00400000 ) != 0 );
    Features.HasMMX     = ( ( CPUFeatures & 0x00800000 ) != 0 );
	Features.HasSSE     = ( ( CPUFeatures & 0x02000000 ) != 0 );
	Features.HasSSE2    = ( ( CPUFeatures & 0x04000000 ) != 0 );
	Features.HasThermal = ( ( CPUFeatures & 0x20000000 ) != 0 );
	Features.HasIA64    = ( ( CPUFeatures & 0x40000000 ) != 0 );

	if ( Features.HasSSE ) {
#if defined(_WIN64)
		Features.HasSSEFP = true;
#else
		__try {
			_asm {
				_emit 0x0f
	    		_emit 0x56
	    		_emit 0xc0	
			}
			Features.HasSSEFP = true;
	    } __except ( 1 ) {
			Features.HasSSEFP = false;
		}
#endif
	} else {
		Features.HasSSEFP = false;
	}

	if ( ChipManufacturer == Intel ) {
		Features.ExtendedFeatures.SupportsHyperthreading       = ( ( CPUFeatures & 0x10000000 ) != 0 );
		Features.ExtendedFeatures.LogicalProcessorsPerPhysical = ( Features.ExtendedFeatures.SupportsHyperthreading ) ? ( ( CPUAdvanced & 0x00FF0000 ) >> 16 ) : 1;
		if ( ( Features.ExtendedFeatures.SupportsHyperthreading ) && ( Features.HasAPIC ) ) {
			Features.ExtendedFeatures.APIC_ID = ( ( CPUAdvanced & 0xFF000000 ) >> 24 );
		}
	}
	return true;
}

bool __cdecl CPUInfo::RetrieveCPUIdentity ( void )
{
	int CPUVendor [ 3 ] = {0};
	int CPUSignature = 0;

#if defined(_WIN64)
	int info[4] = {0};
	__cpuid(info, 0);
	CPUVendor[0] = info[1];
	CPUVendor[1] = info[3];
	CPUVendor[2] = info[2];
	__cpuid(info, 1);
	CPUSignature = info[0];
#else
	__try {
		_asm {
			push eax
			push ebx
			push ecx
			push edx
			mov eax, 0
			CPUID_INSTRUCTION
			mov CPUVendor [ 0 * TYPE int ], ebx
			mov CPUVendor [ 1 * TYPE int ], edx
			mov CPUVendor [ 2 * TYPE int ], ecx
			mov eax,1
			CPUID_INSTRUCTION
			mov CPUSignature, eax
			pop edx
			pop ecx
			pop ebx
			pop eax
		}
	} __except ( 1 ) {
		return false;
	}
#endif

	memcpy ( ChipID.Vendor,             & ( CPUVendor [ 0 ] ), sizeof ( int ) );
	memcpy ( & ( ChipID.Vendor [ 4 ] ), & ( CPUVendor [ 1 ] ), sizeof ( int ) );
	memcpy ( & ( ChipID.Vendor [ 8 ] ), & ( CPUVendor [ 2 ] ), sizeof ( int ) );
	ChipID.Vendor [ 12 ] = '\0';

	if ( strcmp ( ChipID.Vendor, "GenuineIntel" ) == 0 ) ChipManufacturer = Intel;
	else if ( strcmp ( ChipID.Vendor, "AuthenticAMD" ) == 0 ) ChipManufacturer = AMD;
	else ChipManufacturer = UnknownManufacturer;

	ChipID.ExtendedFamily =	( ( CPUSignature & 0x0FF00000 ) >> 20 );
	ChipID.ExtendedModel  =	( ( CPUSignature & 0x000F0000 ) >> 16 );
	ChipID.Type           =	( ( CPUSignature & 0x0000F000 ) >> 12 );
	ChipID.Family         =	( ( CPUSignature & 0x00000F00 ) >>  8 );
	ChipID.Model          =	( ( CPUSignature & 0x000000F0 ) >>  4 );
	ChipID.Revision       =	( ( CPUSignature & 0x0000000F ) >>  0 );
	return true;
}

bool __cdecl CPUInfo::RetrieveCPUCacheDetails ( void )
{
	int L1Cache [ 4 ] = { 0, 0, 0, 0 };
	int L2Cache [ 4 ] = { 0, 0, 0, 0 };

	if ( RetrieveCPUExtendedLevelSupport ( 0x80000005 ) ) {
#if defined(_WIN64)
		int info[4] = {0};
		__cpuid(info, 0x80000005);
		L1Cache[0] = info[0]; L1Cache[1] = info[1]; L1Cache[2] = info[2]; L1Cache[3] = info[3];
#else
		__try {
			_asm {
				push eax
				push ebx
				push ecx
				push edx
				mov eax, 0x80000005
				CPUID_INSTRUCTION
				mov L1Cache [ 0 * TYPE int ], eax
				mov L1Cache [ 1 * TYPE int ], ebx
				mov L1Cache [ 2 * TYPE int ], ecx
				mov L1Cache [ 3 * TYPE int ], edx
				pop edx
				pop ecx
				pop ebx
				pop eax
			}
		} __except ( 1 ) {
			return false;
		}
#endif
		Features.L1CacheSize  = ( ( L1Cache [ 2 ] & 0xFF000000 ) >> 24 );
		Features.L1CacheSize += ( ( L1Cache [ 3 ] & 0xFF000000 ) >> 24 );
	} else {
		Features.L1CacheSize = -1;
	}

	if ( RetrieveCPUExtendedLevelSupport ( 0x80000006 ) ) {
#if defined(_WIN64)
		int info[4] = {0};
		__cpuid(info, 0x80000006);
		L2Cache[0] = info[0]; L2Cache[1] = info[1]; L2Cache[2] = info[2]; L2Cache[3] = info[3];
#else
		__try {
			_asm {
				push eax
				push ebx
				push ecx
				push edx
				mov eax, 0x80000006
				CPUID_INSTRUCTION
				mov L2Cache [ 0 * TYPE int ], eax
				mov L2Cache [ 1 * TYPE int ], ebx
				mov L2Cache [ 2 * TYPE int ], ecx
				mov L2Cache [ 3 * TYPE int ], edx
				pop edx
				pop ecx
				pop ebx
				pop eax
			}
		} __except ( 1 ) {
			return false;
		}
#endif
		Features.L2CacheSize = ( ( L2Cache [ 2 ] & 0xFFFF0000 ) >> 16 );
	} else {
		Features.L2CacheSize = -1;
	}
	return true;
}

bool __cdecl CPUInfo::RetrieveClassicalCPUCacheDetails ( void )
{
	int TLBCacheData [ 4 ] = {0};
	int TLBDataIndex = 0;
	int TLBCache [ 256 ] = {0};

#if defined(_WIN64)
	int info[4] = {0};
	__cpuid(info, 2);
	TLBCacheData[0] = info[0]; TLBCacheData[1] = info[1]; TLBCacheData[2] = info[2]; TLBCacheData[3] = info[3];
#else
	__try {
		_asm {
			push eax
			push ebx
			push ecx
			push edx
			mov eax, 2
			CPUID_INSTRUCTION
			mov TLBCacheData [ 0 * TYPE int ], eax
			mov TLBCacheData [ 1 * TYPE int ], ebx
			mov TLBCacheData [ 2 * TYPE int ], ecx
			mov TLBCacheData [ 3 * TYPE int ], edx
			pop edx
			pop ecx
			pop ebx
			pop eax
		}
	} __except ( 1 ) {
		return false;
	}
#endif

	for ( int i = 0; i < 4; ++i ) {
		if ( ( TLBCacheData [ i ] & 0x80000000 ) == 0 ) {
			unsigned char* b = ( unsigned char* ) &TLBCacheData [ i ];
			for ( int j = ( i == 0 ? 1 : 0 ); j < 4; ++j ) {
				TLBCache [ TLBDataIndex++ ] = b [ j ];
			}
		}
	}
	return true;
}

bool __cdecl CPUInfo::RetrieveCPUClockSpeed ( void )
{
	// first of all we check to see if the RDTSC (0x0F, 0x31) instruction is supported
	if ( !Features.HasTSC )
		return false;

	// get the clock speed
	Speed = new CPUSpeed ( );

	if ( Speed == NULL )
		return false;

	return true;
}

bool __cdecl CPUInfo::RetrieveClassicalCPUClockSpeed ( void )
{
	LARGE_INTEGER liStart, liEnd, liFrequency;
	QueryPerformanceFrequency ( &liFrequency );
	QueryPerformanceCounter ( &liStart );
#if defined(_WIN64)
	Sleep(50);
#else
	__try {
		_asm {
			mov eax, 0x80000000
			mov ebx, CLASSICAL_CPU_FREQ_LOOP
			Timer_Loop: 
			bsf ecx,eax
			dec ebx
			jnz Timer_Loop
		}	
	} __except ( 1 ) {
		return false;
	}
#endif
	QueryPerformanceCounter ( &liEnd );
	double dDifference = ( ( double ) ( liEnd.QuadPart - liStart.QuadPart ) ) / ( ( double ) liFrequency.QuadPart );
	if ( dDifference > 0.0 ) {
		Speed->CPUSpeedInMHz = ( int ) ( 1.0 / ( dDifference * 1000.0 ) );
	}
	return true;
}

bool __cdecl CPUInfo::RetrieveCPUExtendedLevelSupport ( int CPULevelToCheck )
{
	int MaxCPUExtendedLevel = 0;
#if defined(_WIN64)
	int info[4] = {0};
	__cpuid(info, 0x80000000);
	MaxCPUExtendedLevel = info[0];
#else
	__try {
		_asm {
			push eax
			push ebx
			push ecx
			push edx
			mov eax,0x80000000
			CPUID_INSTRUCTION
			mov MaxCPUExtendedLevel, eax
			pop edx
			pop ecx
			pop ebx
			pop eax
		}
	} __except ( 1 ) {
		return false;
	}
#endif
	return ( ( unsigned int ) MaxCPUExtendedLevel >= ( unsigned int ) CPULevelToCheck );
}

bool __cdecl CPUInfo::RetrieveExtendedCPUFeatures ( void )
{
	int CPUExtendedFeatures = 0;
	if ( ChipManufacturer == Intel ) return false;
	if ( !RetrieveCPUExtendedLevelSupport ( 0x80000001 ) ) return false;

#if defined(_WIN64)
	int info[4] = {0};
	__cpuid(info, 0x80000001);
	CPUExtendedFeatures = info[3];
#else
	__try {
		_asm {
			push eax
			push ebx
			push ecx
			push edx
			mov eax,0x80000001
			CPUID_INSTRUCTION
			mov CPUExtendedFeatures, edx
			pop edx
			pop ecx
			pop ebx
			pop eax
		}
	} __except ( 1 ) {
		return false;
	}
#endif

	Features.ExtendedFeatures.Has3DNow     = ( ( CPUExtendedFeatures & 0x80000000 ) != 0 );
	Features.ExtendedFeatures.Has3DNowPlus = ( ( CPUExtendedFeatures & 0x40000000 ) != 0 );
	Features.ExtendedFeatures.HasSSEMMX    = ( ( CPUExtendedFeatures & 0x00400000 ) != 0 );
	Features.ExtendedFeatures.SupportsMP   = ( ( CPUExtendedFeatures & 0x00080000 ) != 0 );
	
	if ( ChipManufacturer == AMD ) {
		Features.ExtendedFeatures.HasMMXPlus = ( ( CPUExtendedFeatures & 0x00400000 ) != 0 );
	}
	if ( ChipManufacturer == Cyrix ) {
		Features.ExtendedFeatures.HasMMXPlus = ( ( CPUExtendedFeatures & 0x01000000 ) != 0 );
	}
	return true;
}

bool __cdecl CPUInfo::RetrieveProcessorSerialNumber ( void )
{
	int SerialNumber [ 3 ] = {0};
	if ( DoesCPUSupportFeature ( SERIALNUMBER_FEATURE ) ) {
#if defined(_WIN64)
		int info[4] = {0};
		__cpuid(info, 3);
		SerialNumber[0] = info[1]; SerialNumber[1] = info[2]; SerialNumber[2] = info[3];
#else
		__try {
			_asm {
				push eax
				push ebx
				push ecx
				push edx
				mov eax, 3
				CPUID_INSTRUCTION
				mov SerialNumber [ 0 * TYPE int ], ebx
				mov SerialNumber [ 1 * TYPE int ], ecx
				mov SerialNumber [ 2 * TYPE int ], edx
				pop edx
				pop ecx
				pop ebx
				pop eax
			}
		} __except ( 1 ) {
			return false;
		}
#endif
		sprintf ( ChipID.SerialNumber, "%04X-%04X-%04X-%04X-%04X-%04X",
			( ( SerialNumber [ 1 ] & 0xFFFF0000 ) >> 16 ), ( ( SerialNumber [ 1 ] & 0x0000FFFF ) >>  0 ),
			( ( SerialNumber [ 2 ] & 0xFFFF0000 ) >> 16 ), ( ( SerialNumber [ 2 ] & 0x0000FFFF ) >>  0 ),
			( ( SerialNumber [ 0 ] & 0xFFFF0000 ) >> 16 ), ( ( SerialNumber [ 0 ] & 0x0000FFFF ) >>  0 ) );
	}
	return true;
}

bool __cdecl CPUInfo::RetrieveCPUPowerManagement ( void )
{
	int CPUPowerManagement = 0;
	if ( RetrieveCPUExtendedLevelSupport ( 0x80000007 ) ) {
#if defined(_WIN64)
		int info[4] = {0};
		__cpuid(info, 0x80000007);
		CPUPowerManagement = info[3];
#else
		__try {
			_asm {
				push eax
				push ebx
				push ecx
				push edx
				mov eax,0x80000007
				CPUID_INSTRUCTION
				mov CPUPowerManagement, edx
				pop edx
				pop ecx
				pop ebx
				pop eax
			}
		} __except ( 1 ) {
			return false;
		}
#endif
		Features.ExtendedFeatures.PowerManagement.HasTempSenseDiode = ( ( CPUPowerManagement & 0x00000001 ) != 0 );
		Features.ExtendedFeatures.PowerManagement.HasFrequencyID    = ( ( CPUPowerManagement & 0x00000002 ) != 0 );
		Features.ExtendedFeatures.PowerManagement.HasVoltageID      = ( ( CPUPowerManagement & 0x00000004 ) != 0 );
	}
	return true;
}

bool __cdecl CPUInfo::RetrieveExtendedCPUIdentity ( void )
{
	int CPUExtendedIdentity [ 12 ] = {0};
	if ( RetrieveCPUExtendedLevelSupport ( 0x80000004 ) ) {
#if defined(_WIN64)
		__cpuid(&CPUExtendedIdentity[0], 0x80000002);
		__cpuid(&CPUExtendedIdentity[4], 0x80000003);
		__cpuid(&CPUExtendedIdentity[8], 0x80000004);
#else
		__try {
			_asm {
				push eax
				push ebx
				push ecx
				push edx
				mov eax,0x80000002
				CPUID_INSTRUCTION
				mov CPUExtendedIdentity[0 * TYPE int], eax
				mov CPUExtendedIdentity[1 * TYPE int], ebx
				mov CPUExtendedIdentity[2 * TYPE int], ecx
				mov CPUExtendedIdentity[3 * TYPE int], edx
				mov eax,0x80000003
				CPUID_INSTRUCTION
				mov CPUExtendedIdentity[4 * TYPE int], eax
				mov CPUExtendedIdentity[5 * TYPE int], ebx
				mov CPUExtendedIdentity[6 * TYPE int], ecx
				mov CPUExtendedIdentity[7 * TYPE int], edx
				mov eax,0x80000004
				CPUID_INSTRUCTION
				mov CPUExtendedIdentity[8 * TYPE int], eax
				mov CPUExtendedIdentity[9 * TYPE int], ebx
				mov CPUExtendedIdentity[10 * TYPE int], ecx
				mov CPUExtendedIdentity[11 * TYPE int], edx
				pop edx
				pop ecx
				pop ebx
				pop eax
			}
		} __except ( 1 ) {
			return false;
		}
#endif
		memcpy ( ( void* ) ChipID.ProcessorName, ( void* ) CPUExtendedIdentity, 48 );
		ChipID.ProcessorName [ 48 ] = '\0';
		return true;
	}
	return false;
}

bool _cdecl CPUInfo::RetrieveClassicalCPUIdentity ()
{
	// Start by decided which manufacturer we are using....
	switch (ChipManufacturer) {
		case Intel:
			// Check the family / model / revision to determine the CPU ID.
			switch (ChipID.Family) {
				/*
				case 3:
					sprintf (ChipID.ProcessorName, "Newer i80386 family"); 
					break;
				case 4:
					switch (ChipID.Model) {
						case 0: STORE_CLASSICAL_NAME ("i80486DX-25/33"); break;
						case 1: STORE_CLASSICAL_NAME ("i80486DX-50"); break;
						case 2: STORE_CLASSICAL_NAME ("i80486SX"); break;
						case 3: STORE_CLASSICAL_NAME ("i80486DX2"); break;
						case 4: STORE_CLASSICAL_NAME ("i80486SL"); break;
						case 5: STORE_CLASSICAL_NAME ("i80486SX2"); break;
						case 7: STORE_CLASSICAL_NAME ("i80486DX2 WriteBack"); break;
						case 8: STORE_CLASSICAL_NAME ("i80486DX4"); break;
						case 9: STORE_CLASSICAL_NAME ("i80486DX4 WriteBack"); break;
						//default: STORE_CLASSICAL_NAME ("Unknown 80486 family"); return false;
					}
					break;
					
				case 5:
					
					switch (ChipID.Model) {
						case 0: STORE_CLASSICAL_NAME ("P5 A-Step"); break;
						case 1: STORE_CLASSICAL_NAME ("P5"); break;
						case 2: STORE_CLASSICAL_NAME ("P54C"); break;
						case 3: STORE_CLASSICAL_NAME ("P24T OverDrive"); break;
						case 4: STORE_CLASSICAL_NAME ("P55C"); break;
						case 7: STORE_CLASSICAL_NAME ("P54C"); break;
						case 8: STORE_CLASSICAL_NAME ("P55C (0.25µm)"); break;
						default: STORE_CLASSICAL_NAME ("Unknown Pentium® family"); return false;
					}
					
					break;
					*/

				case 6:
					switch (ChipID.Model) {
						/*
						case 0: STORE_CLASSICAL_NAME ("P6 A-Step"); break;
						case 1: STORE_CLASSICAL_NAME ("P6"); break;
						case 3: STORE_CLASSICAL_NAME ("Pentium® II (0.28 µm)"); break;
						case 5: STORE_CLASSICAL_NAME ("Pentium® II (0.25 µm)"); break;
						case 6: STORE_CLASSICAL_NAME ("Pentium® II With On-Die L2 Cache"); break;
						case 7: STORE_CLASSICAL_NAME ("Pentium® III (0.25 µm)"); break;
						case 8: STORE_CLASSICAL_NAME ("Pentium® III (0.18 µm) With 256 KB On-Die L2 Cache "); break;
						case 0xa: STORE_CLASSICAL_NAME ("Pentium® III (0.18 µm) With 1 Or 2 MB On-Die L2 Cache "); break;
						case 0xb: STORE_CLASSICAL_NAME ("Pentium® III (0.13 µm) With 256 Or 512 KB On-Die L2 Cache "); break;
						default: STORE_CLASSICAL_NAME ("Unknown P6 family"); return false;
						*/

						case 0: STORE_CLASSICAL_NAME ("P6"); break;
						//case 1: STORE_CLASSICAL_NAME ("P6"); break;
						case 3: STORE_CLASSICAL_NAME ("Pentium® II"); break;
						case 5: STORE_CLASSICAL_NAME ("Pentium® II"); break;
						case 6: STORE_CLASSICAL_NAME ("Pentium® II"); break;
						case 7: STORE_CLASSICAL_NAME ("Pentium® III"); break;
						case 8: STORE_CLASSICAL_NAME ("Pentium® III"); break;
						case 0xa: STORE_CLASSICAL_NAME ("Pentium® III"); break;
						case 0xb: STORE_CLASSICAL_NAME ("Pentium® III"); break;
						default: STORE_CLASSICAL_NAME ("Unknown Pentium 3 family"); return false;
					}
					break;
					/*
				case 7:
					STORE_CLASSICAL_NAME ("Intel Merced (IA-64)");
					break;
					*/
				case 0xf:
					// Check the extended family bits...
					switch (ChipID.ExtendedFamily) {
						case 0:
							switch (ChipID.Model) {
								case 0: STORE_CLASSICAL_NAME ("Pentium® IV"); break;
								case 1: STORE_CLASSICAL_NAME ("Pentium® IV"); break;
								case 2: STORE_CLASSICAL_NAME ("Pentium® IV"); break;
								default: STORE_CLASSICAL_NAME ("Unknown Pentium 4 family"); return false;
							}
							break;
							/*
						case 1:
							STORE_CLASSICAL_NAME ("Intel McKinley (IA-64)");
							break;
							*/
					}
					break;
				default:
					STORE_CLASSICAL_NAME ("Unknown Intel family");
					return false;
			}
			break;

		case AMD:
			// Check the family / model / revision to determine the CPU ID.
			switch (ChipID.Family) {
				case 4:
					/*
					switch (ChipID.Model) {
						case 3: STORE_CLASSICAL_NAME ("80486DX2"); break;
						case 7: STORE_CLASSICAL_NAME ("80486DX2 WriteBack"); break;
						case 8: STORE_CLASSICAL_NAME ("80486DX4"); break;
						case 9: STORE_CLASSICAL_NAME ("80486DX4 WriteBack"); break;
						case 0xe: STORE_CLASSICAL_NAME ("5x86"); break;
						case 0xf: STORE_CLASSICAL_NAME ("5x86WB"); break;
						default: STORE_CLASSICAL_NAME ("Unknown 80486 family"); return false;
					}
					break;
					*/
				case 5:
					switch (ChipID.Model) {
						case 0: STORE_CLASSICAL_NAME ("SSA5"); break;
						case 1: STORE_CLASSICAL_NAME ("5k86"); break;
						case 2: STORE_CLASSICAL_NAME ("5k86"); break;
						case 3: STORE_CLASSICAL_NAME ("5k86"); break;
						case 6: STORE_CLASSICAL_NAME ("K6"); break;
						case 7: STORE_CLASSICAL_NAME ("K6"); break;
						case 8: STORE_CLASSICAL_NAME ("K6-2"); break;
						case 9: STORE_CLASSICAL_NAME ("K6-III"); break;
						case 0xd: STORE_CLASSICAL_NAME ("K6-2+"); break;
						default: STORE_CLASSICAL_NAME ("Unknown 80586 family"); return false;
					}
					break;
				case 6:
					switch (ChipID.Model) {
						case 1: STORE_CLASSICAL_NAME ("Athlon"); break;
						case 2: STORE_CLASSICAL_NAME ("Athlon"); break;
						case 3: STORE_CLASSICAL_NAME ("Duron ( SF core )"); break;
						case 4: STORE_CLASSICAL_NAME ("Athlon ( Thunderbird core )"); break;
						case 6: STORE_CLASSICAL_NAME ("Athlon ( Palomino core )"); break;
						case 7: STORE_CLASSICAL_NAME ("Duron ( Morgan core )"); break;
						case 8: 
							if (Features.ExtendedFeatures.SupportsMP)
								STORE_CLASSICAL_NAME ("Athlon MP"); 
							else STORE_CLASSICAL_NAME ("Athlon XP");
							break;
						default: STORE_CLASSICAL_NAME ("Unknown K7 family"); return false;
					}
					break;
				default:
					STORE_CLASSICAL_NAME ("Unknown AMD family");
					return false;
			}
			break;

			/*
		case Transmeta:
			switch (ChipID.Family) {	
				case 5:
					switch (ChipID.Model) {
						case 4: STORE_CLASSICAL_NAME ("Crusoe TM3x00 and TM5x00"); break;
						default: STORE_CLASSICAL_NAME ("Unknown Crusoe family"); return false;
					}
					break;
				default:
					STORE_CLASSICAL_NAME ("Unknown Transmeta family");
					return false;
			}
			break;

		case Rise:
			switch (ChipID.Family) {	
				case 5:
					switch (ChipID.Model) {
						case 0: STORE_CLASSICAL_NAME ("mP6 (0.25 µm)"); break;
						case 2: STORE_CLASSICAL_NAME ("mP6 (0.18 µm)"); break;
						default: STORE_CLASSICAL_NAME ("Unknown Rise family"); return false;
					}
					break;
				default:
					STORE_CLASSICAL_NAME ("Unknown Rise family");
					return false;
			}
			break;

		case UMC:
			switch (ChipID.Family) {	
				case 4:
					switch (ChipID.Model) {
						case 1: STORE_CLASSICAL_NAME ("U5D"); break;
						case 2: STORE_CLASSICAL_NAME ("U5S"); break;
						default: STORE_CLASSICAL_NAME ("Unknown UMC family"); return false;
					}
					break;
				default:
					STORE_CLASSICAL_NAME ("Unknown UMC family");
					return false;
			}
			break;

		case IDT:
			switch (ChipID.Family) {	
				case 5:
					switch (ChipID.Model) {
						case 4: STORE_CLASSICAL_NAME ("C6"); break;
						case 8: STORE_CLASSICAL_NAME ("C2"); break;
						case 9: STORE_CLASSICAL_NAME ("C3"); break;
						default: STORE_CLASSICAL_NAME ("Unknown IDT\\Centaur family"); return false;
					}
					break;
				case 6:
					switch (ChipID.Model) {
						case 6: STORE_CLASSICAL_NAME ("VIA Cyrix III - Samuel"); break;
						default: STORE_CLASSICAL_NAME ("Unknown IDT\\Centaur family"); return false;
					}
					break;
				default:
					STORE_CLASSICAL_NAME ("Unknown IDT\\Centaur family");
					return false;
			}
			break;
			*/

			/*
		case Cyrix:
			switch (ChipID.Family) {	
				case 4:
					switch (ChipID.Model) {
						case 4: STORE_CLASSICAL_NAME ("MediaGX GX, GXm"); break;
						case 9: STORE_CLASSICAL_NAME ("5x86"); break;
						default: STORE_CLASSICAL_NAME ("Unknown Cx5x86 family"); return false;
					}
					break;
				case 5:
					switch (ChipID.Model) {
						case 2: STORE_CLASSICAL_NAME ("Cx6x86"); break;
						case 4: STORE_CLASSICAL_NAME ("MediaGX GXm"); break;
						default: STORE_CLASSICAL_NAME ("Unknown Cx6x86 family"); return false;
					}
					break;
				case 6:
					switch (ChipID.Model) {
						case 0: STORE_CLASSICAL_NAME ("6x86MX"); break;
						case 5: STORE_CLASSICAL_NAME ("Cyrix M2 Core"); break;
						case 6: STORE_CLASSICAL_NAME ("WinChip C5A Core"); break;
						case 7: STORE_CLASSICAL_NAME ("WinChip C5B\\C5C Core"); break;
						case 8: STORE_CLASSICAL_NAME ("WinChip C5C-T Core"); break;
						default: STORE_CLASSICAL_NAME ("Unknown 6x86MX\\Cyrix III family"); return false;
					}
					break;
				default:
					STORE_CLASSICAL_NAME ("Unknown Cyrix family");
					return false;
			}
			break;
			*/

			/*
		case NexGen:
			switch (ChipID.Family) {	
				case 5:
					switch (ChipID.Model) {
						case 0: STORE_CLASSICAL_NAME ("Nx586 or Nx586FPU"); break;
						default: STORE_CLASSICAL_NAME ("Unknown NexGen family"); return false;
					}
					break;
				default:
					STORE_CLASSICAL_NAME ("Unknown NexGen family");
					return false;
			}
			break;

		case NSC:
			STORE_CLASSICAL_NAME ("Cx486SLC \\ DLC \\ Cx486S A-Step");
			break;
			*/

		default:
			// We cannot identify the processor.
			STORE_CLASSICAL_NAME ("Unknown family");
			return false;
	}

	return true;
}

// --------------------------------------------------------
//
//         Constructor Functions - CPUSpeed Class
//
// --------------------------------------------------------

CPUSpeed::CPUSpeed ()
{
	unsigned int uiRepetitions = 1;
	unsigned int uiMSecPerRepetition = 50;
	__int64	i64Total = 0, i64Overhead = 0;

	for (unsigned int nCounter = 0; nCounter < uiRepetitions; nCounter ++) {
		i64Total += GetCyclesDifference (CPUSpeed::Delay, uiMSecPerRepetition);
		i64Overhead += GetCyclesDifference (CPUSpeed::DelayOverhead, uiMSecPerRepetition);
	}

	// Calculate the MHz speed.
	i64Total -= i64Overhead;
	i64Total /= uiRepetitions;
	i64Total /= uiMSecPerRepetition;
	i64Total /= 1000;

	// Save the CPU speed.
	CPUSpeedInMHz = (int) i64Total;
}

CPUSpeed::~CPUSpeed ()
{
}

__int64 __cdecl CPUSpeed::GetCyclesDifference ( DELAY_FUNC DelayFunction, unsigned int uiParameter )
{
	unsigned int eax1 = 0, edx1 = 0, eax2 = 0, edx2 = 0;
#if defined(_WIN64)
	unsigned __int64 startTsc = __rdtsc();
	if (DelayFunction) DelayFunction(uiParameter);
	else Sleep(uiParameter);
	unsigned __int64 endTsc = __rdtsc();
	eax1 = (unsigned int)(startTsc & 0xFFFFFFFF);
	edx1 = (unsigned int)(startTsc >> 32);
	eax2 = (unsigned int)(endTsc & 0xFFFFFFFF);
	edx2 = (unsigned int)(endTsc >> 32);
#else
	__try {
		_asm {
			push uiParameter
			mov ebx, DelayFunction
			RDTSC_INSTRUCTION
			mov esi, eax
			mov edi, edx
			call ebx
			RDTSC_INSTRUCTION
			pop ebx
			mov edx2, edx
			mov eax2, eax
			mov edx1, edi
			mov eax1, esi
		}
	} __except ( 1 ) {
		return 0;
	}
#endif
	return (CPUSPEED_I32TO64 (edx2, eax2) - CPUSPEED_I32TO64 (edx1, eax1));
}

void CPUSpeed::Delay (unsigned int uiMS)
{
	LARGE_INTEGER Frequency, StartCounter, EndCounter;
	__int64 x;

	// Get the frequency of the high performance counter.
	if (!QueryPerformanceFrequency (&Frequency)) return;
	x = Frequency.QuadPart / 1000 * uiMS;

	// Get the starting position of the counter.
	QueryPerformanceCounter (&StartCounter);

	do {
		// Get the ending position of the counter.	
		QueryPerformanceCounter (&EndCounter);
	} while (EndCounter.QuadPart - StartCounter.QuadPart < x);
}

void CPUSpeed::DelayOverhead (unsigned int uiMS)
{
	LARGE_INTEGER Frequency, StartCounter, EndCounter;
	__int64 x;

	// Get the frequency of the high performance counter.
	if (!QueryPerformanceFrequency (&Frequency)) return;
	x = Frequency.QuadPart / 1000 * uiMS;

	// Get the starting position of the counter.
	QueryPerformanceCounter (&StartCounter);
	
	do {
		// Get the ending position of the counter.	
		QueryPerformanceCounter (&EndCounter);
	} while (EndCounter.QuadPart - StartCounter.QuadPart == x);
}
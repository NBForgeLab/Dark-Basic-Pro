#ifndef LMGLOBAL_H
#define LMGLOBAL_H

#include <windows.h>
#include <stdio.h>
#include <intrin.h>

#include "SharedData.h"

//#include <stdlib.h>
//#include <time.h>

extern SharedData *g_pShared;
extern int g_iLightmapFileFormat;

// x64 replacement for the legacy FLD/FISTP inline assembly. CVTSS2SI keeps
// the identical MXCSR round-to-nearest-even behaviour; a plain (int) cast
// would truncate instead and change lightmap pixel results.
inline int FtoI( float f )
{
	return _mm_cvtss_si32( _mm_set_ss( f ) );
}

#endif
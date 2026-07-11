#ifndef LMGLOBAL_H
#define LMGLOBAL_H

#include <windows.h>
#include <stdio.h>

#include "SharedData.h"

//#include <stdlib.h>
//#include <time.h>

extern SharedData *g_pShared;
extern int g_iLightmapFileFormat;

inline int FtoI( float f )
{
	return static_cast<int>(f);
}

#endif
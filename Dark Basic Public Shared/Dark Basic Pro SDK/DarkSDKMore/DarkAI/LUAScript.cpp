
#include "LUAScript.h"
#include <stdio.h>

extern "C" FILE* GG_fopen( const char* filename, const char* mode )
{
	FILE* fp = NULL;
	fopen_s(&fp, filename, mode);
	return fp;
}

extern "C" int GG_fopen_s( FILE** pFile, const char* filename, const char* mode )
{
	return fopen_s(pFile, filename, mode);
}

int waypoint_getmax(void) { return 0; }
int waypoint_ispointinzoneex(int iZone, float fX, float fY, float fZ, int iCheck) { (void)iZone; (void)fX; (void)fY; (void)fZ; (void)iCheck; return 0; }
void mp_refresh(void) {}
float GetLUATerrainHeightEx(float fX, float fZ) { (void)fX; (void)fZ; return 0.0f; }
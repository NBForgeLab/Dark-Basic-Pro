#ifndef _ENCRYPTEDFILE_H_
#define _ENCRYPTEDFILE_H_

#define _CRT_SECURE_NO_WARNINGS

#include <windows.h>
#include <cstdio>
#include <cstring>
#include <cstdint>

// Resolves an encrypted ("_e_") media variant of a virtual filename.
// Pure DBPro engine asset-protection feature; no Steam / Steam Workshop dependency.
inline bool CheckForWorkshopFile(LPSTR VirtualFilename)
{
	if (!VirtualFilename) return false;
	if (strlen(VirtualFilename) < 3) return false;

	char* tempCharPointerCheck = strrchr(VirtualFilename, '\\');
	if (tempCharPointerCheck == VirtualFilename + strlen(VirtualFilename) - 1) return false;
	if (VirtualFilename[0] == '.') return false;
	if (strstr(VirtualFilename, ".fpm")) return false;

	// encrypted file check
	char szEncryptedFilename[_MAX_PATH];
	char szEncryptedFilenameFolder[_MAX_PATH];
	strcpy(szEncryptedFilenameFolder, VirtualFilename);

	// replace forward slashes with backslash
	for (size_t c = 0; c < strlen(szEncryptedFilenameFolder); c++)
	{
		if (szEncryptedFilenameFolder[c] == '/')
			szEncryptedFilenameFolder[c] = '\\';
	}

	char* tempCharPointer = strrchr(szEncryptedFilenameFolder, '\\');
	if (tempCharPointer && tempCharPointer != szEncryptedFilenameFolder + strlen(szEncryptedFilenameFolder) - 1)
	{
		tempCharPointer[0] = 0;
		sprintf(szEncryptedFilename, "%s\\_e_%s", szEncryptedFilenameFolder, tempCharPointer + 1);
	}
	else
	{
		sprintf(szEncryptedFilename, "_e_%s", szEncryptedFilenameFolder);
	}
	FILE* tempFile = fopen(szEncryptedFilename, "r");
	if (tempFile)
	{
		fclose(tempFile);
		strcpy(VirtualFilename, szEncryptedFilename);
		return true;
	}

	return false;
}

#endif

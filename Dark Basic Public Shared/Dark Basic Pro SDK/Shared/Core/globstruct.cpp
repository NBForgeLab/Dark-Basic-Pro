//
// Glob Struct Functions (shared by all DLLs)
//

// Includes
#include <windows.h>
#include <cstdint>
#include <cstring>
#include "globstruct.h"

// Global Function Ptr to CreateString in Core ptr passed in
extern GlobStruct* g_pGlob;

// Checklist creation
void GlobExpandChecklist( DWORD iIndex, DWORD dwImminentStringSize )
{
	// First must make a checklist if new
	if(g_pGlob->checklist == nullptr)
	{
		// Create array of items
		g_pGlob->dwChecklistArraySize = 256;
		DWORD dwMemSize = g_pGlob->dwChecklistArraySize * sizeof(GlobChecklistStruct);
		g_pGlob->CreateDeleteString(reinterpret_cast<DWORD_PTR*>(&g_pGlob->checklist), dwMemSize);
		ZeroMemory(g_pGlob->checklist, sizeof(GlobChecklistStruct) * g_pGlob->dwChecklistArraySize);

		// Create strings for each item
		for(DWORD n=0; n<g_pGlob->dwChecklistArraySize; n++)
		{
			// Create default blank string
			g_pGlob->checklist[n].dwStringSize = 2;
			DWORD dwItemMemSize = g_pGlob->checklist[n].dwStringSize;
			g_pGlob->CreateDeleteString(reinterpret_cast<DWORD_PTR*>(&g_pGlob->checklist[n].string), dwItemMemSize);
			if (g_pGlob->checklist[n].string)
				strcpy_s(g_pGlob->checklist[n].string, dwItemMemSize, "");
		}
	}

	// Ensure checklist is big enough
	if(iIndex > g_pGlob->dwChecklistArraySize - 2)
	{
		// Double size of array
		DWORD dwArraySize = g_pGlob->dwChecklistArraySize * 2;

		// Make new larger checklist
		GlobChecklistStruct* pNewArray = nullptr;
		DWORD dwMemSize = dwArraySize * sizeof(GlobChecklistStruct);
		g_pGlob->CreateDeleteString(reinterpret_cast<DWORD_PTR*>(&pNewArray), dwMemSize);
		ZeroMemory(pNewArray, sizeof(GlobChecklistStruct) * dwArraySize);
		
		// Copy strings over to new array
		DWORD n = 0;
		for(n=0; n<g_pGlob->dwChecklistArraySize; n++)
		{
			// Create default blank string
			pNewArray[n].dwStringSize = g_pGlob->checklist[n].dwStringSize;
			pNewArray[n].string = g_pGlob->checklist[n].string;
		}

		// Create new strings
		for(; n<dwArraySize; n++)
		{
			// Create default blank string
			pNewArray[n].dwStringSize = 2;
			DWORD dwItemMemSize = pNewArray[n].dwStringSize;
			g_pGlob->CreateDeleteString(reinterpret_cast<DWORD_PTR*>(&pNewArray[n].string), dwItemMemSize);
			if (pNewArray[n].string)
				strcpy_s(pNewArray[n].string, dwItemMemSize, "");
		}

		// Transfer pointers and delete old one
		if(g_pGlob->checklist) g_pGlob->CreateDeleteString(reinterpret_cast<DWORD_PTR*>(&g_pGlob->checklist), 0);
		g_pGlob->dwChecklistArraySize = dwArraySize;
		g_pGlob->checklist = pNewArray;
	}

	// Ensure string being referenced is big enough
	if(dwImminentStringSize > g_pGlob->checklist[iIndex].dwStringSize)
	{
		// Expand string within checklist
		LPSTR pNewString = nullptr;
		DWORD dwMemSize = dwImminentStringSize + 1;
		g_pGlob->CreateDeleteString(reinterpret_cast<DWORD_PTR*>(&pNewString), dwMemSize);
		if (pNewString && g_pGlob->checklist[iIndex].string)
			strcpy_s(pNewString, dwMemSize, g_pGlob->checklist[iIndex].string);
		g_pGlob->checklist[iIndex].dwStringSize = dwImminentStringSize;
		g_pGlob->CreateDeleteString(reinterpret_cast<DWORD_PTR*>(&g_pGlob->checklist[iIndex].string), 0);
		g_pGlob->checklist[iIndex].string = pNewString;
	}
}

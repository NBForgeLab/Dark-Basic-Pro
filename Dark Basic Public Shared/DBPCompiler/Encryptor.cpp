// Encryptor.cpp: implementation of the CEncryptor class.
//
//////////////////////////////////////////////////////////////////////

//#include "stdafx.h"
#include "Encryptor.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CEncryptor::CEncryptor(DWORD dwUniqueKeyValue)
{
	m_dwUniqueKey=dwUniqueKeyValue;
}

CEncryptor::~CEncryptor()
{

}

bool CEncryptor::EncryptFileData(LPSTR filebuffer, DWORD filebuffersize, bool bEncryptIfTrue)
{
	DWORD dwUniqueKeyValue = m_dwUniqueKey;
	if(dwUniqueKeyValue>0)
	{
		// New key string
		char pNewKey[10];
		strcpy_s ( pNewKey, sizeof(pNewKey), m_szStringKey );
		pNewKey[9]=0;

		// Key Index
		size_t dwKeyIndex=0;
		size_t dwKeyMax=strlen(pNewKey);

		// Encrypt Data using Key (mess the file data up)
		size_t dwSpan = static_cast<size_t>(filebuffersize) / 1024;

		//Dave - span can be 0 with files under 1024 bytes
		if ( dwSpan < 1 ) dwSpan = 1;

		// Change a byte at each step point
		char* pPtr = filebuffer;
		char* pPtrEnd = filebuffer + filebuffersize;
		while(pPtr<pPtrEnd)
		{
			// Replacement keycode thats reliable
			int iKeyData = static_cast<int>(dwUniqueKeyValue % 64);

			// Modify byte (true=encrypt)
			for(int r=0; r<iKeyData; r++)
			{
				if(bEncryptIfTrue)
				{
					if(static_cast<unsigned char>(*pPtr)==255)
						*pPtr=0;
					else
						*pPtr+=1;
				}
				else
				{
					if(static_cast<unsigned char>(*pPtr)==0)
						*pPtr=static_cast<char>(255);
					else
						*pPtr-=1;
				}
			}

			// Next step in data
			pPtr+=dwSpan;

			// Move key index
			dwKeyIndex++;
			if(dwKeyIndex>=dwKeyMax) dwKeyIndex=0;
		}
	}

	// Complete
	return true;
}
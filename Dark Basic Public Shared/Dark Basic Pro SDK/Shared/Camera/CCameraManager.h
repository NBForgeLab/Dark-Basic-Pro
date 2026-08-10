#ifndef _CCAMERAMANAGER_H_
#define _CCAMERAMANAGER_H_

#include "..\Data\CDataC.h"
#include "CCameraData.h"

#pragma comment ( lib, "dxguid.lib" )
#pragma comment ( lib, "d3d8.lib"   )
#pragma comment ( lib, "d3dx8.lib"  )
#pragma comment ( lib, "dxerr8.lib" )

#include <d3d9.h>
#include <d3dx9.h>
#include <d3dx9tex.h>
#include <basetsd.h>
#include <stdio.h>
#include <math.h>

#define WIN32_LEAN_AND_MEAN 
#include <windows.h>   
#include <windowsx.h> 

class CCameraManager : private CData
{
	private:
		CData	m_List;	

	public:
		CCameraManager  ( );
		~CCameraManager ( );

		bool Add    ( tagCameraData* pData, int iID );
		bool Delete ( int iID );

		tagCameraData* GetData ( int iID );

		void Update ( void );
};

#endif _CCAMERAMANAGER_H_
#pragma once

// Standard Includes
#include <cstdint>
#include <cstddef>
#include "macros.h"

// Forward Declarations
class CError;
class ICodeGenerator;
class CDBMWriter;
class CStatementList;

// Parser Constants
inline constexpr uint32_t DBMPLACEMENT_TOP		= 1;
inline constexpr uint32_t DBMPLACEMENT_MIDDLE	= 2;
inline constexpr uint32_t DBMPLACEMENT_BOTTOM	= 3;

// External Class Pointers
extern CError*				g_pErrorReport;
extern ICodeGenerator*			g_pASMWriter;
extern CDBMWriter*			g_pDBMWriter;
extern CStatementList*		g_pStatementList;

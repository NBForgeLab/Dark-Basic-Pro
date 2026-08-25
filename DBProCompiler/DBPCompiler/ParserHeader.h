#pragma once

// Standard Includes
#include <cstdint>
#include <cstddef>
#include "macros.h"

// Custom Includes
#include "StatementList.h"
#include "ASMWriter.h"
#include "DBMWriter.h"
#include "Error.h"

// Parser Defines
#define DBMPLACEMENT_TOP		1
#define DBMPLACEMENT_MIDDLE		2
#define DBMPLACEMENT_BOTTOM		3

// External Class Pointers
extern CError*				g_pErrorReport;
extern ICodeGenerator*			g_pASMWriter;
extern CDBMWriter*			g_pDBMWriter;
extern CStatementList*		g_pStatementList;

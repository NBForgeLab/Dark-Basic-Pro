#include "resource.h"

//
// Common Macros
//

#if 0
#define SAFE_DELETE(x)	if(x) { delete x; x=nullptr; }
#define SAFE_CLOSE(x)	if(x) { CloseHandle(x); x=nullptr; }
#define SAFE_FREE(x)	if(x) { GlobalFree(x); x=nullptr; }
#else
//# include "../../Dark Basic Public Shared/Dark Basic Pro SDK/Shared/global.h"
#include "DB3.h"
#endif

#ifdef UNICODE
#undef GetPrivateProfileString
#define GetPrivateProfileString GetPrivateProfileStringA
#undef WritePrivateProfileString
#define WritePrivateProfileString WritePrivateProfileStringA
#undef wsprintf
#define wsprintf wsprintfA
#endif

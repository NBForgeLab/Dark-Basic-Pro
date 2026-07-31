#include "Tokenizer.h"
#include "Statement.h"
#include <cctype>
#include <cstring>

void CTokenizer::SetSourceBuffer(const char* pSourceBuffer) noexcept
{
    m_pSourceBuffer = pSourceBuffer;
    m_dwCurrentPos = 0;
}

void CTokenizer::SkipAllComments() noexcept
{
    if (m_pSourceBuffer == nullptr) return;

    while (m_pSourceBuffer[m_dwCurrentPos] != '\0')
    {
        char c = m_pSourceBuffer[m_dwCurrentPos];
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n')
        {
            m_dwCurrentPos++;
            continue;
        }

        // Check for REM or `//` comments
        if ((c == 'r' || c == 'R') &&
            (m_pSourceBuffer[m_dwCurrentPos + 1] == 'e' || m_pSourceBuffer[m_dwCurrentPos + 1] == 'E') &&
            (m_pSourceBuffer[m_dwCurrentPos + 2] == 'm' || m_pSourceBuffer[m_dwCurrentPos + 2] == 'M'))
        {
            char nextChar = m_pSourceBuffer[m_dwCurrentPos + 3];
            if (nextChar == ' ' || nextChar == '\t' || nextChar == ':' || nextChar == '\r' || nextChar == '\n' || nextChar == '\0')
            {
                SkipToCR();
                continue;
            }
        }

        if (c == '`' || (c == '/' && m_pSourceBuffer[m_dwCurrentPos + 1] == '/'))
        {
            SkipToCR();
            continue;
        }

        break;
    }
}

void CTokenizer::SkipToCR() noexcept
{
    if (m_pSourceBuffer == nullptr) return;

    while (m_pSourceBuffer[m_dwCurrentPos] != '\0' &&
           m_pSourceBuffer[m_dwCurrentPos] != '\r' &&
           m_pSourceBuffer[m_dwCurrentPos] != '\n')
    {
        m_dwCurrentPos++;
    }
}

void CTokenizer::SeekToSeperator() noexcept
{
    if (m_pSourceBuffer == nullptr) return;

    while (m_pSourceBuffer[m_dwCurrentPos] != '\0')
    {
        char c = m_pSourceBuffer[m_dwCurrentPos];
        if (c == ':' || c == ',' || c == '\r' || c == '\n')
        {
            break;
        }
        m_dwCurrentPos++;
    }
}

void CTokenizer::AdvancePastCRandSPACES() noexcept
{
    if (m_pSourceBuffer == nullptr) return;

    while (m_pSourceBuffer[m_dwCurrentPos] != '\0')
    {
        char c = m_pSourceBuffer[m_dwCurrentPos];
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n')
        {
            m_dwCurrentPos++;
        }
        else
        {
            break;
        }
    }
}

std::string CTokenizer::GetStringToEndOfLine()
{
    if (m_pSourceBuffer == nullptr) return "";

    std::string line;
    while (m_pSourceBuffer[m_dwCurrentPos] != '\0' &&
           m_pSourceBuffer[m_dwCurrentPos] != '\r' &&
           m_pSourceBuffer[m_dwCurrentPos] != '\n')
    {
        line += m_pSourceBuffer[m_dwCurrentPos];
        m_dwCurrentPos++;
    }
    return line;
}

int CTokenizer::DetermineNameToken(const char* pNameStr) const noexcept
{
    if (pNameStr == nullptr || pNameStr[0] == '\0') return 0;

    size_t len = std::strlen(pNameStr);
    char lastChar = pNameStr[len - 1];

    if (lastChar == '$') return 1; // String
    if (lastChar == '#') return 2; // Float
    return 3; // Integer or Default
}

DWORD CTokenizer::DetermineKeywordToken(const char* pToken) const noexcept
{
    if (pToken == nullptr || pToken[0] == '\0') return 0;

    // Loop keywords
    if (_stricmp(pToken, "DO") == 0) return static_cast<DWORD>(Token::Do);
    if (_stricmp(pToken, "LOOP") == 0) return static_cast<DWORD>(Token::Loop);
    if (_stricmp(pToken, "WHILE") == 0) return static_cast<DWORD>(Token::While);
    if (_stricmp(pToken, "ENDWHILE") == 0) return static_cast<DWORD>(Token::EndWhile);
    if (_stricmp(pToken, "REPEAT") == 0) return static_cast<DWORD>(Token::Repeat);
    if (_stricmp(pToken, "UNTIL") == 0) return static_cast<DWORD>(Token::Until);

    // For Next
    if (_stricmp(pToken, "FOR") == 0) return static_cast<DWORD>(Token::For);
    if (_stricmp(pToken, "NEXT") == 0) return static_cast<DWORD>(Token::Next);

    // Function
    if (_stricmp(pToken, "FUNCTION") == 0) return static_cast<DWORD>(Token::UserFunction);
    if (_stricmp(pToken, "EXITFUNCTION") == 0) return static_cast<DWORD>(Token::ExitUserFunction);
    if (_stricmp(pToken, "ENDFUNCTION") == 0) return static_cast<DWORD>(Token::EndUserFunction);

    // Jump & Control
    if (_stricmp(pToken, "EXIT") == 0) return static_cast<DWORD>(Token::Exit);
    if (_stricmp(pToken, "IF") == 0) return static_cast<DWORD>(Token::If);
    if (_stricmp(pToken, "ELSE") == 0) return static_cast<DWORD>(Token::Else);
    if (_stricmp(pToken, "ENDIF") == 0) return static_cast<DWORD>(Token::EndIf);
    if (_stricmp(pToken, "GOTO") == 0) return static_cast<DWORD>(Token::Goto);
    if (_stricmp(pToken, "GOSUB") == 0) return static_cast<DWORD>(Token::Gosub);
    if (_stricmp(pToken, "SELECT") == 0) return static_cast<DWORD>(Token::Select);
    if (_stricmp(pToken, "ENDSELECT") == 0) return static_cast<DWORD>(Token::EndSelect);
    if (_stricmp(pToken, "CASE") == 0) return static_cast<DWORD>(Token::Case);
    if (_stricmp(pToken, "ENDCASE") == 0) return static_cast<DWORD>(Token::EndCase);
    if (_stricmp(pToken, "END") == 0) return static_cast<DWORD>(Token::End);

    // Declaration & Types
    if (_stricmp(pToken, "TYPE") == 0) return static_cast<DWORD>(Token::Type);
    if (_stricmp(pToken, "ENDTYPE") == 0) return static_cast<DWORD>(Token::EndType);
    if (_stricmp(pToken, "GLOBAL") == 0) return static_cast<DWORD>(Token::Global);
    if (_stricmp(pToken, "LOCAL") == 0) return static_cast<DWORD>(Token::Local);
    if (_stricmp(pToken, "DIM") == 0) return static_cast<DWORD>(Token::Dim);
    if (_stricmp(pToken, "UNDIM") == 0) return static_cast<DWORD>(Token::Undim);
    if (_stricmp(pToken, "AS") == 0) return static_cast<DWORD>(Token::Asterisk);

    // Data types
    if (_stricmp(pToken, "BOOLEAN") == 0) return static_cast<DWORD>(Token::Boolean);
    if (_stricmp(pToken, "BYTE") == 0) return static_cast<DWORD>(Token::Byte);
    if (_stricmp(pToken, "WORD") == 0) return static_cast<DWORD>(Token::Word);
    if (_stricmp(pToken, "DWORD") == 0) return static_cast<DWORD>(Token::Dword);
    if (_stricmp(pToken, "INTEGER") == 0) return static_cast<DWORD>(Token::Integer);
    if (_stricmp(pToken, "FLOAT") == 0) return static_cast<DWORD>(Token::Float);
    if (_stricmp(pToken, "STRING") == 0) return static_cast<DWORD>(Token::String);
    if (_stricmp(pToken, "DOUBLE") == 0) return static_cast<DWORD>(Token::Double);

    // Comments
    if (_stricmp(pToken, "REMSTART") == 0) return static_cast<DWORD>(Token::RemStart);
    if (_stricmp(pToken, "REM") == 0) return static_cast<DWORD>(Token::RemLine);
    if (_stricmp(pToken, "//") == 0) return static_cast<DWORD>(Token::RemLine);
    if (_stricmp(pToken, "`") == 0) return static_cast<DWORD>(Token::RemLine);
    if (_stricmp(pToken, "'") == 0) return static_cast<DWORD>(Token::RemLine);
    if (_stricmp(pToken, "REMEND") == 0) return static_cast<DWORD>(Token::RemEnd);
    if (_stricmp(pToken, "HIDESTART") == 0) return static_cast<DWORD>(Token::RemLine);
    if (_stricmp(pToken, "HIDEEND") == 0) return static_cast<DWORD>(Token::RemLine);

    // Data
    if (_stricmp(pToken, "DATA") == 0) return static_cast<DWORD>(Token::Data);

    return 0;
}

bool CTokenizer::DetermineIfReservedWord(const char* pToken) const noexcept
{
    if (pToken == nullptr) return false;

    static const char* reservedKeywords[] = {
        "if", "then", "else", "endif", "do", "loop", "for", "next",
        "while", "wend", "repeat", "until", "function", "endfunction",
        "type", "endtype", "dim", "global", "local", "goto", "gosub", "return"
    };

    for (const char* kw : reservedKeywords)
    {
        if (_stricmp(pToken, kw) == 0)
        {
            return true;
        }
    }
    return false;
}

bool CTokenizer::DetermineIfFunctionName(const char* pToken) const noexcept
{
    if (pToken == nullptr || pToken[0] == '\0') return false;
    return (std::isalpha(static_cast<unsigned char>(pToken[0])) || pToken[0] == '_');
}

LPSTR CTokenizer::ProduceNextToken(LPSTR* pString, bool bIncrementLineNumber, bool bProduceCRTK, bool bIncludeCommas) const
{
    return ProduceNextTokenEx(pString, bIncrementLineNumber, bProduceCRTK, bIncludeCommas, false);
}

LPSTR CTokenizer::ProduceNextTokenEx(LPSTR* pString, bool bIncrementLineNumber, bool bProduceCRTK, bool bIncludeCommas, bool bIgnoreSpacesAroundEquateSymbol) const
{
	if (!pString || !*pString) return nullptr;

	char SepChars[33];
	for(unsigned int s=0; s<32; s++) SepChars[s]=1+s;
	SepChars[32]=0;

	if ( bIgnoreSpacesAroundEquateSymbol) SepChars[31]=31;

	LPSTR pFindStartOfToken=NULL;
	DWORD dwSpeechMark=0;
	bool bLineSeperatorFlag=false;
	bool bIncToAvoidRepeatCR=false;
	LPSTR pStringPointer = *pString;
	unsigned int len=static_cast<unsigned int>(strlen(SepChars));
	while(pStringPointer<g_pStatementList->GetFileDataEnd())
	{
		bool bFlag=false;
		for(unsigned int n=0; n<len; n++)
		{
			if(*(unsigned char*)pStringPointer==(unsigned char)SepChars[n])
				bFlag=true;
		}
		if(*(unsigned char*)(pStringPointer+0)==13
		&& *(unsigned char*)(pStringPointer+1)==10)
			bLineSeperatorFlag=true;

		if ( bIgnoreSpacesAroundEquateSymbol && dwSpeechMark==0 )
		{
			bool bSpaceIsAllowed=false;
			if(*(unsigned char*)pStringPointer==' ')
			{
				for ( LPSTR pChk=pStringPointer-1; pChk>=*pString; pChk-- )
				{
					if ( *(unsigned char*)pChk!=' ' && *(unsigned char*)pChk!='=' )
						break;

					if ( *(unsigned char*)pChk=='=' )
					{
						bSpaceIsAllowed=true;
						break;
					}
				}
				if ( !bSpaceIsAllowed )
				{
					for ( LPSTR pChk=pStringPointer+1; pChk<g_pStatementList->GetFileDataEnd(); pChk++ )
					{
						if ( *(unsigned char*)pChk!=' ' && *(unsigned char*)pChk!='=' )
							break;

						if ( *(unsigned char*)pChk=='=' )
						{
							bSpaceIsAllowed=true;
							break;
						}
					}
				}
				if ( bSpaceIsAllowed==false )
					bFlag=true;
			}
		}

		if(pStringPointer>=g_pStatementList->GetFileDataEnd())
			break;

		if(*(unsigned char*)(pStringPointer+0)=='"') dwSpeechMark=1-dwSpeechMark;

		bool bQuickQuit=false;
		if(dwSpeechMark==0)
		{
			if(*(unsigned char*)(pStringPointer+0)==':')
			{
				if(pFindStartOfToken==NULL)
					bQuickQuit=true;
				else
					break;
			}
		}

		if(pFindStartOfToken==NULL)
			if(pStringPointer>=g_pStatementList->GetFileDataEnd()-1)
				bQuickQuit=true;

		if(bQuickQuit==true)
		{
			LPSTR pProducedToken = new char[3];
			pProducedToken[0]=13;
			pProducedToken[1]=10;
			pProducedToken[2]=0;

			pStringPointer+=1;
			*pString = pStringPointer;

			return pProducedToken;
		}

		if(dwSpeechMark==0)
			if(bIncludeCommas==true && *(unsigned char*)(pStringPointer+0)==',')
				bFlag=true;

		if(dwSpeechMark==0)
			if(*(unsigned char*)(pStringPointer+0)=='`')
			{
				pFindStartOfToken=pStringPointer;
				pStringPointer++;
				if(*(unsigned char*)(pStringPointer+0)==13) bLineSeperatorFlag=true;
				break;
			}

		if(bLineSeperatorFlag==true)
		{
			if(bProduceCRTK && pStringPointer>(*pString))
			{
				if(pFindStartOfToken==NULL)
				{
					LPSTR pProducedToken = new char[3];
					pProducedToken[0]=13;
					pProducedToken[1]=10;
					pProducedToken[2]=0;
					return pProducedToken;
				}
				else
					break;
			}

			if(bProduceCRTK && pStringPointer==(*pString))
			{
				if(bIncrementLineNumber)
					g_pStatementList->IncLineNumber();

				LPSTR pProducedToken = new char[3];
				pProducedToken[0]=13;
				pProducedToken[1]=10;
				pProducedToken[2]=0;

				pStringPointer+=2;
				*pString = pStringPointer;

				return pProducedToken;
			}

			if(bIncrementLineNumber)
			{
				if(bIncrementLineNumber)
				{
					g_pStatementList->IncLineNumber();
					bIncToAvoidRepeatCR = true;
				}
				bLineSeperatorFlag = false;
				if ( pFindStartOfToken!=NULL )
					break;
			}
		}

		if(pFindStartOfToken==NULL)
		{
			if(bFlag==false)
			{
				g_pStatementList->SetTokenLineNumber(g_pStatementList->GetLineNumber());
				g_pStatementList->SetLastCharInDataPosition((DWORD)(pStringPointer-g_pStatementList->GetFileDataStart()));
				pFindStartOfToken=pStringPointer;
			}
		}
		else
		{
			if(bFlag==true) break;
		}
		pStringPointer++;
	}

	LPSTR pProducedToken = NULL;
	if(pFindStartOfToken!=NULL)
	{
		unsigned int length = pStringPointer-pFindStartOfToken;
		if(length>0)
		{
			pProducedToken = new char[length+1];
			memcpy(pProducedToken, pFindStartOfToken, length+1);
			pProducedToken[length]=0;
		}
	}
	if ( bIncToAvoidRepeatCR ) pStringPointer++;
	*pString = pStringPointer;

	return pProducedToken;
}

LPSTR CTokenizer::ProduceNextArrayToken(LPSTR* pOrigPointer) const
{
	int iBracketCount=0;
	DWORD dwSpeechMarks=0;
	LPSTR pPointer=*pOrigPointer;
	LPSTR pStart=pPointer;
	LPSTR pEndOfBracket=NULL;
	DWORD dwInitGetStage=0;
	while(true)
	{
		if(*(unsigned char*)(pPointer+0)==13
		&& *(unsigned char*)(pPointer+1)==10)
			break;

		if(pPointer>=g_pStatementList->GetFileDataEnd())
			break;

		if(*(unsigned char*)pPointer=='"') dwSpeechMarks=1-dwSpeechMarks;

		if(pEndOfBracket==NULL)
		{
			if(*(unsigned char*)pPointer=='(') iBracketCount++;
			if(iBracketCount>0)
			{
				if(*(unsigned char*)pPointer==')') iBracketCount--;
				if(iBracketCount==0)
				{
					pPointer++;
					pEndOfBracket=pPointer;
				}
			}
		}
		if(pEndOfBracket)
		{
			if(dwInitGetStage==0)
			{
				if(*(unsigned char*)pPointer=='=')
				{
					dwInitGetStage=1;
				}
				else
				{
					if(*(unsigned char*)pPointer==' ')
					{
					}
					else
					{
						break;
					}
				}
			}
			if(dwInitGetStage==1)
			{
				if(*(unsigned char*)pPointer!=' ') dwInitGetStage=2;
			}
			if(dwInitGetStage==2)
			{
				if(*(unsigned char*)pPointer==' ')
					break;
			}
		}

		if(dwSpeechMarks==0)
			if(*(unsigned char*)pPointer==':')
				break;

		pPointer++;
	}

	if(dwInitGetStage>0)
	{
	}
	else
		pPointer=pEndOfBracket;

	if(pPointer)
	{
		unsigned int length = pPointer-pStart;
		if(length>0)
		{
			LPSTR pProduceLine = new char[length+1];
			memcpy(pProduceLine, pStart, length+1);
			pProduceLine[length]=0;
			*pOrigPointer=pPointer;
			return pProduceLine;
		}
	}

	return NULL;
}

LPSTR CTokenizer::ProduceFullSegment(LPSTR* pOrigPointer) const
{
	DWORD dwSpeechMarks=0;
	LPSTR pPointer=*pOrigPointer;
	LPSTR pStart=pPointer;
	while(true)
	{
		if(*(unsigned char*)(pPointer+0)==13
		&& *(unsigned char*)(pPointer+1)==10)
			break;

		if(pPointer>=g_pStatementList->GetFileDataEnd())
			break;

		if(*(unsigned char*)pPointer=='"') dwSpeechMarks=1-dwSpeechMarks;
		if(dwSpeechMarks==0)
			if(*(unsigned char*)pPointer==':')
				break;

		pPointer++;
	}

	unsigned int length = pPointer-pStart;
	if(length>0)
	{
		LPSTR pProduceLine = new char[length+1];
		memcpy(pProduceLine, pStart, length+1);
		pProduceLine[length]=0;
		*pOrigPointer=pPointer;
		return pProduceLine;
	}

	return NULL;
}

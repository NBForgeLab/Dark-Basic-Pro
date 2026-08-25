#include "ParserHeader.h"
#include "StringUtils.h"
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

int CTokenizer::DetermineNameToken(std::string_view nameStr) const noexcept
{
    if (nameStr.empty()) return 0;

    char lastChar = nameStr.back();

    if (lastChar == '$') return 1; // String
    if (lastChar == '#') return 2; // Float
    return 3; // Integer or Default
}

DWORD CTokenizer::DetermineKeywordToken(std::string_view token) const noexcept
{
    if (token.empty()) return 0;

    // Loop keywords
    if (dbp::iequals(token, "DO")) return static_cast<DWORD>(Token::Do);
    if (dbp::iequals(token, "LOOP")) return static_cast<DWORD>(Token::Loop);
    if (dbp::iequals(token, "WHILE")) return static_cast<DWORD>(Token::While);
    if (dbp::iequals(token, "ENDWHILE")) return static_cast<DWORD>(Token::EndWhile);
    if (dbp::iequals(token, "REPEAT")) return static_cast<DWORD>(Token::Repeat);
    if (dbp::iequals(token, "UNTIL")) return static_cast<DWORD>(Token::Until);

    // For Next
    if (dbp::iequals(token, "FOR")) return static_cast<DWORD>(Token::For);
    if (dbp::iequals(token, "NEXT")) return static_cast<DWORD>(Token::Next);

    // Function
    if (dbp::iequals(token, "FUNCTION")) return static_cast<DWORD>(Token::UserFunction);
    if (dbp::iequals(token, "EXITFUNCTION")) return static_cast<DWORD>(Token::ExitUserFunction);
    if (dbp::iequals(token, "ENDFUNCTION")) return static_cast<DWORD>(Token::EndUserFunction);

    // Jump & Control
    if (dbp::iequals(token, "EXIT")) return static_cast<DWORD>(Token::Exit);
    if (dbp::iequals(token, "IF")) return static_cast<DWORD>(Token::If);
    if (dbp::iequals(token, "ELSE")) return static_cast<DWORD>(Token::Else);
    if (dbp::iequals(token, "ENDIF")) return static_cast<DWORD>(Token::EndIf);
    if (dbp::iequals(token, "GOTO")) return static_cast<DWORD>(Token::Goto);
    if (dbp::iequals(token, "GOSUB")) return static_cast<DWORD>(Token::Gosub);
    if (dbp::iequals(token, "SELECT")) return static_cast<DWORD>(Token::Select);
    if (dbp::iequals(token, "ENDSELECT")) return static_cast<DWORD>(Token::EndSelect);
    if (dbp::iequals(token, "CASE")) return static_cast<DWORD>(Token::Case);
    if (dbp::iequals(token, "ENDCASE")) return static_cast<DWORD>(Token::EndCase);
    if (dbp::iequals(token, "END")) return static_cast<DWORD>(Token::End);

    // Declaration & Types
    if (dbp::iequals(token, "TYPE")) return static_cast<DWORD>(Token::Type);
    if (dbp::iequals(token, "ENDTYPE")) return static_cast<DWORD>(Token::EndType);
    if (dbp::iequals(token, "GLOBAL")) return static_cast<DWORD>(Token::Global);
    if (dbp::iequals(token, "LOCAL")) return static_cast<DWORD>(Token::Local);
    if (dbp::iequals(token, "DIM")) return static_cast<DWORD>(Token::Dim);
    if (dbp::iequals(token, "UNDIM")) return static_cast<DWORD>(Token::Undim);
    if (dbp::iequals(token, "AS")) return static_cast<DWORD>(Token::Asterisk);

    // Data types
    if (dbp::iequals(token, "BOOLEAN")) return static_cast<DWORD>(Token::Boolean);
    if (dbp::iequals(token, "BYTE")) return static_cast<DWORD>(Token::Byte);
    if (dbp::iequals(token, "WORD")) return static_cast<DWORD>(Token::Word);
    if (dbp::iequals(token, "DWORD")) return static_cast<DWORD>(Token::Dword);
    if (dbp::iequals(token, "INTEGER")) return static_cast<DWORD>(Token::Integer);
    if (dbp::iequals(token, "FLOAT")) return static_cast<DWORD>(Token::Float);
    if (dbp::iequals(token, "STRING")) return static_cast<DWORD>(Token::String);
    if (dbp::iequals(token, "DOUBLE")) return static_cast<DWORD>(Token::Double);

    // Comments
    if (dbp::iequals(token, "REMSTART")) return static_cast<DWORD>(Token::RemStart);
    if (dbp::iequals(token, "REM")) return static_cast<DWORD>(Token::RemLine);
    if (dbp::iequals(token, "//")) return static_cast<DWORD>(Token::RemLine);
    if (dbp::iequals(token, "`")) return static_cast<DWORD>(Token::RemLine);
    if (dbp::iequals(token, "'")) return static_cast<DWORD>(Token::RemLine);
    if (dbp::iequals(token, "REMEND")) return static_cast<DWORD>(Token::RemEnd);
    if (dbp::iequals(token, "HIDESTART")) return static_cast<DWORD>(Token::RemLine);
    if (dbp::iequals(token, "HIDEEND")) return static_cast<DWORD>(Token::RemLine);

    // Data
    if (dbp::iequals(token, "DATA")) return static_cast<DWORD>(Token::Data);

    return 0;
}

bool CTokenizer::DetermineIfReservedWord(std::string_view token) const noexcept
{
    if (token.empty()) return false;

    static const char* reservedKeywords[] = {
        "if", "then", "else", "endif", "do", "loop", "for", "next",
        "while", "wend", "repeat", "until", "function", "endfunction",
        "type", "endtype", "dim", "global", "local", "goto", "gosub", "return"
    };

    for (const char* kw : reservedKeywords)
    {
        if (dbp::iequals(token, kw))
        {
            return true;
        }
    }
    return false;
}

bool CTokenizer::DetermineIfFunctionName(std::string_view token) const noexcept
{
    if (token.empty()) return false;
    return (std::isalpha(static_cast<unsigned char>(static_cast<unsigned char>(token.front()))) || token.front() == '_');
}

LPSTR CTokenizer::ProduceNextToken(LPSTR* pString, bool bIncrementLineNumber, bool bProduceCRTK, bool bIncludeCommas) const
{
    return ProduceNextTokenEx(pString, bIncrementLineNumber, bProduceCRTK, bIncludeCommas, false);
}

LPSTR CTokenizer::ProduceNextTokenEx(LPSTR* pString, bool bIncrementLineNumber, bool bProduceCRTK, bool bIncludeCommas, bool bIgnoreSpacesAroundEquateSymbol) const
{
	if (!pString || !*pString) return nullptr;

	char SepChars[33];
	for(unsigned int s=0; s<32; s++) SepChars[s]=static_cast<char>(1+s);
	SepChars[32]=0;

	if ( bIgnoreSpacesAroundEquateSymbol) SepChars[31]=31;

	LPSTR pFindStartOfToken=nullptr;
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
				if(pFindStartOfToken==nullptr)
					bQuickQuit=true;
				else
					break;
			}
		}

		if(pFindStartOfToken==nullptr)
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
				if(pFindStartOfToken==nullptr)
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
				if ( pFindStartOfToken!=nullptr )
					break;
			}
		}

		if(pFindStartOfToken==nullptr)
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

	LPSTR pProducedToken = nullptr;
	if(pFindStartOfToken!=nullptr)
	{
		unsigned int length = static_cast<unsigned int>(pStringPointer-pFindStartOfToken);
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
	LPSTR pEndOfBracket=nullptr;
	DWORD dwInitGetStage=0;
	while(true)
	{
		if(*(unsigned char*)(pPointer+0)==13
		&& *(unsigned char*)(pPointer+1)==10)
			break;

		if(pPointer>=g_pStatementList->GetFileDataEnd())
			break;

		if(*(unsigned char*)pPointer=='"') dwSpeechMarks=1-dwSpeechMarks;

		if(pEndOfBracket==nullptr)
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
		unsigned int length = static_cast<unsigned int>(pPointer-pStart);
		if(length>0)
		{
			LPSTR pProduceLine = new char[length+1];
			memcpy(pProduceLine, pStart, length+1);
			pProduceLine[length]=0;
			*pOrigPointer=pPointer;
			return pProduceLine;
		}
	}

	return nullptr;
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

	unsigned int length = static_cast<unsigned int>(pPointer-pStart);
	if(length>0)
	{
		LPSTR pProduceLine = new char[length+1];
		memcpy(pProduceLine, pStart, length+1);
		pProduceLine[length]=0;
		*pOrigPointer=pPointer;
		return pProduceLine;
	}

	return nullptr;
}

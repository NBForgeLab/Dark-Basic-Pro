// MachineCodeBuffer.cpp: implementation of the CMachineCodeBuffer class.
// Handles raw x86 machine code storage, low-level emission, and buffer expansion.
//////////////////////////////////////////////////////////////////////

#include "MachineCodeBuffer.h"
#include <cstring>

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CMachineCodeBuffer::CMachineCodeBuffer()
{
	m_dwMCBlockSize = 0;
	m_pProgramStart = NULL;
	m_pMachineBlock = NULL;
}

CMachineCodeBuffer::~CMachineCodeBuffer()
{
}

//////////////////////////////////////////////////////////////////////
// Buffer Management
//////////////////////////////////////////////////////////////////////

bool CMachineCodeBuffer::Initialize(DWORD dwInitialSize)
{
	m_dwMCBlockSize = dwInitialSize;
	m_machineCodeStorage.assign(m_dwMCBlockSize, (char)0xC3);
	m_pProgramStart = m_machineCodeStorage.data();
	m_pMachineBlock = m_pProgramStart;
	return true;
}

bool CMachineCodeBuffer::CheckAndExpandMCBMemory()
{
	// If within 100 bytes of end, expand memory
	LPSTR pMCBDataBarrier = (m_pProgramStart + m_dwMCBlockSize) - EXPANSION_THRESHOLD;
	if (m_pMachineBlock > pMCBDataBarrier)
	{
		// Work out offset of pointer
		DWORD dwOffset = m_pMachineBlock - m_pProgramStart;

		// Expand memory (another 100K) via vector resize
		DWORD dwNewSize = m_dwMCBlockSize + EXPANSION_CHUNK;
		DWORD dwOldSize = m_dwMCBlockSize;
		m_machineCodeStorage.resize(dwNewSize);
		// Fill new portion with RET codes
		memset(m_machineCodeStorage.data() + dwOldSize, 0xC3, dwNewSize - dwOldSize);

		// Rereference to new memory
		m_dwMCBlockSize = dwNewSize;
		m_pProgramStart = m_machineCodeStorage.data();
		m_pMachineBlock = m_pProgramStart + dwOffset;

		// Mem was expanded
		return true;
	}

	// Did not expand
	return false;
}

DWORD CMachineCodeBuffer::GetCurrentMCPosition() const
{
	return m_pMachineBlock - m_pProgramStart;
}

DWORD CMachineCodeBuffer::GetBytePosOfLastInstruction() const
{
	return m_pMachineBlock - m_pProgramStart;
}

void CMachineCodeBuffer::FreeMachineBlock()
{
	m_machineCodeStorage.clear();
	m_pProgramStart = NULL;
	m_pMachineBlock = NULL;
	m_dwMCBlockSize = 0;
}

//////////////////////////////////////////////////////////////////////
// Low-Level Write Operations
//////////////////////////////////////////////////////////////////////

void CMachineCodeBuffer::WriteByte(int byte)
{
	*(m_pMachineBlock) = byte;
	(m_pMachineBlock)++;
}

void CMachineCodeBuffer::WriteDWORD(DWORD value, DWORD dwSize)
{
	*(DWORD*)(m_pMachineBlock) = value;
	(m_pMachineBlock) += dwSize;
}

// MachineCodeBuffer.cpp: implementation of the CMachineCodeBuffer class.
// Handles raw x86 machine code storage, low-level emission, and buffer expansion.
//////////////////////////////////////////////////////////////////////

#include "MachineCodeBuffer.h"
#include <cstring>

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CMachineCodeBuffer::CMachineCodeBuffer() = default;

CMachineCodeBuffer::~CMachineCodeBuffer() = default;

//////////////////////////////////////////////////////////////////////
// Buffer Management
//////////////////////////////////////////////////////////////////////

bool CMachineCodeBuffer::Initialize(DWORD dwInitialSize)
{
	m_dwMCBlockSize = dwInitialSize;
	m_machineCodeStorage.assign(m_dwMCBlockSize, static_cast<char>(0xC3));
	m_pProgramStart = m_machineCodeStorage.data();
	m_pMachineBlock = m_pProgramStart;
	return true;
}

bool CMachineCodeBuffer::CheckAndExpandMCBMemory()
{
	if (!m_pProgramStart || !m_pMachineBlock)
		return false;

	const auto usedSize = static_cast<size_t>(m_pMachineBlock - m_pProgramStart);
	if (usedSize > m_machineCodeStorage.size())
		return false;

	// If fewer than 100 bytes remain, expand memory.
	if (m_machineCodeStorage.size() - usedSize < EXPANSION_THRESHOLD)
	{
		// Work out offset of pointer
		DWORD dwOffset = static_cast<DWORD>(usedSize);

		// Expand memory (another 100K) via vector resize
		DWORD dwNewSize = m_dwMCBlockSize + EXPANSION_CHUNK;
		DWORD dwOldSize = m_dwMCBlockSize;
		m_machineCodeStorage.resize(dwNewSize);
		// Fill new portion with RET codes (0xC3)
		std::memset(m_machineCodeStorage.data() + dwOldSize, 0xC3, dwNewSize - dwOldSize);

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

DWORD CMachineCodeBuffer::GetCurrentMCPosition() const noexcept
{
	return m_pProgramStart ? static_cast<DWORD>(m_pMachineBlock - m_pProgramStart) : 0;
}

DWORD CMachineCodeBuffer::GetBytePosOfLastInstruction() const noexcept
{
	return GetCurrentMCPosition();
}

void CMachineCodeBuffer::FreeMachineBlock() noexcept
{
	m_machineCodeStorage.clear();
	m_pProgramStart = nullptr;
	m_pMachineBlock = nullptr;
	m_dwMCBlockSize = 0;
}

//////////////////////////////////////////////////////////////////////
// Low-Level Write Operations
//////////////////////////////////////////////////////////////////////

void CMachineCodeBuffer::WriteByte(int byte)
{
	if (CanWrite(sizeof(char)))
	{
		*(m_pMachineBlock) = static_cast<char>(byte);
		(m_pMachineBlock)++;
	}
}

void CMachineCodeBuffer::WriteDWORD(DWORD value, DWORD dwSize)
{
	if (dwSize == 0 || dwSize > sizeof(value) || !CanWrite(dwSize))
		return;

	std::memcpy(m_pMachineBlock, &value, dwSize);
	m_pMachineBlock += dwSize;
}

bool CMachineCodeBuffer::CanWrite(size_t byteCount) const noexcept
{
	if (!m_pProgramStart || !m_pMachineBlock)
		return false;

	const auto usedSize = static_cast<size_t>(m_pMachineBlock - m_pProgramStart);
	return usedSize <= m_machineCodeStorage.size()
		&& byteCount <= m_machineCodeStorage.size() - usedSize;
}

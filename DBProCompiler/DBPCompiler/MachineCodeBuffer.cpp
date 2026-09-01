// MachineCodeBuffer.cpp: implementation of the CMachineCodeBuffer class.
// Handles raw x86 machine code storage, low-level emission, and buffer expansion.
//////////////////////////////////////////////////////////////////////

#include "MachineCodeBuffer.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace
{
// Optional raw emission audit channel: set DBP_TRACE_BYTES=1 to record every
// emitted machine-code byte (offset + hex) to mcb_trace.txt in the compiler's
// working directory. Used to audit ABI marshalling bytes for stack balance.
class McbByteTrace
{
public:
	static McbByteTrace& Get() noexcept
	{
		static McbByteTrace instance;
		return instance;
	}

	static void Emit(DWORD offset, const void* bytes, size_t count) noexcept
	{
		McbByteTrace& self = Get();
		if (!self.m_file) return;
		std::fprintf(self.m_file, "%08X:", static_cast<unsigned>(offset));
		const auto* p = static_cast<const uint8_t*>(bytes);
		for (size_t i = 0; i < count; ++i)
			std::fprintf(self.m_file, " %02X", p[i]);
		std::fprintf(self.m_file, "\n");
	}

	static void Emit(DWORD offset, uint8_t byte) noexcept
	{
		Emit(offset, &byte, 1);
	}

private:
	McbByteTrace() noexcept
	{
		const char* env = std::getenv("DBP_TRACE_BYTES");
		if (env && env[0] != '\0')
			m_file = std::fopen("mcb_trace.txt", "w");
	}

	~McbByteTrace() noexcept
	{
		if (m_file) std::fclose(m_file);
	}

	std::FILE* m_file = nullptr;
};
} // namespace

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
		DWORD offset = GetCurrentMCPosition();
		*(m_pMachineBlock) = static_cast<char>(byte);
		(m_pMachineBlock)++;
		McbByteTrace::Emit(offset, static_cast<uint8_t>(byte));
	}
}

void CMachineCodeBuffer::WriteDWORD(DWORD value, DWORD dwSize)
{
	if (dwSize == 0 || dwSize > sizeof(value) || !CanWrite(dwSize))
		return;

	DWORD offset = GetCurrentMCPosition();
	std::memcpy(m_pMachineBlock, &value, dwSize);
	m_pMachineBlock += dwSize;
	McbByteTrace::Emit(offset, &value, dwSize);
}

void CMachineCodeBuffer::WriteQWORD(uint64_t value, DWORD dwSize)
{
	if (dwSize == 0 || dwSize > sizeof(value) || !CanWrite(dwSize))
		return;

	DWORD offset = GetCurrentMCPosition();
	std::memcpy(m_pMachineBlock, &value, dwSize);
	m_pMachineBlock += dwSize;
	McbByteTrace::Emit(offset, &value, dwSize);
}

bool CMachineCodeBuffer::CanWrite(size_t byteCount) const noexcept
{
	if (!m_pProgramStart || !m_pMachineBlock)
		return false;

	const auto usedSize = static_cast<size_t>(m_pMachineBlock - m_pProgramStart);
	return usedSize <= m_machineCodeStorage.size()
		&& byteCount <= m_machineCodeStorage.size() - usedSize;
}

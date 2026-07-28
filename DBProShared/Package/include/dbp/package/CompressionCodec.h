#pragma once

#include "dbp/package/PackageError.h"
#include "dbp/package/PackageFormat.h"

#include <cstddef>
#include <cstdint>
#include <istream>
#include <ostream>
#include <vector>

namespace dbp::package {

struct CompressionStats {
    std::uint64_t inputSize = 0;
    std::uint64_t outputSize = 0;
};

struct CompressedBuffer {
    CompressionAlgorithm algorithm = CompressionAlgorithm::None;
    std::vector<std::uint8_t> bytes;
};

class ZstdCompressionCodec {
public:
    PackageResult<CompressedBuffer> CompressIfSmaller(
        const std::vector<std::uint8_t>& plaintext,
        int compressionLevel = 3) const;

    PackageResult<std::vector<std::uint8_t>> Decompress(
        const CompressedBuffer& stored,
        std::uint64_t expectedPlaintextSize,
        std::uint64_t maximumPlaintextSize) const;

    PackageResult<CompressionStats> CompressStream(
        std::istream& input,
        std::ostream& output,
        int compressionLevel = 3) const;

    PackageResult<CompressionStats> DecompressStream(
        std::istream& input,
        std::ostream& output,
        std::uint64_t expectedPlaintextSize,
        std::uint64_t maximumPlaintextSize) const;
};

} // namespace dbp::package

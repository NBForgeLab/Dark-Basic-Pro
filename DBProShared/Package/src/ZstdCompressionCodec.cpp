#include "dbp/package/CompressionCodec.h"

#include "dbp/package/ByteCodec.h"

#include <zstd.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace dbp::package {

namespace {

constexpr std::size_t streamBufferSize = 1024U * 1024U;

PackageError CompressionError(std::string message) {
    return {
        PackageErrorCode::CompressionFailed,
        std::move(message),
        std::nullopt,
    };
}

PackageError StreamError(std::string message) {
    return {
        PackageErrorCode::IoFailed,
        std::move(message),
        std::nullopt,
    };
}

PackageResult<std::uint64_t> AddSize(
    const std::uint64_t current,
    const std::size_t amount,
    const char* const operation) {
    const auto result = CheckedAdd(current, amount);
    if (!result) {
        return PackageResult<std::uint64_t>::Failure(
            CompressionError(
                std::string(operation) + " size overflowed."));
    }
    return result;
}

bool WriteChunk(
    std::ostream& output,
    const std::uint8_t* const bytes,
    const std::size_t size) {
    if (size == 0) {
        return true;
    }
    output.write(
        reinterpret_cast<const char*>(bytes),
        static_cast<std::streamsize>(size));
    return static_cast<bool>(output);
}

} // namespace

PackageResult<CompressionStats> ZstdCompressionCodec::CompressStream(
    std::istream& input,
    std::ostream& output,
    const int compressionLevel) const {
    using Context = std::unique_ptr<ZSTD_CCtx, decltype(&ZSTD_freeCCtx)>;
    Context context(ZSTD_createCCtx(), &ZSTD_freeCCtx);
    if (!context) {
        return PackageResult<CompressionStats>::Failure(
            CompressionError("Zstandard compression context allocation failed."));
    }

    std::size_t status = ZSTD_CCtx_setParameter(
        context.get(),
        ZSTD_c_compressionLevel,
        compressionLevel);
    if (ZSTD_isError(status)) {
        return PackageResult<CompressionStats>::Failure(
            CompressionError(ZSTD_getErrorName(status)));
    }
    status = ZSTD_CCtx_setParameter(
        context.get(),
        ZSTD_c_checksumFlag,
        1);
    if (ZSTD_isError(status)) {
        return PackageResult<CompressionStats>::Failure(
            CompressionError(ZSTD_getErrorName(status)));
    }
    status = ZSTD_CCtx_setParameter(
        context.get(),
        ZSTD_c_contentSizeFlag,
        0);
    if (ZSTD_isError(status)) {
        return PackageResult<CompressionStats>::Failure(
            CompressionError(ZSTD_getErrorName(status)));
    }

    std::vector<std::uint8_t> inputBuffer(streamBufferSize);
    std::vector<std::uint8_t> outputBuffer(streamBufferSize);
    CompressionStats stats;
    bool finishedInput = false;
    while (!finishedInput) {
        input.read(
            reinterpret_cast<char*>(inputBuffer.data()),
            static_cast<std::streamsize>(inputBuffer.size()));
        const auto bytesRead = input.gcount();
        if (bytesRead < 0 || input.bad()) {
            return PackageResult<CompressionStats>::Failure(
                StreamError("Reading the compression input failed."));
        }
        finishedInput = input.eof();

        const auto nextInputSize = AddSize(
            stats.inputSize,
            static_cast<std::size_t>(bytesRead),
            "Compression input");
        if (!nextInputSize) {
            return PackageResult<CompressionStats>::Failure(
                nextInputSize.error());
        }
        stats.inputSize = nextInputSize.value();

        ZSTD_inBuffer source{
            inputBuffer.data(),
            static_cast<std::size_t>(bytesRead),
            0,
        };
        const ZSTD_EndDirective directive =
            finishedInput ? ZSTD_e_end : ZSTD_e_continue;
        std::size_t remaining = 1;
        do {
            ZSTD_outBuffer destination{
                outputBuffer.data(),
                outputBuffer.size(),
                0,
            };
            remaining = ZSTD_compressStream2(
                context.get(),
                &destination,
                &source,
                directive);
            if (ZSTD_isError(remaining)) {
                return PackageResult<CompressionStats>::Failure(
                    CompressionError(ZSTD_getErrorName(remaining)));
            }
            if (!WriteChunk(
                    output,
                    outputBuffer.data(),
                    destination.pos)) {
                return PackageResult<CompressionStats>::Failure(
                    StreamError("Writing compressed output failed."));
            }
            const auto nextOutputSize = AddSize(
                stats.outputSize,
                destination.pos,
                "Compression output");
            if (!nextOutputSize) {
                return PackageResult<CompressionStats>::Failure(
                    nextOutputSize.error());
            }
            stats.outputSize = nextOutputSize.value();
        } while (source.pos < source.size ||
                 (finishedInput && remaining != 0));
    }

    output.flush();
    if (!output) {
        return PackageResult<CompressionStats>::Failure(
            StreamError("Flushing compressed output failed."));
    }
    return PackageResult<CompressionStats>::Success(stats);
}

PackageResult<CompressionStats> ZstdCompressionCodec::DecompressStream(
    std::istream& input,
    std::ostream& output,
    const std::uint64_t expectedPlaintextSize,
    const std::uint64_t maximumPlaintextSize) const {
    if (expectedPlaintextSize > maximumPlaintextSize) {
        return PackageResult<CompressionStats>::Failure(
            CompressionError(
                "Declared plaintext size exceeds the decompression limit."));
    }

    using Context = std::unique_ptr<ZSTD_DCtx, decltype(&ZSTD_freeDCtx)>;
    Context context(ZSTD_createDCtx(), &ZSTD_freeDCtx);
    if (!context) {
        return PackageResult<CompressionStats>::Failure(
            CompressionError("Zstandard decompression context allocation failed."));
    }

    std::vector<std::uint8_t> inputBuffer(streamBufferSize);
    std::vector<std::uint8_t> outputBuffer(streamBufferSize);
    CompressionStats stats;
    bool frameCompleted = false;
    while (true) {
        input.read(
            reinterpret_cast<char*>(inputBuffer.data()),
            static_cast<std::streamsize>(inputBuffer.size()));
        const auto bytesRead = input.gcount();
        if (bytesRead < 0 || input.bad()) {
            return PackageResult<CompressionStats>::Failure(
                StreamError("Reading compressed input failed."));
        }
        if (bytesRead == 0) {
            break;
        }
        if (frameCompleted) {
            return PackageResult<CompressionStats>::Failure(
                CompressionError(
                    "Compressed input contains trailing frames or bytes."));
        }

        const auto nextInputSize = AddSize(
            stats.inputSize,
            static_cast<std::size_t>(bytesRead),
            "Decompression input");
        if (!nextInputSize) {
            return PackageResult<CompressionStats>::Failure(
                nextInputSize.error());
        }
        stats.inputSize = nextInputSize.value();

        ZSTD_inBuffer source{
            inputBuffer.data(),
            static_cast<std::size_t>(bytesRead),
            0,
        };
        while (source.pos < source.size) {
            ZSTD_outBuffer destination{
                outputBuffer.data(),
                outputBuffer.size(),
                0,
            };
            const std::size_t remaining = ZSTD_decompressStream(
                context.get(),
                &destination,
                &source);
            if (ZSTD_isError(remaining)) {
                return PackageResult<CompressionStats>::Failure(
                    CompressionError(ZSTD_getErrorName(remaining)));
            }

            const auto nextOutputSize = AddSize(
                stats.outputSize,
                destination.pos,
                "Decompression output");
            if (!nextOutputSize ||
                nextOutputSize.value() > maximumPlaintextSize ||
                nextOutputSize.value() > expectedPlaintextSize) {
                return PackageResult<CompressionStats>::Failure(
                    CompressionError(
                        "Decompressed data exceeds its declared size or limit."));
            }
            if (!WriteChunk(
                    output,
                    outputBuffer.data(),
                    destination.pos)) {
                return PackageResult<CompressionStats>::Failure(
                    StreamError("Writing decompressed output failed."));
            }
            stats.outputSize = nextOutputSize.value();

            if (remaining == 0) {
                frameCompleted = true;
                if (source.pos != source.size) {
                    return PackageResult<CompressionStats>::Failure(
                        CompressionError(
                            "Compressed input contains trailing frames or bytes."));
                }
            }
        }
    }

    if (!input.eof() || !frameCompleted ||
        stats.outputSize != expectedPlaintextSize) {
        return PackageResult<CompressionStats>::Failure(
            CompressionError(
                "Compressed frame is truncated or has an invalid plaintext size."));
    }
    output.flush();
    if (!output) {
        return PackageResult<CompressionStats>::Failure(
            StreamError("Flushing decompressed output failed."));
    }
    return PackageResult<CompressionStats>::Success(stats);
}

PackageResult<CompressedBuffer> ZstdCompressionCodec::CompressIfSmaller(
    const std::vector<std::uint8_t>& plaintext,
    const int compressionLevel) const {
    std::string inputBytes;
    if (!plaintext.empty()) {
        inputBytes.assign(
            reinterpret_cast<const char*>(plaintext.data()),
            plaintext.size());
    }
    std::istringstream input(
        inputBytes,
        std::ios::in | std::ios::binary);
    std::ostringstream output(
        std::ios::out | std::ios::binary);
    const auto compressed =
        CompressStream(input, output, compressionLevel);
    if (!compressed) {
        return PackageResult<CompressedBuffer>::Failure(
            compressed.error());
    }

    const std::string compressedBytes = output.str();
    if (compressedBytes.size() >= plaintext.size()) {
        return PackageResult<CompressedBuffer>::Success({
            CompressionAlgorithm::None,
            plaintext,
        });
    }
    return PackageResult<CompressedBuffer>::Success({
        CompressionAlgorithm::Zstandard,
        std::vector<std::uint8_t>(
            compressedBytes.begin(),
            compressedBytes.end()),
    });
}

PackageResult<std::vector<std::uint8_t>>
ZstdCompressionCodec::Decompress(
    const CompressedBuffer& stored,
    const std::uint64_t expectedPlaintextSize,
    const std::uint64_t maximumPlaintextSize) const {
    if (expectedPlaintextSize > maximumPlaintextSize) {
        return PackageResult<std::vector<std::uint8_t>>::Failure(
            CompressionError(
                "Declared plaintext size exceeds the decompression limit."));
    }
    if (stored.algorithm == CompressionAlgorithm::None) {
        if (stored.bytes.size() != expectedPlaintextSize) {
            return PackageResult<std::vector<std::uint8_t>>::Failure(
                CompressionError(
                    "Uncompressed data does not match its declared size."));
        }
        return PackageResult<std::vector<std::uint8_t>>::Success(
            stored.bytes);
    }
    if (stored.algorithm != CompressionAlgorithm::Zstandard) {
        return PackageResult<std::vector<std::uint8_t>>::Failure(
            CompressionError("Compression algorithm is unsupported."));
    }

    std::string inputBytes;
    if (!stored.bytes.empty()) {
        inputBytes.assign(
            reinterpret_cast<const char*>(stored.bytes.data()),
            stored.bytes.size());
    }
    std::istringstream input(
        inputBytes,
        std::ios::in | std::ios::binary);
    std::ostringstream output(
        std::ios::out | std::ios::binary);
    const auto decompressed = DecompressStream(
        input,
        output,
        expectedPlaintextSize,
        maximumPlaintextSize);
    if (!decompressed) {
        return PackageResult<std::vector<std::uint8_t>>::Failure(
            decompressed.error());
    }

    const std::string outputBytes = output.str();
    return PackageResult<std::vector<std::uint8_t>>::Success(
        std::vector<std::uint8_t>(
            outputBytes.begin(),
            outputBytes.end()));
}

} // namespace dbp::package

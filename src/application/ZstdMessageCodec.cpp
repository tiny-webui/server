#include "ZstdMessageCodec.h"

#include <cstring>
#include <limits>
#include <stdexcept>

using namespace TUI::Application;

namespace
{
	constexpr int COMPRESSION_LEVEL = ZSTD_CLEVEL_DEFAULT;
	constexpr std::size_t MIN_EFFECTIVE_SIZE = 100;

	enum class CompressionType : std::uint8_t
	{
		None = 0,
		Zstd = 1,
	};
}

ZstdMessageCodec::ZstdMessageCodec()
{
	_cctx = ZSTD_createCCtx();
	_dctx = ZSTD_createDCtx();
	if (_cctx == nullptr || _dctx == nullptr)
	{
		if (_cctx)
		{
			ZSTD_freeCCtx(_cctx);
			_cctx = nullptr;
		}
		if (_dctx)
		{
			ZSTD_freeDCtx(_dctx);
			_dctx = nullptr;
		}
		throw std::runtime_error("Failed to create zstd contexts");
	}
}

ZstdMessageCodec::~ZstdMessageCodec()
{
	if (_cctx)
	{
		ZSTD_freeCCtx(_cctx);
		_cctx = nullptr;
	}
	if (_dctx)
	{
		ZSTD_freeDCtx(_dctx);
		_dctx = nullptr;
	}
}

std::vector<std::uint8_t> ZstdMessageCodec::Encode(const std::vector<std::uint8_t>& data)
{
	const std::uint64_t originalSize = data.size();

	// Below threshold, do not compress.
	if (originalSize < MIN_EFFECTIVE_SIZE)
	{
		std::vector<std::uint8_t> result(1 + originalSize);
		result[0] = static_cast<std::uint8_t>(CompressionType::None);
		std::memcpy(result.data() + 1, data.data(), originalSize);
		return result;
	}

	const std::size_t maxCompressedSize = ZSTD_compressBound(originalSize);
    /** 1: for the compression type header. */
    std::vector<std::uint8_t> output(1 + maxCompressedSize);

	const std::size_t compressedSize = ZSTD_compressCCtx(
		_cctx,
		output.data() + 1,
		maxCompressedSize,
		data.data(),
		originalSize,
		COMPRESSION_LEVEL);

	if (ZSTD_isError(compressedSize))
	{
		throw std::runtime_error(ZSTD_getErrorName(compressedSize));
	}

	if (compressedSize + 1 >= originalSize)
	{
		output.resize(1 + originalSize);
		output[0] = static_cast<std::uint8_t>(CompressionType::None);
		std::memcpy(output.data() + 1, data.data(), originalSize);
		return output;
	}

	output.resize(1 + compressedSize);
	output[0] = static_cast<std::uint8_t>(CompressionType::Zstd);
	return output;
}

std::vector<std::uint8_t> ZstdMessageCodec::Decode(const std::vector<std::uint8_t>& data)
{
	if (data.empty())
	{
		throw std::runtime_error("Compressed payload missing body");
	}

	const auto type = static_cast<CompressionType>(data[0]);
	const std::uint8_t* payload = data.data() + 1;
	const std::size_t payloadSize = data.size() - 1;

	switch (type)
	{
	case CompressionType::None:
	{
		std::vector<std::uint8_t> output(payloadSize);
		if (payloadSize > 0)
		{
			std::memcpy(output.data(), payload, payloadSize);
		}
		return output;
	}
	case CompressionType::Zstd:
	{
		if (payloadSize == 0)
		{
			throw std::runtime_error("Compressed payload missing zstd frame");
		}
		const unsigned long long frameSize = ZSTD_getFrameContentSize(payload, payloadSize);
		if (frameSize == ZSTD_CONTENTSIZE_ERROR)
		{
			throw std::runtime_error("Invalid zstd frame");
		}
		if (frameSize == ZSTD_CONTENTSIZE_UNKNOWN)
		{
			throw std::runtime_error("Unknown uncompressed size in zstd frame");
		}

		std::vector<std::uint8_t> output(static_cast<std::size_t>(frameSize));
		const std::size_t decompressedSize = ZSTD_decompressDCtx(
			_dctx,
			output.data(),
			frameSize,
			payload,
			payloadSize);

		if (ZSTD_isError(decompressedSize))
		{
			throw std::runtime_error(ZSTD_getErrorName(decompressedSize));
		}

		if (decompressedSize != frameSize)
		{
			throw std::runtime_error("Decompressed size does not match header length");
		}

		return output;
	}
	default:
		throw std::runtime_error("Unknown compression type");
	}

	return {};
}

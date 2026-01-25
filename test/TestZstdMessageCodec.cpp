#include <cstdint>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

#include "Utility.h"
#include "application/ZstdMessageCodec.h"

using namespace TUI::Application;

namespace
{
	std::vector<std::uint8_t> MakeRandomBytes(std::size_t length)
	{
		std::vector<std::uint8_t> data(length);
		if (!data.empty())
		{
			// Deterministic low-entropy pattern to encourage compression effectiveness.
			std::uint8_t value = static_cast<std::uint8_t>((length % 17) + 1);
			std::fill(data.begin(), data.end(), value);
		}
		return data;
	}
}

static void TestRoundTripSmall()
{
	ZstdMessageCodec codec;
	for (std::uint64_t len : {std::uint64_t{0}, std::uint64_t{1}, std::uint64_t{5}, std::uint64_t{99}})
	{
		auto input = MakeRandomBytes(static_cast<std::size_t>(len));
		auto encoded = codec.Encode(input);
		AssertWithMessage(!encoded.empty(), "Encoded payload is empty");
		AssertWithMessage(encoded[0] == 0, "Small payload should be uncompressed");
		AssertWithMessage(encoded.size() == 1 + input.size(), "Uncompressed payload should include type byte");
		auto decoded = codec.Decode(encoded);
		AssertWithMessage(decoded == input, "Round-trip mismatch for small payload");
	}
}

static void TestRoundTripMedium()
{
	ZstdMessageCodec codec;
	for (std::uint64_t len : {std::uint64_t{150}, std::uint64_t{500}, std::uint64_t{1024}, std::uint64_t{2048}})
	{
		// Highly compressible data to ensure compression wins.
		std::vector<std::uint8_t> input(static_cast<std::size_t>(len), 0x11);
		auto encoded = codec.Encode(input);
		AssertWithMessage(!encoded.empty(), "Encoded payload is empty");
		AssertWithMessage(encoded[0] == 1, "Medium payload should be compressed");
		auto decoded = codec.Decode(encoded);
		AssertWithMessage(decoded == input, "Round-trip mismatch for medium payload");
	}
}

static void TestRoundTripLarge()
{
	ZstdMessageCodec codec;
	for (std::uint64_t len : {std::uint64_t{5000}, std::uint64_t{20000}, std::uint64_t{75000}})
	{
		std::vector<std::uint8_t> input(static_cast<std::size_t>(len), 0x22);
		auto encoded = codec.Encode(input);
		AssertWithMessage(!encoded.empty(), "Encoded payload is empty");
		AssertWithMessage(encoded[0] == 1, "Large payload should be compressed");
		auto decoded = codec.Decode(encoded);
		AssertWithMessage(decoded == input, "Round-trip mismatch for large payload");
	}
}

static void TestRepeatedRoundTrips()
{
	ZstdMessageCodec codec;
	for (int i = 0; i < 200; ++i)
	{
		std::uint64_t len = static_cast<std::uint64_t>(i * 37 % 5000); // varied but small to keep test fast
		auto input = MakeRandomBytes(static_cast<std::size_t>(len));
		auto encoded = codec.Encode(input);
		auto decoded = codec.Decode(encoded);
		AssertWithMessage(decoded == input, "Round-trip mismatch in repeated run");
	}
}

int main(int argc, char const *argv[])
{
	(void)argc;
	(void)argv;

	RunTest(TestRoundTripSmall());
	RunTest(TestRoundTripMedium());
	RunTest(TestRoundTripLarge());
	RunTest(TestRepeatedRoundTrips());

	return 0;
}


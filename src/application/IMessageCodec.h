#pragma once

#include <vector>
#include <cstdint>

namespace TUI::Application
{

    class IMessageCodec
    {
    public:
        virtual ~IMessageCodec() = default;

        virtual std::vector<std::uint8_t> Encode(const std::vector<std::uint8_t>& data) = 0;
        virtual std::vector<std::uint8_t> Decode(const std::vector<std::uint8_t>& data) = 0;
    };

}

#pragma once

#include <type_traits>
#include <limits>
#include <concepts>
#include <cstdint>
#include <vector>
#include <array>
#include <cstring>
#include <map>
#include <optional>
#include <stdexcept>

namespace TUI::Common
{
    template<typename T>
    concept Enum = std::is_enum_v<T>;

    template<typename T>
    concept UnsignedInt = std::is_integral_v<T> && std::is_unsigned_v<T>;

    template<Enum E, UnsignedInt LengthType = uint32_t>
    class Tlv
    {
    public:
        using EnumValueType = std::underlying_type_t<E>;

        static Tlv Parse(const uint8_t* data, size_t length)
        {
            Tlv result{};
            size_t offset = 0;
            while (offset < length)
            {
                if (offset + sizeof(EnumValueType) + sizeof(LengthType) > length)
                {
                    throw std::runtime_error("Invalid TLV data: not enough data for type and length");
                }
                EnumValueType typeValue;
                LengthType valueLength;
                std::memcpy(&typeValue, data + offset, sizeof(EnumValueType));
                offset += sizeof(EnumValueType);
                std::memcpy(&valueLength, data + offset, sizeof(LengthType));
                offset += sizeof(LengthType);
                if (offset + valueLength > length)
                {
                    throw std::runtime_error("Invalid TLV data: not enough data for value");
                }
                E type = static_cast<E>(typeValue);
                std::vector<uint8_t> value(data + offset, data + offset + valueLength);
                result._elements[type] = std::move(value);
                offset += valueLength;
            }
            return result;
        }

        static Tlv Parse(const std::vector<uint8_t>& data)
        {
            return Parse(data.data(), data.size());
        }

        template<size_t N>
        static Tlv Parse(const std::array<uint8_t, N>& data)
        {
            return Parse(data.data(), data.size());
        }

        Tlv() = default;

        void SetElement(E type, std::vector<uint8_t> value)
        {
            if (value.size() > static_cast<size_t>(std::numeric_limits<LengthType>::max()))
            {
                throw std::invalid_argument("Value size exceeds maximum length");
            }
            _elements[type] = std::move(value);
        }

        template<size_t N>
        void SetElement(E type, const std::array<uint8_t, N>& value)
        {
            if (N > static_cast<size_t>(std::numeric_limits<LengthType>::max()))
            {
                throw std::invalid_argument("Value size exceeds maximum length");
            }
            _elements[type] = std::vector<uint8_t>(value.begin(), value.end());
        }

        void SetElement(E type, const std::string& value)
        {
            if (value.size() > static_cast<size_t>(std::numeric_limits<LengthType>::max()))
            {
                throw std::invalid_argument("Value size exceeds maximum length");
            }
            _elements[type] = std::vector<uint8_t>(value.begin(), value.end());
        }

        std::optional<std::vector<uint8_t>> GetElement(E type) const
        {
            auto item = _elements.find(type);
            if (item == _elements.end())
            {
                return std::nullopt;
            }
            return item->second;
        }

        std::vector<uint8_t> Serialize() const
        {
            /** Calculate the total size first */
            size_t totalSize = 0;
            for (const auto& item : _elements)
            {
                totalSize += sizeof(EnumValueType) + sizeof(LengthType) + item.second.size();
            }
            std::vector<uint8_t> serializedData;
            serializedData.reserve(totalSize);
            for (const auto& item : _elements)
            {
                EnumValueType typeValue = static_cast<EnumValueType>(item.first);
                LengthType length = static_cast<LengthType>(item.second.size());
                serializedData.insert(
                    serializedData.end(),
                    reinterpret_cast<uint8_t*>(&typeValue),
                    reinterpret_cast<uint8_t*>(&typeValue) + sizeof(EnumValueType));
                serializedData.insert(
                    serializedData.end(),
                    reinterpret_cast<uint8_t*>(&length),
                    reinterpret_cast<uint8_t*>(&length) + sizeof(LengthType));
                serializedData.insert(
                    serializedData.end(),
                    item.second.begin(),
                    item.second.end());
            }
            return serializedData;
        }
    private:
        std::map<E, std::vector<uint8_t>> _elements;
    };
}

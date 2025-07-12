#include <stdexcept>
#include "HandshakeMessage.h"

using namespace TUI::Cipher;

using TypeType = uint8_t;
using LengthType = uint32_t;

HandshakeMessage::HandshakeMessage(const std::map<Type, std::vector<uint8_t>>& elements)
    : _elements(elements)
{
}

HandshakeMessage::HandshakeMessage(const std::vector<uint8_t>& data)
    : _elements()
{
    size_t offset = 0;
    while(offset < data.size())
    {
        if (offset + sizeof(TypeType) + sizeof(LengthType) > data.size())
        {
            throw std::runtime_error("Invalid size");
        }
        Type type = static_cast<Type>(data[offset]);
        offset += sizeof(TypeType);
        LengthType length = *reinterpret_cast<const LengthType*>(&data[offset]);
        offset += sizeof(LengthType);
        if (offset + length > data.size())
        {
            throw std::runtime_error("Invalid size");
        }
        if (type > Type::KeyIndex)
        {
            /** Ignore unknown types */
            offset += length;
            continue;
        }
        std::vector<uint8_t> element(data.begin() + offset, data.begin() + offset + length);
        _elements[type] = std::move(element);
        offset += length;
    }
}

std::vector<uint8_t> HandshakeMessage::Serialize() const
{
    std::vector<uint8_t> data;
    /** _elements is an ordered map. This ensures the serialized data will be the same for the same entries */
    for (const auto& [type, element] : _elements)
    {
        TypeType typeValue = static_cast<TypeType>(type);
        LengthType length = static_cast<LengthType>(element.size());
        data.push_back(typeValue);
        data.insert(data.end(), reinterpret_cast<const uint8_t*>(&length), reinterpret_cast<const uint8_t*>(&length) + sizeof(LengthType));
        data.insert(data.end(), element.begin(), element.end());
    }
    return data;
}

std::optional<std::vector<uint8_t>> HandshakeMessage::GetElement(Type type) const
{
    auto it = _elements.find(type);
    if (it != _elements.end())
    {
        return it->second;
    }
    return std::nullopt;
}

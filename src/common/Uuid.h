#pragma once

#include <string>
#include <stdexcept>
#include <uuid/uuid.h>

namespace TUI::Common
{
    class Uuid
    {
    public:
        Uuid()
        {
            uuid_generate(_uuid);
        }
        explicit Uuid(const std::string& str)
        {
            if (uuid_parse(str.c_str(), _uuid) != 0)
            {
                throw std::runtime_error("Invalid UUID string");
            }
        }
        explicit Uuid(std::nullptr_t)
        {
            uuid_clear(_uuid);
        }

        Uuid(const Uuid& other)
        {
            uuid_copy(_uuid, other._uuid);
        }
        Uuid(Uuid&& other) noexcept
        {
            uuid_copy(_uuid, other._uuid);
            uuid_clear(other._uuid);
        }
        Uuid& operator=(const Uuid& other)
        {
            if (this != &other)
            {
                uuid_copy(_uuid, other._uuid);
            }
            return *this;
        }
        Uuid& operator=(Uuid&& other) noexcept
        {
            if (this != &other)
            {
                uuid_copy(_uuid, other._uuid);
                uuid_clear(other._uuid);
            }
            return *this;
        }
        Uuid& operator=(std::nullptr_t)
        {
            uuid_clear(_uuid);
            return *this;
        }

        ~Uuid() = default;

        bool operator== (const Uuid& other) const
        {
            return uuid_compare(_uuid, other._uuid) == 0;
        }
        bool operator!= (const Uuid& other) const
        {
            return uuid_compare(_uuid, other._uuid) != 0;
        }
        bool operator< (const Uuid& other) const
        {
            return uuid_compare(_uuid, other._uuid) < 0;
        }
        bool operator> (const Uuid& other) const
        {
            return uuid_compare(_uuid, other._uuid) > 0;
        }
        bool operator<= (const Uuid& other) const
        {
            return uuid_compare(_uuid, other._uuid) <= 0;
        }
        bool operator>= (const Uuid& other) const
        {
            return uuid_compare(_uuid, other._uuid) >= 0;
        }

        bool operator== (std::nullptr_t) const
        {
            return uuid_is_null(_uuid);
        }
        bool operator!= (std::nullptr_t) const
        {
            return !uuid_is_null(_uuid);
        }

        explicit operator std::string() const
        {
            std::string str(UUID_STR_LEN, '\0');
            uuid_unparse(_uuid, str.data());
            return str;
        }

        size_t Hash() const noexcept
        {
            return std::hash<std::string_view>()(
                std::string_view(reinterpret_cast<const char*>(_uuid), sizeof(uuid_t)));
        }
    private:
        uuid_t _uuid{};
    };
}

namespace std
{
    template <>
    struct hash<TUI::Common::Uuid>
    {
        size_t operator()(const TUI::Common::Uuid& uuid) const noexcept
        {
            return uuid.Hash();
        }
    };
}

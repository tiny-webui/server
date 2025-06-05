#pragma once

#include <string>
#include <vector>
#include <type_traits>
#include <memory>
#include <unordered_set>
#include <functional>
#include <optional>
#include <nlohmann/json.hpp>

namespace TUI::ApiProvider::Option
{
    enum class Type
    {
        STRING,
        STRING_FROM_LIST,
        NUMBER_FROM_RANGE,
        NUMBER_FROM_LIST,
        BOOLEAN,
    };

    static const std::unordered_map<Type, std::string> TypeToJsonType = {
        {Type::STRING, "string"},
        {Type::STRING_FROM_LIST, "string"},
        {Type::NUMBER_FROM_RANGE, "string"},
        {Type::NUMBER_FROM_LIST, "number"},
        {Type::BOOLEAN, "boolean"},
    };

    template<typename T>
    struct OptionBase
    {
        virtual ~OptionBase() = default;
        virtual nlohmann::json GetDefaultValue() const = 0;
        virtual nlohmann::json GetParams() const = 0;
        virtual void Parse(T& options, nlohmann::json value) const = 0;
    };

    template<typename T>
    struct StringOption : public OptionBase<T>
    {
        StringOption(std::function<void(T&, std::string)> assignValueIn, std::string defaultValueIn)
            : assignValue(std::move(assignValueIn)), defaultValue(std::move(defaultValueIn))
        {
        }
        StringOption(std::function<void(T&, std::string)> assignValueIn)
            : assignValue(std::move(assignValueIn))
        {
        }
        nlohmann::json GetDefaultValue() const override
        {
            if (defaultValue.has_value())
            {
                return defaultValue.value();
            }
            else
            {
                return nlohmann::json{};
            }
        }
        nlohmann::json GetParams() const override
        {
            /** null */
            return nlohmann::json{};
        }
        void Parse(T& result, nlohmann::json value) const override
        {
            if (value.is_null())
            {
                if (!defaultValue.has_value())
                {
                    throw std::invalid_argument("Value is null but no default value is set");
                }
                assignValue(result, defaultValue.value());
                return;
            }
            if (!value.is_string())
            {
                throw std::invalid_argument("Expected a string value");
            }
            assignValue(result, value.get<std::string>());
        }
        std::function<void(T&, std::string)> assignValue;
        std::optional<std::string> defaultValue{std::nullopt};
    };

    template<typename T>
    struct StringFromListOption : public OptionBase<T>
    {
        StringFromListOption(std::function<void(T&, std::string)> assignValueIn, std::vector<std::string> optionsIn, std::string defaultValueIn)
            : assignValue(std::move(assignValueIn)), options(std::move(optionsIn)), defaultValue(std::move(defaultValueIn))
        {
            if (std::find(options.begin(), options.end(), defaultValue) == options.end())
            {
                throw std::invalid_argument("Default value is not in the list of options");
            }
        }
        StringFromListOption(std::function<void(T&, std::string)> assignValueIn, std::vector<std::string> optionsIn)
            : assignValue(std::move(assignValueIn)), options(std::move(optionsIn))
        {
        }
        nlohmann::json GetDefaultValue() const override
        {
            if (defaultValue.has_value())
            {
                return defaultValue.value();
            }
            else
            {
                return nlohmann::json{};
            }
        }
        nlohmann::json GetParams() const override
        {
            return options;
        }
        void Parse(T& result, nlohmann::json value) const override
        {
            if (value.is_null())
            {
                if (!defaultValue.has_value())
                {
                    throw std::invalid_argument("Value is null but no default value is set");
                }
                assignValue(result, defaultValue.value());
                return;
            }
            if (!value.is_string())
            {
                throw std::invalid_argument("Expected a string value");
            }
            if (std::find(options.begin(), options.end(), value.get<std::string>()) == options.end())
            {
                throw std::invalid_argument("Value is not in the list of options");
            }
            assignValue(result, value.get<std::string>());
        }
        std::function<void(T&, std::string)> assignValue;
        std::vector<std::string> options;
        std::optional<std::string> defaultValue{std::nullopt};
    };

    template<typename T>
    struct NumberFromRangeOption : public OptionBase<T>
    {
        NumberFromRangeOption(std::function<void(T&, double)> assignValueIn, double minIn, double maxIn, double defaultValueIn)
            : assignValue(std::move(assignValueIn)), min(minIn), max(maxIn), defaultValue(defaultValueIn)
        {
            if (defaultValue < min || defaultValue > max)
            {
                throw std::invalid_argument("Default value is out of range");
            }
        }
        NumberFromRangeOption(std::function<void(T&, double)> assignValueIn, double minIn, double maxIn)
            : assignValue(std::move(assignValueIn)), min(minIn), max(maxIn)
        {
        }
        nlohmann::json GetDefaultValue() const override
        {
            if (defaultValue.has_value())
            {
                return defaultValue.value();
            }
            else
            {
                return nlohmann::json{};
            }
        }
        nlohmann::json GetParams() const override
        {
            return nlohmann::json{
                {"min", min},
                {"max", max},
            };
        }
        void Parse(T& result, nlohmann::json value) const override
        {
            if (value.is_null())
            {
                if (!defaultValue.has_value())
                {
                    throw std::invalid_argument("Value is null but no default value is set");
                }
                assignValue(result, defaultValue.value());
                return;
            }
            if (!value.is_number())
            {
                throw std::invalid_argument("Expected a number value");
            }
            double num = value.get<double>();
            if (num < min || num > max)
            {
                throw std::invalid_argument("Value is out of range");
            }
            assignValue(result, num);
        }
        std::function<void(T&, double)> assignValue;
        double min;
        double max;
        std::optional<double> defaultValue{std::nullopt};
    };

    template<typename T>
    struct NumberFromListOption : public OptionBase<T>
    {
        NumberFromListOption(std::function<void(T&, double)> assignValueIn, std::vector<double> optionsIn, double defaultValueIn)
            : assignValue(std::move(assignValueIn)), options(std::move(optionsIn)), defaultValue(defaultValueIn) 
        {
            if (std::find(options.begin(), options.end(), defaultValue) == options.end())
            {
                throw std::invalid_argument("Default value is not in the list of options");
            }
        }
        NumberFromListOption(std::function<void(T&, double)> assignValueIn, std::vector<double> optionsIn)
            : assignValue(std::move(assignValueIn)), options(std::move(optionsIn))
        {
        }
        nlohmann::json GetDefaultValue() const override
        {
            if (defaultValue.has_value())
            {
                return defaultValue.value();
            }
            else
            {
                return nlohmann::json{};
            }
        }
        nlohmann::json GetParams() const override
        {
            return options;
        }
        void Parse(T& result, nlohmann::json value) const override
        {
            if (value.is_null())
            {
                if (!defaultValue.has_value())
                {
                    throw std::invalid_argument("Value is null but no default value is set");
                }
                assignValue(result, defaultValue.value());
                return;
            }
            if (!value.is_number())
            {
                throw std::invalid_argument("Expected a number value");
            }
            double num = value.get<double>();
            if (std::find(this->options.begin(), this->options.end(), num) == this->options.end())
            {
                throw std::invalid_argument("Value is not in the list of options");
            }
            assignValue(result, num);
        }
        std::function<void(T&, double)> assignValue;
        std::vector<double> options;
        std::optional<double> defaultValue{std::nullopt};
    };

    template<typename T>
    struct BooleanOption : public OptionBase<T>
    {
        BooleanOption(std::function<void(T&, bool)> assignValueIn, bool defaultValueIn)
            : assignValue(std::move(assignValueIn)), defaultValue(defaultValueIn)
        {
        }
        BooleanOption(std::function<void(T&, bool)> assignValueIn)
            : assignValue(std::move(assignValueIn))
        {
        }
        nlohmann::json GetDefaultValue() const override
        {
            if (defaultValue.has_value())
            {
                return defaultValue.value();
            }
            else
            {
                return nlohmann::json{};
            }
        }
        nlohmann::json GetParams() const override
        {
            /** null */
            return nlohmann::json{};
        }
        void Parse(T& result, nlohmann::json value) const override
        {
            if (value.is_null())
            {
                if (!defaultValue.has_value())
                {
                    throw std::invalid_argument("Value is null but no default value is set");
                }
                assignValue(result, defaultValue.value());
                return;
            }
            if (!value.is_boolean())
            {
                throw std::invalid_argument("Expected a boolean value");
            }
            assignValue(result, value.get<bool>());
        }
        std::function<void(T&, bool)> assignValue;
        std::optional<bool> defaultValue{std::nullopt};
    };

    template<typename T>
    struct Option
    {
        Type type;
        std::string jsonKey;
        bool optional{false};
        std::shared_ptr<OptionBase<T>> params;
        Option(Type typeIn, std::string jsonKeyIn, bool optionalIn, std::shared_ptr<OptionBase<T>> paramsIn)
            : type(typeIn),
              jsonKey(std::move(jsonKeyIn)),
              optional(optionalIn),
              params(std::move(paramsIn))
        {
            if (!params)
            {
                throw std::invalid_argument("params cannot be null");
            }
        }
        nlohmann::json ToJson() const
        {
            nlohmann::json json;
            json["type"] = TypeToJsonType.at(type);
            json["key"] = jsonKey;
            json["optional"] = optional;
            auto defaultValue = params->GetDefaultValue();
            if (!defaultValue.is_null())
            {
                json["default"] = defaultValue;
            }
            auto paramsJson = params->GetParams();
            if (!paramsJson.is_null())
            {
                json["params"] = paramsJson;
            }
            return json;
        }
        void Parse(T& result, nlohmann::json value) const
        {
            if (!optional)
            {
                params->Parse(result, value);
                return;
            }
            if ((!value.is_null()) || (!params->GetDefaultValue().is_null()))
            {
                params->Parse(result, value);
                return;
            }
            /** The value is optional and there is no given value nor default value. This is valid */
        }
    };

    /**
     * @brief Define a list of options that fill a given type T.
     * 
     * @tparam T Use std::optional for optional values.
     */
    template<typename T>
    struct OptionList : std::vector<Option<T>>
    {
        using std::vector<Option<T>>::vector;
        OptionList(std::initializer_list<Option<T>> options) : std::vector<Option<T>>(options)
        {
            /** Check for duplicating keys */
            std::unordered_set<std::string> keys;
            for (const auto& option : *this)
            {
                if (keys.find(option.jsonKey) != keys.end())
                {
                    throw std::invalid_argument("Duplicate jsonKey found: " + option.jsonKey);
                }
                keys.insert(option.jsonKey);
            }
        }
        nlohmann::json ToJson() const
        {
            nlohmann::json jsonArray = nlohmann::json::array();
            for (const auto& option : *this)
            {
                jsonArray.push_back(option.ToJson());
            }
            return jsonArray;
        }
        std::string ToString() const
        {
            return ToJson().dump();
        }
        T Parse(nlohmann::json filledOptions) const
        {
            T result{};
            for (const auto& option : *this)
            {
                auto value = filledOptions.value(option.jsonKey, nlohmann::json{});
                option.Parse(result, value);
            }
            return result;
        }
    };
    
    template <typename U>
    struct TypeFromOption;

    template <typename T>
    struct TypeFromOption<StringOption<T>>
    {
        static constexpr Type value = Type::STRING;
    };
    template <typename T>
    struct TypeFromOption<StringFromListOption<T>>
    {
        static constexpr Type value = Type::STRING_FROM_LIST;
    };
    template <typename T>
    struct TypeFromOption<NumberFromRangeOption<T>>
    {
        static constexpr Type value = Type::NUMBER_FROM_RANGE;
    };
    template <typename T>
    struct TypeFromOption<NumberFromListOption<T>>
    {
        static constexpr Type value = Type::NUMBER_FROM_LIST;
    };
    template <typename T>
    struct TypeFromOption<BooleanOption<T>>
    {
        static constexpr Type value = Type::BOOLEAN;
    };

    template <typename T, typename U, typename... Args>
    Option<T> CreateOption(std::string jsonKey, bool optional, Args&&... args)
    {
        static_assert(std::is_base_of_v<OptionBase<T>, U>, "U must derive from OptionBase<T>");
        return Option<T>{
            TypeFromOption<U>::value,
            std::move(jsonKey),
            optional,
            std::make_shared<U>(std::forward<Args>(args)...)
        };
    };
}

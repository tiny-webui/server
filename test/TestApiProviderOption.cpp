#include <iostream>
#include <apiProvider/Option.h>

using namespace TUI::ApiProvider::Option;

struct TestOptions
{
    std::string stringOption;
    std::string stringFromListOption;
    double numberFromRangeOption;
    std::optional<double> numberFromListOption;
    std::optional<bool> booleanOption;
    std::string ToString() const
    {
        std::string result = "";
        result += "stringOption: " + stringOption + "\n";
        result += "stringFromListOption: " + stringFromListOption + "\n";
        result += "numberFromRangeOption: " + std::to_string(numberFromRangeOption) + "\n";
        if (numberFromListOption.has_value())
        {
            result += "numberFromListOption: " + std::to_string(numberFromListOption.value()) + "\n";
        }
        if (booleanOption.has_value())
        {
            result += "booleanOption: " + std::to_string(booleanOption.value()) + "\n";
        }
        return result;
    }
};

int main(int argc, char const *argv[])
{
    (void)argc;
    (void)argv;

    auto optionList = OptionList<TestOptions>{
        /** Mandatory option w/o default value */
        CreateOption<TestOptions,StringOption<TestOptions>>(
            "stringOption",
            false,
            [](TestOptions& options, std::string value){options.stringOption = value;}),
        /** Mandatory option w/ default value */
        CreateOption<TestOptions, StringFromListOption<TestOptions>>(
            "stringFromListOption",
            false,
            [](TestOptions& options, std::string value){options.stringFromListOption = value;},
            std::vector<std::string>{"option1", "option2", "option3"},
            "option1"),
        /** Optional option w/ default value */
        CreateOption<TestOptions, NumberFromRangeOption<TestOptions>>(
            "numberFromRangeOption",
            true,
            [](TestOptions& options, double value){options.numberFromRangeOption = value;},
            0.0, 100.0,
            50.0),
        /** Optional option w/o default value */
        CreateOption<TestOptions, NumberFromListOption<TestOptions>>(
            "numberFromListOption",
            true,
            [](TestOptions& options, double value){options.numberFromListOption = value;},
            std::vector<double>{1.0, 2.0, 3.0}),
        /** Optional option w/o default value */
        CreateOption<TestOptions, BooleanOption<TestOptions>>(
            "booleanOption",
            true,
            [](TestOptions& options, bool value){options.booleanOption = value;})
    };

    std::cout << "Option List JSON:" << std::endl << optionList.ToString() << std::endl;

    nlohmann::json filledOption = {
        {"stringOption", "test_string"},
        {"numberFromListOption", 2.0}
    };

    auto result = optionList.Parse(filledOption);

    std::cout << "Parsed Options:" << std::endl << result.ToString() << std::endl;

    return 0;
}


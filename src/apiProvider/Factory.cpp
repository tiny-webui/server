#include <functional>
#include "Factory.h"

#include "AzureOpenAI.h"

using namespace TUI::ApiProvider;

static std::unordered_map<std::string, std::function<std::shared_ptr<IProvider>()>> providerMap = {
    {"AzureOpenAI", [](){return std::make_shared<AzureOpenAI>();}}
    /** Register new providers here */
};

std::shared_ptr<IProvider> Factory::CreateProvider(
    const std::string& name,
    const nlohmann::json& params)
{
    auto item = providerMap.find(name);
    if (item == providerMap.end())
    {
        throw std::runtime_error("Invalid provider: " + name);
    }
    auto provider = item->second();
    provider->Initialize(params);
    return provider;
}

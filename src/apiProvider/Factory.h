#pragma once

#include <memory>
#include "IProvider.h"

namespace TUI::ApiProvider::Factory
{
    std::shared_ptr<IProvider> CreateProvider(
        const std::string& name,
        const nlohmann::json& params);
}

#pragma once

#include <optional>
#include <string>
namespace Env {

enum class Key {
    // provider settings
    PROVIDER_UPDATE_MESSAGE,
    PROVIDER_DISABLE_CONFIG,
};

std::optional<std::string> Get(Key key);

std::string_view ToString(Key key);

}

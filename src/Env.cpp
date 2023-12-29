#include "Env.h"
#include <optional>

std::optional<std::string> Env::Get(Env::Key key) {
    auto StrKey = ToString(key);
    auto Value = std::getenv(StrKey.data());
    if (!Value || std::string_view(Value).empty()) {
        return std::nullopt;
    }
    return Value;
}

std::string_view Env::ToString(Env::Key key) {
    switch (key) {
    case Key::PROVIDER_UPDATE_MESSAGE:
        return "BEAMMP_PROVIDER_UPDATE_MESSAGE";
        break;
    }
    return "";
}

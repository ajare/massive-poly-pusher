#pragma once
#include <string>
#include <vector>
#include <cassert>
#include "mpp/MppException.h"
namespace mpp {
class RenderSystem {
public:
    std::vector<std::string> warnings;
    void warnMessage(std::string const& message) { warnings.push_back(message); }
};
}

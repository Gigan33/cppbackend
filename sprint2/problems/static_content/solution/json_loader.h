#pragma once

#include <string>
#include <vector>
#include "model.h"

namespace json_loader {

model::Game LoadGame(const std::string& json_path);

}  // namespace json_loader

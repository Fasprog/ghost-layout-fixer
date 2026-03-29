#pragma once

#include "CliTypes.h"

namespace ghost::cli {

class CliParser {
public:
    CliOptions parse(int argc, const char* const argv[]) const;
};

} // namespace ghost::cli

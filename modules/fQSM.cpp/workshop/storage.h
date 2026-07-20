#pragma once

#include <filesystem>
#include <fQSM/api/interface.h>

namespace placeholder {

    bool loadRegistry(fqsm::Writing context, std::filesystem::path dbPath);
    void saveRegistry(fqsm::Reading context, std::filesystem::path dbPath);

}

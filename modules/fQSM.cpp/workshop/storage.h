#pragma once

#include <filesystem>

#include <fQSM/api/interface.h>

namespace placeholder {

    void saveRegistry(fqsm::Reading context, std::filesystem::path dbPath);

}

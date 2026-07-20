#pragma once

#include <filesystem>
#include <fQSM/model/intertype/set.h>
#include <fQSM/processing/_forwards.h>

namespace fqsm::processing {

    struct Archivist {
        virtual ~Archivist()=default;

        using Location = std::filesystem::path;
        using Palette = model::intertype::Set;

        virtual Palette getTypesAtLocation(Reading, Location) = 0;
        virtual bool updateFromLocation(Writing, Palette, Location) = 0; // adds data from source to current model state
        virtual bool replaceFromLocation(Writing, Palette, Location) = 0;
        virtual bool saveToLocation(Writing, Palette, Location) = 0;
    };
}
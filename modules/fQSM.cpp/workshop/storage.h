#pragma once

#include <memory>
#include <unordered_map>

#include <fQSM/api/interface.h>
#include <fQSM/processing/persistency/archivist.h>

namespace fqsm_workshop::storage {

    class DatabaseProxy;

    // Workshop draft of the future Schema Node archive-slot.
    // Typed side: ArchiveOpsFor<Meta> with four methods.
    // Schema/catalog side: shared_ptr<ArchiveOps> from of<Meta>().
    struct ArchiveOps {
        virtual ~ArchiveOps() = default;

        virtual bool present(DatabaseProxy&) = 0;
        virtual void clear(fqsm::Writing) = 0;
        virtual void pull(fqsm::Writing, DatabaseProxy&) = 0;
        virtual void push(fqsm::Reading, DatabaseProxy&) = 0;

        template<fqsm::meta::category::Any Meta>
        static auto of() -> std::shared_ptr<ArchiveOps>;
    };

    struct DatabaseArchivist : fqsm::processing::Archivist {
        using Catalog = std::unordered_map<fqsm::meta::Rtid, std::shared_ptr<ArchiveOps>, fqsm::meta::Rtid::Hash>;

        explicit DatabaseArchivist(Catalog catalog);

        auto getTypesAtLocation(fqsm::Reading, Location) -> Palette override;
        bool updateFromLocation(fqsm::Writing, Palette, Location) override;
        bool replaceFromLocation(fqsm::Writing, Palette, Location) override;
        bool saveToLocation(fqsm::Writing, Palette, Location) override;

    private:
        Catalog catalog;
    };

}

#pragma once

#include <filesystem>
#include <memory>
#include <utility>

#include <sqlite3.h>

#include <fQSM/meta/categories.h>
#include <fQSM/processing/persistency/archivist.h>
#include <pQRF/database/retrospection.h>

#include <fQSM/processing/persistency/schema.h>
#include <fQSM/processing/_forwards.h>

namespace fqsm::processing::persistency::database {

    class DatabaseProxy {
    public:
        explicit DatabaseProxy(sqlite3* engine) : engine_(engine) {}
        DatabaseProxy(const DatabaseProxy&) = delete;
        auto operator=(const DatabaseProxy&) -> DatabaseProxy& = delete;
        DatabaseProxy(DatabaseProxy&& other) noexcept : engine_(std::exchange(other.engine_, nullptr)) {}
        auto operator=(DatabaseProxy&& other) noexcept -> DatabaseProxy& {
            if (this != &other) {
                if (engine_) sqlite3_close(engine_);
                engine_ = std::exchange(other.engine_, nullptr);
            }
            return *this;
        }
        ~DatabaseProxy() {
            if (engine_) sqlite3_close(engine_);
        }

        auto engine() const -> sqlite3* { return engine_; }

    private:
        sqlite3* engine_ = nullptr;
    };

    struct ArchiveOps : AspectArchive {
        virtual ~ArchiveOps() = default;

        virtual bool present(DatabaseProxy&) = 0;
        virtual void clear(Writing) = 0;
        virtual void pull(Writing, DatabaseProxy&) = 0;
        virtual void push(Reading, DatabaseProxy&) = 0;

        template<meta::category::Any Meta>
            requires HasRetrospection<Meta>
        static auto of() -> std::shared_ptr<ArchiveOps>;
    };

    struct DatabaseArchivist : Archivist {
        explicit DatabaseArchivist(Schema schema) : Archivist(std::move(schema)) {}

        auto getTypesAtLocation(Reading, Location) -> Palette override;
        bool updateFromLocation(Writing, Palette, Location) override;
        bool replaceFromLocation(Writing, Palette, Location) override;
        bool saveToLocation(Writing, Palette, Location) override;
    };

}

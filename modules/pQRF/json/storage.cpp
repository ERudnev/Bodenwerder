#include <fstream>
#include <optional>
#include <sstream>

#include <base/logging.h>

#include <pQRF/json/engine.h>

namespace fqsm::processing::persistency::json {

    using namespace fqsm::api;

    namespace {

        auto read_file(const std::filesystem::path& path) -> std::optional<std::string> {
            if (!std::filesystem::exists(path)) return std::nullopt;
            std::ifstream input(path, std::ios::binary);
            if (!input) return std::nullopt;
            std::ostringstream buffer;
            buffer << input.rdbuf();
            return buffer.str();
        }

        auto open_existing(const std::filesystem::path& path) -> std::optional<JsonDocument> {
            const auto text = read_file(path);
            if (!text) return std::nullopt;
            try {
                return JsonDocument{parse(*text)};
            } catch (...) {
                return std::nullopt;
            }
        }

        auto ops_for(const Graph::Node& node) -> std::shared_ptr<ArchiveOps> {
            return std::dynamic_pointer_cast<ArchiveOps>(node.archive);
        }

    }

    auto JsonArchivist::getTypesAtLocation(Reading, Location location) -> Palette {
        Palette found;
        auto document = open_existing(location);
        if (!document) return found;

        for (const auto& [type, node] : schema_->nodes) {
            const auto ops = ops_for(node);
            if (!ops) continue;
            if (ops->present(*document))
                found.insert(type);
        }
        return found;
    }

    bool JsonArchivist::updateFromLocation(Writing context, Palette palette, Location location) {
        auto document = open_existing(location);
        if (!document) return false;

        bool loaded = false;
        for (const auto& type : palette) {
            const auto found = schema_->nodes.find(type);
            if (found == schema_->nodes.end()) continue;
            const auto ops = ops_for(found->second);
            if (!ops) continue;
            if (!ops->present(*document)) continue;
            ops->pull(context, *document);
            loaded = true;
        }
        return loaded;
    }

    bool JsonArchivist::replaceFromLocation(Writing context, Palette palette, Location location) {
        for (const auto& type : palette) {
            const auto found = schema_->nodes.find(type);
            if (found == schema_->nodes.end()) continue;
            const auto ops = ops_for(found->second);
            if (!ops) continue;
            ops->clear(context);
        }
        return updateFromLocation(context, std::move(palette), std::move(location));
    }

    bool JsonArchivist::saveToLocation(Writing context, Palette palette, Location location) {
        std::filesystem::create_directories(location.parent_path());

        JsonDocument document{};
        const Reading reading = context;
        for (const auto& type : palette) {
            const auto found = schema_->nodes.find(type);
            if (found == schema_->nodes.end()) continue;
            const auto ops = ops_for(found->second);
            if (!ops) continue;
            ops->push(reading, document);
        }

        std::ofstream output(location, std::ios::binary | std::ios::trunc);
        if (!output) return false;
        output << stringify(document.root());
        if (!output) return false;

        base::message("saved registry to {}", location.string());
        return true;
    }

}

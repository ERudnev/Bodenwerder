#include <fstream>
#include <optional>
#include <sstream>
#include <string_view>

#include <base/logging.h>

#include <pQRF/json/engine.h>

namespace fqsm::processing::persistency::json {

    using namespace fqsm::api;

    namespace {

        auto ops_for(const Graph::Node& node) -> std::shared_ptr<ArchiveOps> {
            return std::dynamic_pointer_cast<ArchiveOps>(node.archive);
        }

        auto read_all(std::istream& input) -> std::optional<std::string> {
            if (!input) return std::nullopt;
            std::ostringstream buffer;
            buffer << input.rdbuf();
            if (!input && !input.eof()) return std::nullopt;
            return buffer.str();
        }

        auto open_from_text(std::string_view text) -> std::optional<JsonDocument> {
            try {
                return JsonDocument{parse(text)};
            } catch (...) {
                return std::nullopt;
            }
        }

        auto open_from_stream(std::istream& input) -> std::optional<JsonDocument> {
            const auto text = read_all(input);
            if (!text) return std::nullopt;
            return open_from_text(*text);
        }

        auto open_existing(const std::filesystem::path& path) -> std::optional<JsonDocument> {
            if (!std::filesystem::exists(path)) return std::nullopt;
            std::ifstream input(path, std::ios::binary);
            return open_from_stream(input);
        }

    }

    auto JsonArchivist::capture(Reading context, Palette palette) const -> JsonDocument {
        JsonDocument document{};
        for (const auto& type : palette) {
            const auto found = schema_->nodes.find(type);
            if (found == schema_->nodes.end()) continue;
            const auto ops = ops_for(found->second);
            if (!ops) continue;
            ops->push(context, document);
        }
        return document;
    }

    auto JsonArchivist::to_string(Reading context, Palette palette) const -> std::string {
        return stringify(capture(context, std::move(palette)).root());
    }

    bool JsonArchivist::saveToStream(Reading context, Palette palette, std::ostream& output) const {
        if (!output) return false;
        output << to_string(context, std::move(palette));
        return static_cast<bool>(output);
    }

    bool JsonArchivist::updateFromStream(Writing context, Palette palette, std::istream& input) const {
        auto document = open_from_stream(input);
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

        std::ofstream output(location, std::ios::binary | std::ios::trunc);
        if (!output) return false;
        if (!saveToStream(context, std::move(palette), output)) return false;

        base::message("saved registry to {}", location.string());
        return true;
    }

}

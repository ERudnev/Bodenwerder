#include <rmmr/resources/texts.q1.h>

#include <base/logging.h>
#include <base/maybe.h>

#include <filesystem>
#include <format>
#include <fstream>
#include <iterator>
#include <sstream>
#include <string>

namespace rmmr::resource::text {

    using namespace fqsm::api;

    namespace {

        auto readFile(const std::filesystem::path& path) -> maybe<std::string> {
            std::ifstream input(path, std::ios::binary);
            if (not input)
                return {};
            return std::string{std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
        }

        auto trim(string line) -> string {
            const auto first = line.find_first_not_of(" \t\r");
            if (first == string::npos)
                return {};
            const auto last = line.find_last_not_of(" \t\r");
            return line.substr(first, last - first + 1);
        }

        auto parseLines(const string& body) -> vector<string> {
            vector<string> lines{};
            std::istringstream stream(body);
            string raw;
            while (std::getline(stream, raw)) {
                const auto line = trim(std::move(raw));
                if (line.empty() or line.front() == '#')
                    continue;
                lines.push_back(line);
            }
            return lines;
        }

    }

    void LoaderCatalog::Actions::load(Writing context, Id packId) {
        const auto& loader = with<LoaderCatalog>::get(context, packId);
        const auto& unit = with<Unit>::get(context, packId);
        const auto dirPath = with<Manager>::resolve(context, unit, loader.directory);
        base::whisper("rmmr: text::LoaderCatalog '{}' ← {}", unit.name.text(), dirPath.string());
        if (not std::filesystem::is_directory(dirPath))
            return (void)context.refuse(std::format("resource::text::LoaderCatalog::load: '{}' is not a directory", dirPath.string()));
        const auto basename = dirPath.filename().string();
        if (basename != unit.name.own)
            return (void)context.refuse(std::format("resource::text::LoaderCatalog::load: directory basename '{}' != pack own name '{}'", basename, unit.name.own));
        umap<string, Pack::Document> documents{};
        for (const auto& entry : std::filesystem::directory_iterator(dirPath)) {
            if (not entry.is_regular_file())
                continue;
            if (entry.path().extension() != ".txt")
                continue;
            const auto body = readFile(entry.path());
            if (not body)
                return (void)context.refuse(std::format("resource::text::LoaderCatalog::load: failed to read '{}'", entry.path().string()));
            documents.insert_or_assign(entry.path().stem().string(), Pack::Document{.lines = parseLines(*body)});
        }
        if (documents.empty())
            return (void)context.refuse(std::format("resource::text::LoaderCatalog::load: '{}' has no .txt files", dirPath.string()));
        with<Pack>::modify(context, packId)->documents = std::move(documents);
        base::message("rmmr: text '{}' loaded ({} files from '{}')", unit.name.text(), with<Pack>::get(context, packId).documents.size(), dirPath.string());
    }

    auto doctrine::texts() -> Schema {
        return ask::schema::merge({
            ask::schema::aspect<Pack>(),
            ask::schema::aspect<LoaderCatalog>(),
        });
    }

}

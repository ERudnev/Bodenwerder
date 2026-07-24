#include <rmmr/resources/sprites.q1.h>

#include <base/logging.h>

#include <filesystem>
#include <format>
#include <fstream>
#include <string>
#include <string_view>

namespace rmmr::resource::sprite {

    using namespace fqsm::api;

    namespace {

        auto resolve_under_manager(const Manager::Quantum& manager, const Unit::Quantum& unit, const filename& relative) -> filepath {
            const std::filesystem::path file_path(relative);
            if (file_path.is_absolute()) {
                return file_path;
            }
            if (unit.library.empty()) {
                return manager.location / file_path;
            }
            return manager.location / unit.library / file_path;
        }

        auto read_attr_int(std::string_view tag, std::string_view key) -> maybe<integer> {
            const auto needle = std::string(key) + "=\"";
            const auto start = tag.find(needle);
            if (start == std::string_view::npos) {
                return {};
            }
            const auto value_begin = start + needle.size();
            const auto value_end = tag.find('"', value_begin);
            if (value_end == std::string_view::npos) {
                return {};
            }
            try {
                return static_cast<integer>(std::stoi(std::string(tag.substr(value_begin, value_end - value_begin))));
            } catch (...) {
                return {};
            }
        }

        auto parse_kenney_entries(const filepath& descriptor_path) -> maybe<vector<Pack::Entry>> {
            std::ifstream input(descriptor_path, std::ios::binary);
            if (not input) {
                return {};
            }
            const std::string text{
                std::istreambuf_iterator<char>(input),
                std::istreambuf_iterator<char>(),
            };

            vector<Pack::Entry> entries;
            constexpr std::string_view open = "<SubTexture";
            for (std::size_t cursor = 0; cursor < text.size();) {
                const auto tag_begin = text.find(open, cursor);
                if (tag_begin == std::string::npos) {
                    break;
                }
                const auto tag_end = text.find('>', tag_begin);
                if (tag_end == std::string::npos) {
                    break;
                }
                const auto tag = std::string_view(text).substr(tag_begin, tag_end - tag_begin);
                const auto x = read_attr_int(tag, "x");
                const auto y = read_attr_int(tag, "y");
                const auto width = read_attr_int(tag, "width");
                const auto height = read_attr_int(tag, "height");
                if (not x or not y or not width or not height) {
                    return {};
                }
                const index2 min{*x, *y};
                const index2 max{*x + *width, *y + *height};
                entries.push_back(Pack::Entry{
                    .min = min,
                    .max = max,
                    .pivot = index2{(min.x + max.x) / 2, (min.y + max.y) / 2},
                });
                cursor = tag_end + 1;
            }
            return entries;
        }

    } // namespace

    void LoaderKenney::Actions::load(Writing context, Id pack_id) {
        const auto& loader = with<LoaderKenney>::get(context, pack_id);
        const auto& unit = with<Unit>::get(context, pack_id);
        const auto& manager = with<Manager>::get(context, unit.manager);
        const auto descriptor_path = resolve_under_manager(manager, unit, loader.descriptor);
        const auto entries = parse_kenney_entries(descriptor_path);
        if (not entries) {
            context.refuse(std::format(
                "resource::sprite::LoaderKenney::load: failed to parse Kenney atlas '{}'",
                descriptor_path.string()));
            return;
        }
        with<Pack>::modify(context, pack_id)->entries = std::move(*entries);
    }

    auto Pack::Actions::materialize(Writing, Id, system::Device::Id) -> optional<Runtime::Id> {
        base::message("resource::sprite::Pack::materialize: nothing materialized");
        return {};
    }

}

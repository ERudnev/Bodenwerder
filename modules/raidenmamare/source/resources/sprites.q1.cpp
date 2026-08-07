#include <rmmr/resources/sprites.q1.h>
#include <rmmr/resources/runtimes.q1.h>

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <base/logging.h>

#include <filesystem>
#include <format>
#include <fstream>
#include <string>
#include <string_view>

namespace rmmr::resource::sprite {

    using namespace fqsm::api;

    namespace {

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

        void release_gl(Writing context, const Runtime::Quantum& last) {
            if (not last.entries_buffer) {
                return;
            }
            const auto& device_quantum = with<system::Device>::get(context, last.device);
            glfwMakeContextCurrent(device_quantum.handle);
            auto entries_buffer = last.entries_buffer;
            glDeleteBuffers(1, &entries_buffer);
        }

        auto install_runtime(Writing context, system::Device::Id device, Pack::Id pack_id, Runtime::Quantum quantum) -> Runtime::Id {
            const auto& runtimes = with<Runtimes>::get(context, device);
            if (const auto existing = runtimes.sprites_id_mapping.find(pack_id); existing != runtimes.sprites_id_mapping.end()) {
                if (with<Runtime>::exists(context, existing->second)) {
                    auto runtime = with<Runtime>::modify(context, existing->second);
                    release_gl(context, *runtime);
                    *runtime = std::move(quantum);
                    return existing->second;
                }
            }
            return with<SpriteRuntime_group>::addElement(context, device, std::move(quantum));
        }

    } // namespace

    void LoaderKenney::Actions::load(Writing context, Id pack_id) {
        const auto& loader = with<LoaderKenney>::get(context, pack_id);
        const auto& unit = with<Unit>::get(context, pack_id);
        const auto descriptor_path = with<Manager>::resolve(context, unit, loader.descriptor);
        base::whisper("rmmr: sprite::LoaderKenney '{}' ← {}", unit.name.text(), descriptor_path.string());
        const auto entries = parse_kenney_entries(descriptor_path);
        if (not entries) {
            return (void)context.refuse(std::format(
                "resource::sprite::LoaderKenney::load: '{}' failed to parse Kenney atlas '{}'",
                unit.name.text(),
                descriptor_path.string()));
        }
        if (entries->empty()) {
            return (void)context.refuse(std::format(
                "resource::sprite::LoaderKenney::load: '{}' atlas '{}' has no SubTexture entries",
                unit.name.text(),
                descriptor_path.string()));
        }
        with<Pack>::modify(context, pack_id)->entries = std::move(*entries);
    }

    auto Pack::Actions::materialize(Writing context, Id pack_id, system::Device::Id device) -> optional<Runtime::Id> {
        const auto& pack = with<Pack>::get(context, pack_id);
        if (pack.entries.empty()) {
            const auto& unit = with<Unit>::get(context, pack_id);
            return context.refuse(std::format(
                "resource::sprite::Pack::materialize: '{}' entries are empty (Pack never filled — LoaderKenney::load did not run or its branch was rejected)",
                unit.name.text()));
        }

        const auto& runtimes = with<Runtimes>::get(context, device);
        const auto texture_it = runtimes.textures_id_mapping.find(pack.texture.id);
        if (texture_it == runtimes.textures_id_mapping.end()) {
            return context.refuse("resource::sprite::Pack::materialize: texture runtime missing");
        }

        const auto& device_quantum = with<system::Device>::get(context, device);
        glfwMakeContextCurrent(device_quantum.handle);

        vector<GLint> payload;
        payload.reserve(pack.entries.size() * std::size_t{8});
        for (const auto& entry : pack.entries) {
            payload.push_back(static_cast<GLint>(entry.min.x));
            payload.push_back(static_cast<GLint>(entry.min.y));
            payload.push_back(static_cast<GLint>(entry.max.x - entry.min.x));
            payload.push_back(static_cast<GLint>(entry.max.y - entry.min.y));
            payload.push_back(static_cast<GLint>(entry.pivot.x));
            payload.push_back(static_cast<GLint>(entry.pivot.y));
            payload.push_back(GLint{0});
            payload.push_back(GLint{0});
        }

        renderer::VertexBuffer entries_buffer{};
        glCreateBuffers(1, &entries_buffer);
        if (not entries_buffer) {
            return context.refuse("resource::sprite::Pack::materialize: glCreateBuffers failed");
        }

        glNamedBufferData(
            entries_buffer,
            static_cast<renderer::SizePtr>(payload.size() * sizeof(GLint)),
            payload.data(),
            GL_STATIC_DRAW);

        return install_runtime(context, device, pack_id, Runtime::Quantum{
            .device = device,
            .texture = texture_it->second,
            .entries_buffer = entries_buffer,
            .count = static_cast<integer>(pack.entries.size()),
        });
    }

    struct Runtime::Internals : Runtime::DefaultInternals {
        static void release(Writing context, Id, const Quantum& last) {
            release_gl(context, last);
        }
    };

    auto Runtime::customAspectReactions() -> const Behavior {
        return {
            reaction::deletion<Runtime>(&Runtime::Internals::release),
        };
    }

}

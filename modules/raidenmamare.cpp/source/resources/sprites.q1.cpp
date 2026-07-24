#include <rmmr/resources/sprites.q1.h>
#include <rmmr/resources/runtimes.q1.h>

#include <GL/glew.h>
#include <GLFW/glfw3.h>

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

        void release_gl(Writing context, const Runtime::Quantum& last) {
            const auto& device_quantum = with<system::Device>::get(context, last.device);
            glfwMakeContextCurrent(device_quantum.handle);
            if (last.entries_texture) {
                auto entries_texture = last.entries_texture;
                glDeleteTextures(1, &entries_texture);
            }
            if (last.entries_buffer) {
                auto entries_buffer = last.entries_buffer;
                glDeleteBuffers(1, &entries_buffer);
            }
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

    auto Pack::Actions::materialize(Writing context, Id pack_id, system::Device::Id device) -> optional<Runtime::Id> {
        const auto& pack = with<Pack>::get(context, pack_id);
        if (pack.entries.empty()) {
            return context.refuse("resource::sprite::Pack::materialize: entries are empty");
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
        glGenBuffers(1, &entries_buffer);
        if (not entries_buffer) {
            return context.refuse("resource::sprite::Pack::materialize: glGenBuffers failed");
        }

        glBindBuffer(GL_TEXTURE_BUFFER, entries_buffer);
        glBufferData(
            GL_TEXTURE_BUFFER,
            static_cast<renderer::SizePtr>(payload.size() * sizeof(GLint)),
            payload.data(),
            GL_STATIC_DRAW);
        glBindBuffer(GL_TEXTURE_BUFFER, 0);

        renderer::Texture entries_texture{};
        glGenTextures(1, &entries_texture);
        if (not entries_texture) {
            glDeleteBuffers(1, &entries_buffer);
            return context.refuse("resource::sprite::Pack::materialize: glGenTextures failed");
        }

        glBindTexture(GL_TEXTURE_BUFFER, entries_texture);
        glTexBuffer(GL_TEXTURE_BUFFER, GL_RGBA32I, entries_buffer);
        glBindTexture(GL_TEXTURE_BUFFER, 0);

        return install_runtime(context, device, pack_id, Runtime::Quantum{
            .device = device,
            .texture = texture_it->second,
            .entries_buffer = entries_buffer,
            .entries_texture = entries_texture,
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

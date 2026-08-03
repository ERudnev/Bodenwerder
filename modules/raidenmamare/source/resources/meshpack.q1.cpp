#include <rmmr/resources/meshpack.q1.h>
#include <rmmr/resources/runtimes.q1.h>

#include <base/logging.h>
#include <base/serialization.h>

#include <filesystem>
#include <format>
#include <fstream>

namespace rmmr::resource::meshpack {

    using namespace fqsm::api;

    namespace {

        auto resolve_under_manager(const Manager::Quantum& manager, const Unit::Quantum& unit, const filename& relative) -> filepath {
            const std::filesystem::path file_path(relative);
            if (file_path.is_absolute()) {
                return file_path;
            }
            if (unit.name.library.empty()) {
                return manager.location / file_path;
            }
            return manager.location / unit.name.library / file_path;
        }

        struct FilePartBinding {
            string alias;
            string materialInstancePlaceholder;
        };

        struct FileEntryBody {
            string geometry_file;
            umap<string, FilePartBinding> materials;
        };

        struct FilePayload {
            string name;
            string library;
            umap<string, string> materials;
            umap<string, FileEntryBody> entries;
        };

        auto read_payload(std::istream& in) -> FilePayload {
            FilePayload out{};
            base::serialization::detail::expect(in, '{');
            out.name = base::serialization::detail::read<string>(in);
            base::serialization::detail::expect(in, ',');
            out.library = base::serialization::detail::read<string>(in);
            base::serialization::detail::expect(in, ',');
            out.materials = base::serialization::detail::read<umap<string, string>>(in);
            base::serialization::detail::expect(in, ',');
            out.entries = base::serialization::detail::read<umap<string, FileEntryBody>>(in);
            base::serialization::detail::expect(in, '}');
            return out;
        }

        auto find_material(Reading context, const string& library, const string& name) -> optional<material::Asset::Id> {
            for (const auto [id, unit] : context->aspect<Unit>().items()) {
                if (unit.name.library != library or unit.name.own != name) {
                    continue;
                }
                if (not with<material::Asset>::exists(context, id)) {
                    continue;
                }
                return id;
            }
            return {};
        }

    } // namespace

    auto Asset::Actions::resolve(Reading context, Id pack_id, string name) -> optional<Resolved> {
        if (not with<Asset>::exists(context, pack_id)) {
            return {};
        }
        const auto& pack = with<Asset>::get(context, pack_id);
        const auto entry_it = pack.entries.find(name);
        if (entry_it == pack.entries.end()) {
            return {};
        }
        umap<string, material::Asset::Id> materials;
        for (const auto& [part, binding] : entry_it->second.materials) {
            const auto alias_it = pack.materials.find(binding.alias);
            if (alias_it == pack.materials.end()) {
                return {};
            }
            materials.emplace(part, alias_it->second);
        }
        return Resolved{
            .geometry = entry_it->second.geometry,
            .materials = std::move(materials),
        };
    }

    void Loader::Actions::load(Writing context, Id pack_id) {
        const auto& loader = with<Loader>::get(context, pack_id);
        const auto& unit = with<Unit>::get(context, pack_id);
        const auto manager_id = with<Manager>::singleton(context);
        if (not manager_id) {
            return (void)context.refuse("resource::meshpack::Loader::load: Manager singleton missing");
        }
        const auto& manager = with<Manager>::get(context, *manager_id);
        const auto path = resolve_under_manager(manager, unit, loader.file);
        base::whisper("rmmr: meshpack::Loader '{}' ← {}", unit.name.text(), path.string());

        std::ifstream in{path};
        if (not in) {
            return (void)context.refuse(std::format("resource::meshpack::Loader::load: failed to open '{}'", path.string()));
        }

        FilePayload payload;
        try {
            payload = read_payload(in);
        } catch (const std::exception& error) {
            return (void)context.refuse(std::format("resource::meshpack::Loader::load: parse '{}': {}", path.string(), error.what()));
        }

        const auto file_name = Unit::Name{.library = payload.library, .own = payload.name};
        if (file_name != unit.name) {
            return (void)context.refuse(std::format("resource::meshpack::Loader::load: file identity '{}' != unit '{}'", file_name.text(), unit.name.text()));
        }

        umap<string, material::Asset::Id> materials;
        for (const auto& [alias, material_name] : payload.materials) {
            const auto material_id = find_material(context, unit.name.library, material_name);
            if (not material_id) {
                return (void)context.refuse(std::format("resource::meshpack::Loader::load: material '{}' not found for alias '{}'", Unit::Name{.library = unit.name.library, .own = material_name}.text(), alias));
            }
            materials.emplace(alias, *material_id);
        }

        umap<string, Asset::Entry> entries;
        for (const auto& [entry_name, body] : payload.entries) {
            const auto geometry_id = with<Assets>::add_geometry_loader(
                context,
                Unit::name(unit.name.library, entry_name),
                geometry::Loader::Quantum{.file = body.geometry_file});
            umap<string, Asset::Entry::Binding> part_bindings;
            for (const auto& [part, binding] : body.materials) {
                if (materials.find(binding.alias) == materials.end()) {
                    return (void)context.refuse(std::format("resource::meshpack::Loader::load: entry '{}' part '{}' unknown alias '{}'", entry_name, part, binding.alias));
                }
                part_bindings.emplace(part, Asset::Entry::Binding{
                    .alias = binding.alias,
                    .materialInstancePlaceholder = binding.materialInstancePlaceholder,
                });
            }
            entries.emplace(entry_name, Asset::Entry{
                .geometry = geometry_id,
                .materials = std::move(part_bindings),
            });
            base::message("rmmr: meshpack '{}' entry '{}' ← geometry '{}' ({})", unit.name.text(), entry_name, Unit::Name{.library = unit.name.library, .own = entry_name}.text(), body.geometry_file);
        }

        auto& pack = *with<Asset>::modify(context, pack_id);
        pack.materials = std::move(materials);
        pack.entries = std::move(entries);
        base::message("rmmr: meshpack '{}' loaded ({} aliases, {} entries)", unit.name.text(), pack.materials.size(), pack.entries.size());
    }

}

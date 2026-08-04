#include <rmmr/resources/meshpack.q1.h>
#include <rmmr/resources/runtimes.q1.h>
#include <rmmr/system/content/loader_lwo.h>
#include <rmmr/system/content/unit_name.h>

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

        struct FileEntryBody {
            string geometry_file;
            umap<string, string> materials; // part → material Unit.own
        };

        struct FilePayload {
            string name;
            string library;
            umap<string, FileEntryBody> entries;
        };

        auto read_payload(std::istream& in) -> FilePayload {
            FilePayload out{};
            base::serialization::detail::expect(in, '{');
            out.name = base::serialization::detail::read<string>(in);
            base::serialization::detail::expect(in, ',');
            out.library = base::serialization::detail::read<string>(in);
            base::serialization::detail::expect(in, ',');
            out.entries = base::serialization::detail::read<umap<string, FileEntryBody>>(in);
            base::serialization::detail::expect(in, '}');
            return out;
        }

        struct LwoPackPayload {
            string name;
            string library;
            string lwo_file;
            umap<string, string> materials; // file material name → Unit::Name::text
        };

        auto read_lwo_pack_payload(std::istream& in) -> LwoPackPayload {
            LwoPackPayload out{};
            base::serialization::detail::expect(in, '{');
            out.name = base::serialization::detail::read<string>(in);
            base::serialization::detail::expect(in, ',');
            out.library = base::serialization::detail::read<string>(in);
            base::serialization::detail::expect(in, ',');
            out.lwo_file = base::serialization::detail::read<string>(in);
            base::serialization::detail::expect(in, ',');
            out.materials = base::serialization::detail::read<umap<string, string>>(in);
            base::serialization::detail::expect(in, '}');
            return out;
        }

        auto find_material(Reading context, const Unit::Name& name) -> optional<material::Asset::Id> {
            for (const auto [id, unit] : context->aspect<Unit>().items()) {
                if (unit.name != name) {
                    continue;
                }
                if (not with<material::Asset>::exists(context, id)) {
                    continue;
                }
                return id;
            }
            return {};
        }

        auto find_material(Reading context, const string& library, const string& own) -> optional<material::Asset::Id> {
            return find_material(context, Unit::Name{.library = library, .own = own});
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
        return Resolved{
            .geometry = entry_it->second.geometry,
            .materials = entry_it->second.materials,
        };
    }

    void LoaderObjs::Actions::load(Writing context, Id pack_id) {
        const auto& loader = with<LoaderObjs>::get(context, pack_id);
        const auto& unit = with<Unit>::get(context, pack_id);
        const auto manager_id = with<Manager>::singleton(context);
        if (not manager_id) {
            return (void)context.refuse("resource::meshpack::LoaderObjs::load: Manager singleton missing");
        }
        const auto& manager = with<Manager>::get(context, *manager_id);
        const auto path = resolve_under_manager(manager, unit, loader.file);
        base::whisper("rmmr: meshpack::LoaderObjs '{}' ← {}", unit.name.text(), path.string());

        std::ifstream in{path};
        if (not in) {
            return (void)context.refuse(std::format("resource::meshpack::LoaderObjs::load: failed to open '{}'", path.string()));
        }

        FilePayload payload;
        try {
            payload = read_payload(in);
        } catch (const std::exception& error) {
            return (void)context.refuse(std::format("resource::meshpack::LoaderObjs::load: parse '{}': {}", path.string(), error.what()));
        }

        const auto file_name = Unit::Name{.library = payload.library, .own = payload.name};
        if (file_name != unit.name) {
            return (void)context.refuse(std::format("resource::meshpack::LoaderObjs::load: file identity '{}' != unit '{}'", file_name.text(), unit.name.text()));
        }

        umap<string, Asset::Entry> entries;
        for (const auto& [entry_name, body] : payload.entries) {
            const auto geometry_id = with<Assets>::add_geometry_loader(
                context,
                Unit::name(unit.name.library, entry_name),
                geometry::Loader::Quantum{.file = body.geometry_file, .layer = string{}});
            umap<string, material::Asset::Id> part_materials;
            for (const auto& [part, material_own] : body.materials) {
                const auto material_id = find_material(context, unit.name.library, material_own);
                if (not material_id) {
                    return (void)context.refuse(std::format("resource::meshpack::LoaderObjs::load: entry '{}' part '{}' material '{}' not found", entry_name, part, Unit::Name{.library = unit.name.library, .own = material_own}.text()));
                }
                part_materials.emplace(part, *material_id);
            }
            entries.emplace(entry_name, Asset::Entry{
                .geometry = geometry_id,
                .materials = std::move(part_materials),
            });
            base::message("rmmr: meshpack '{}' entry '{}' ← geometry '{}' ({})", unit.name.text(), entry_name, Unit::Name{.library = unit.name.library, .own = entry_name}.text(), body.geometry_file);
        }

        const auto entry_count = entries.size();
        with<Asset>::modify(context, pack_id)->entries = std::move(entries);
        base::message("rmmr: meshpack '{}' loaded ({} entries)", unit.name.text(), entry_count);
    }

    void LoaderLwo::Actions::load(Writing context, Id pack_id) {
        const auto& loader = with<LoaderLwo>::get(context, pack_id);
        const auto& unit = with<Unit>::get(context, pack_id);
        const auto manager_id = with<Manager>::singleton(context);
        if (not manager_id) {
            return (void)context.refuse("resource::meshpack::LoaderLwo::load: Manager singleton missing");
        }

        const auto& manager = with<Manager>::get(context, *manager_id);
        const auto pack_path = resolve_under_manager(manager, unit, loader.file);
        base::whisper("rmmr: meshpack::LoaderLwo '{}' ← {}", unit.name.text(), pack_path.string());

        std::ifstream in{pack_path};
        if (not in) {
            return (void)context.refuse(std::format("resource::meshpack::LoaderLwo::load: failed to open '{}'", pack_path.string()));
        }

        LwoPackPayload payload;
        try {
            payload = read_lwo_pack_payload(in);
        } catch (const std::exception& error) {
            return (void)context.refuse(std::format("resource::meshpack::LoaderLwo::load: parse '{}': {}", pack_path.string(), error.what()));
        }

        const auto file_name = Unit::Name{.library = payload.library, .own = payload.name};
        if (file_name != unit.name) {
            return (void)context.refuse(std::format("resource::meshpack::LoaderLwo::load: file identity '{}' != unit '{}'", file_name.text(), unit.name.text()));
        }

        const auto lwo_path = resolve_under_manager(manager, unit, payload.lwo_file);
        auto opened = system::content::LwoDocument::open(lwo_path);
        if (not opened.document) {
            return (void)context.refuse(std::format("resource::meshpack::LoaderLwo::load: {}", opened.error));
        }
        const auto& lwo = *opened.document;
        if (lwo.meshes().empty()) {
            return (void)context.refuse(std::format("resource::meshpack::LoaderLwo::load: no meshes in '{}'", lwo_path.string()));
        }

        umap<string, Asset::Entry> entries;
        for (const auto& mesh : lwo.meshes()) {
            const auto geometry_id = with<Assets>::add_geometry_loader(
                context,
                Unit::name(unit.name.library, mesh.name),
                geometry::Loader::Quantum{.file = payload.lwo_file, .layer = mesh.name});

            umap<string, material::Asset::Id> part_materials;
            for (const auto& sub : mesh.submeshes) {
                const auto map_it = payload.materials.find(sub.name);
                if (map_it == payload.materials.end()) {
                    return (void)context.refuse(std::format("resource::meshpack::LoaderLwo::load: mesh '{}' submesh '{}' not in pack table", mesh.name, sub.name));
                }
                const auto parsed = system::content::UnitName::parse(map_it->second);
                if (not parsed) {
                    return (void)context.refuse(std::format("resource::meshpack::LoaderLwo::load: material '{}' → bad Unit::Name '{}'", sub.name, map_it->second));
                }
                const auto material_id = find_material(context, Unit::Name{.library = parsed->library, .own = parsed->own});
                if (not material_id) {
                    return (void)context.refuse(std::format("resource::meshpack::LoaderLwo::load: material '{}' → '{}' not on shelf", sub.name, parsed->text()));
                }
                part_materials.emplace(sub.name, *material_id);
            }
            if (part_materials.empty()) {
                return (void)context.refuse(std::format("resource::meshpack::LoaderLwo::load: mesh '{}' has no submeshes", mesh.name));
            }

            const auto part_count = part_materials.size();
            entries.emplace(mesh.name, Asset::Entry{
                .geometry = geometry_id,
                .materials = std::move(part_materials),
            });
            base::message("rmmr: meshpack '{}' entry '{}' ← LWO mesh ({} submeshes)", unit.name.text(), mesh.name, part_count);
        }

        const auto entry_count = entries.size();
        with<Asset>::modify(context, pack_id)->entries = std::move(entries);
        base::message("rmmr: meshpack '{}' loaded ({} LWO entries)", unit.name.text(), entry_count);
    }

}

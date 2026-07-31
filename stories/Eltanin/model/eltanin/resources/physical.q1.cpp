#include <eltanin/resources/physical.q1.h>
#include <rmmr/resources/manager.q1.h>

#include <base/logging.h>
#include <base/serialization.h>

#include <filesystem>
#include <fstream>
#include <format>

namespace eltanin::resource::physical {

    using namespace fqsm::api;
    using rmmr::resource::Manager;
    using rmmr::resource::Unit;

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

        auto read_pos(std::istream& in) -> rmmr::Pos {
            base::serialization::detail::expect(in, '{');
            const auto x = base::serialization::detail::read<float>(in);
            base::serialization::detail::expect(in, ',');
            const auto y = base::serialization::detail::read<float>(in);
            base::serialization::detail::expect(in, ',');
            const auto z = base::serialization::detail::read<float>(in);
            base::serialization::detail::expect(in, '}');
            return rmmr::Pos{x, y, z};
        }

        struct FilePayload {
            string name;
            string library;
            Asset::Quantum asset;
        };

        auto read_payload(std::istream& in) -> FilePayload {
            FilePayload out{};
            base::serialization::detail::expect(in, '{');
            out.name = base::serialization::detail::read<string>(in);
            base::serialization::detail::expect(in, ',');
            out.library = base::serialization::detail::read<string>(in);
            base::serialization::detail::expect(in, ',');
            base::serialization::detail::read_sequence(in, '[', ']', [&] {
                out.asset.points.push_back(read_pos(in));
            });
            base::serialization::detail::expect(in, ',');
            out.asset.faces = base::serialization::detail::read<vector<Asset::Face>>(in);
            base::serialization::detail::expect(in, '}');
            return out;
        }

    } // namespace

    void Loader::Actions::load(Writing context, Id id) {
        const auto& loader = with<Loader>::get(context, id);
        const auto& unit = with<Unit>::get(context, id);
        const auto manager_id = with<Manager>::singleton(context);
        if (not manager_id) return (void)context.refuse("eltanin::resource::physical::Loader::load: Manager singleton missing");
        const auto& manager = with<Manager>::get(context, *manager_id);

        const auto path = resolve_under_manager(manager, unit, loader.file);
        base::message("eltanin: physical::Loader '{}/{}' ← {}", unit.library, unit.name, path.string());

        std::ifstream in{path};
        if (not in) {
            return (void)context.refuse(std::format("eltanin::resource::physical::Loader::load: failed to open '{}'", path.string()));
        }

        FilePayload payload;
        try {
            payload = read_payload(in);
        } catch (const std::exception& error) {
            return (void)context.refuse(std::format("eltanin::resource::physical::Loader::load: parse '{}': {}", path.string(), error.what()));
        }

        if (payload.name != unit.name or payload.library != unit.library) {
            return (void)context.refuse(std::format(
                "eltanin::resource::physical::Loader::load: file identity '{}/{}' != unit '{}/{}'",
                payload.library,
                payload.name,
                unit.library,
                unit.name));
        }

        *with<Asset>::modify(context, id) = std::move(payload.asset);
    }

}

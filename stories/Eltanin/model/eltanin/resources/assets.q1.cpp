#include <eltanin/resources/assets.q1.h>
#include <eltanin/resources/geometry.q1.h>
#include <rmmr/resources/geometry.q1.h>
#include <rmmr/resources/manager.q1.h>

#include <base/logging.h>
#include <base/serialization.h>

#include <filesystem>
#include <fstream>
#include <format>

namespace eltanin::resource {

    using namespace fqsm::api;
    using rmmr::resource::Manager;
    using rmmr::resource::Unit;
    using rmmr::resource::Unit_group;

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
            vector<rmmr::Pos> points;
            vector<atomic::Asset::Face> faces;
        };

        auto read_payload(std::istream& in) -> FilePayload {
            FilePayload out{};
            base::serialization::detail::expect(in, '{');
            out.name = base::serialization::detail::read<string>(in);
            base::serialization::detail::expect(in, ',');
            out.library = base::serialization::detail::read<string>(in);
            base::serialization::detail::expect(in, ',');
            base::serialization::detail::read_sequence(in, '[', ']', [&] {
                out.points.push_back(read_pos(in));
            });
            base::serialization::detail::expect(in, ',');
            out.faces = base::serialization::detail::read<vector<atomic::Asset::Face>>(in);
            base::serialization::detail::expect(in, '}');
            return out;
        }

        auto host_assets(Writing context) -> optional<rmmr::resource::Assets::Id> {
            const auto host = with<rmmr::resource::Assets>::singleton(context);
            if (not host) {
                context.refuse("eltanin::resource::Assets: rmmr Assets singleton missing");
                return {};
            }
            if (not with<Assets>::exists(context, *host)) {
                context.refuse("eltanin::resource::Assets: missing; create in createCore wave with rmmr::Assets");
                return {};
            }
            if (not with<Unit_group>::exists(context, *host)) {
                with<Unit_group>::extend(context, *host);
            }
            return host;
        }

    } // namespace

    auto Assets::Actions::add_atomic(Writing context, Unit::Quantum unit, filename file) -> atomic::Asset::Id {
        const auto host = host_assets(context);
        if (not host) return context.refuse("eltanin::resource::Assets::add_atomic: host missing");

        const auto manager_id = with<Manager>::singleton(context);
        if (not manager_id) return context.refuse("eltanin::resource::Assets::add_atomic: Manager singleton missing");
        const auto& manager = with<Manager>::get(context, *manager_id);

        const auto path = resolve_under_manager(manager, unit, file);
        base::message("eltanin: atomic '{}/{}' ← {}", unit.library, unit.name, path.string());

        std::ifstream in{path};
        if (not in) {
            return context.refuse(std::format("eltanin::resource::Assets::add_atomic: failed to open '{}'", path.string()));
        }

        FilePayload payload;
        try {
            payload = read_payload(in);
        } catch (const std::exception& error) {
            return context.refuse(std::format("eltanin::resource::Assets::add_atomic: parse '{}': {}", path.string(), error.what()));
        }

        if (payload.name != unit.name or payload.library != unit.library) {
            return context.refuse(std::format(
                "eltanin::resource::Assets::add_atomic: file identity '{}/{}' != unit '{}/{}'",
                payload.library,
                payload.name,
                unit.library,
                unit.name));
        }

        const auto name = unit.name;
        const auto library = unit.library;
        const auto visualizer_name = name + "_visualizer";

        const auto atomic_id = with<Unit_group>::addElement(context, *host, std::move(unit));
        const auto visualizer_id = with<Unit_group>::addElement(context, *host, Unit::Quantum{
            .name = visualizer_name,
            .library = library,
        });
        with<rmmr::resource::geometry::Asset>::extend(context, visualizer_id, rmmr::resource::geometry::Asset::Quantum{});
        with<::eltanin::resources::AtomicVisualizer>::extend(context, visualizer_id, ::eltanin::resources::AtomicVisualizer::Quantum{
            .source = atomic_id,
        });
        with<atomic::Asset>::extend(context, atomic_id, atomic::Asset::Quantum{
            .points = std::move(payload.points),
            .faces = std::move(payload.faces),
            .visualizer = visualizer_id,
        });
        base::message("eltanin: atomic '{}/{}' + visualizer '{}/{}'", library, name, library, visualizer_name);
        return atomic_id;
    }

}

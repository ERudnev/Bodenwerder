#include <eltanin/resources/blueprint.q1.h>

#include <rmmr/resources/manager.q1.h>

#include <base/logging.h>

#include <cctype>
#include <format>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <unordered_map>

namespace eltanin::resource::blueprint {

    using namespace fqsm::api;

    namespace {

        struct Cursor {
            std::string_view text;
            std::size_t at;
        };

        void skip_ws(Cursor& cursor) {
            while (cursor.at < cursor.text.size() and std::isspace(static_cast<unsigned char>(cursor.text[cursor.at])))
                ++cursor.at;
        }

        void expect(Cursor& cursor, char ch) {
            skip_ws(cursor);
            if (cursor.at >= cursor.text.size() or cursor.text[cursor.at] != ch)
                throw std::runtime_error(std::format("blueprint: expected '{}'", ch));
            ++cursor.at;
        }

        auto take_string(Cursor& cursor) -> string {
            skip_ws(cursor);
            expect(cursor, '"');
            string out;
            while (cursor.at < cursor.text.size() and cursor.text[cursor.at] != '"') {
                out.push_back(cursor.text[cursor.at]);
                ++cursor.at;
            }
            expect(cursor, '"');
            return out;
        }

        auto take_int(Cursor& cursor) -> integer {
            skip_ws(cursor);
            const auto begin = cursor.at;
            if (cursor.at < cursor.text.size() and (cursor.text[cursor.at] == '-' or cursor.text[cursor.at] == '+'))
                ++cursor.at;
            while (cursor.at < cursor.text.size() and std::isdigit(static_cast<unsigned char>(cursor.text[cursor.at])))
                ++cursor.at;
            if (begin == cursor.at or (cursor.at - begin == 1 and (cursor.text[begin] == '-' or cursor.text[begin] == '+')))
                throw std::runtime_error("blueprint: expected integer");
            return static_cast<integer>(std::stoi(std::string{cursor.text.substr(begin, cursor.at - begin)}));
        }

        auto take_index3(Cursor& cursor) -> base::common_types::index3 {
            expect(cursor, '[');
            const auto x = take_int(cursor);
            expect(cursor, ',');
            const auto y = take_int(cursor);
            expect(cursor, ',');
            const auto z = take_int(cursor);
            expect(cursor, ']');
            return base::common_types::index3{.x = x, .y = y, .z = z};
        }

        const std::unordered_map<std::string_view, mech::subframe::corner::kind> knotKinds{
            {"c124", mech::subframe::corner::kind::c124},
            {"c1364", mech::subframe::corner::kind::c1364},
            {"c164", mech::subframe::corner::kind::c164},
            {"c134", mech::subframe::corner::kind::c134},
            {"c135", mech::subframe::corner::kind::c135},
            {"c12", mech::subframe::corner::kind::c12},
            {"c13", mech::subframe::corner::kind::c13},
            {"c15", mech::subframe::corner::kind::c15},
            {"c16", mech::subframe::corner::kind::c16},
            {"c34", mech::subframe::corner::kind::c34},
            {"c35", mech::subframe::corner::kind::c35},
        };

        const std::unordered_map<std::string_view, mech::subframe::halfEdge::kind> halfChordKinds{
            {"he1deg90", mech::subframe::halfEdge::kind::he1deg90},
            {"he1deg45", mech::subframe::halfEdge::kind::he1deg45},
            {"he3deg71", mech::subframe::halfEdge::kind::he3deg71},
            {"he3deg90", mech::subframe::halfEdge::kind::he3deg90},
            {"he3deg125", mech::subframe::halfEdge::kind::he3deg125},
        };

        const std::unordered_map<std::string_view, mech::subframe::membrane::kind> wallKinds{
            {"u1111", mech::subframe::membrane::kind::u1111},
            {"u121", mech::subframe::membrane::kind::u121},
            {"u2121", mech::subframe::membrane::kind::u2121},
            {"u222A", mech::subframe::membrane::kind::u222A},
            {"u222V", mech::subframe::membrane::kind::u222V},
        };

        auto knotKindName(mech::subframe::corner::kind kind) -> std::string_view {
            for (const auto& [name, value] : knotKinds) {
                if (value == kind)
                    return name;
            }
            throw std::runtime_error("blueprint: unknown knot kind");
        }

        auto halfChordKindName(mech::subframe::halfEdge::kind kind) -> std::string_view {
            for (const auto& [name, value] : halfChordKinds) {
                if (value == kind)
                    return name;
            }
            throw std::runtime_error("blueprint: unknown half-chord kind");
        }

        auto wallKindName(mech::subframe::membrane::kind kind) -> std::string_view {
            for (const auto& [name, value] : wallKinds) {
                if (value == kind)
                    return name;
            }
            throw std::runtime_error("blueprint: unknown wall kind");
        }

        auto take_knot(Cursor& cursor) -> mech::quarks::Knot {
            expect(cursor, '[');
            const auto kindName = take_string(cursor);
            const auto kindIt = knotKinds.find(kindName);
            if (kindIt == knotKinds.end())
                throw std::runtime_error(std::format("blueprint: unknown knot kind '{}'", kindName));
            expect(cursor, ',');
            const auto pos = take_index3(cursor);
            expect(cursor, ',');
            const auto ori = take_int(cursor);
            expect(cursor, ']');
            return mech::quarks::Knot{
                .kind = kindIt->second,
                .pose = mech::space::cell::Pose{.pos = pos, .ori = static_cast<rmmr::renderer::Signed32>(ori)},
            };
        }

        auto take_half_chord(Cursor& cursor) -> mech::quarks::HalfChord {
            expect(cursor, '[');
            const auto kindName = take_string(cursor);
            const auto kindIt = halfChordKinds.find(kindName);
            if (kindIt == halfChordKinds.end())
                throw std::runtime_error(std::format("blueprint: unknown half-chord kind '{}'", kindName));
            expect(cursor, ',');
            const auto poleName = take_string(cursor);
            mech::subframe::halfEdge::Pole pole = mech::subframe::halfEdge::Pole::s;
            if (poleName == "s")
                pole = mech::subframe::halfEdge::Pole::s;
            else if (poleName == "e")
                pole = mech::subframe::halfEdge::Pole::e;
            else
                throw std::runtime_error(std::format("blueprint: unknown half-chord pole '{}'", poleName));
            expect(cursor, ',');
            const auto pos = take_index3(cursor);
            expect(cursor, ',');
            const auto ori = take_int(cursor);
            expect(cursor, ']');
            return mech::quarks::HalfChord{
                .kind = kindIt->second,
                .pole = pole,
                .pose = mech::space::cell::Pose{.pos = pos, .ori = static_cast<rmmr::renderer::Signed32>(ori)},
            };
        }

        auto take_wall(Cursor& cursor) -> mech::quarks::Wall {
            expect(cursor, '[');
            const auto kindName = take_string(cursor);
            const auto kindIt = wallKinds.find(kindName);
            if (kindIt == wallKinds.end())
                throw std::runtime_error(std::format("blueprint: unknown wall kind '{}'", kindName));
            expect(cursor, ',');
            const auto pos = take_index3(cursor);
            expect(cursor, ',');
            const auto ori = take_int(cursor);
            expect(cursor, ']');
            return mech::quarks::Wall{
                .kind = kindIt->second,
                .pose = mech::space::cell::Pose{.pos = pos, .ori = static_cast<rmmr::renderer::Signed32>(ori)},
            };
        }

        template <typename Item, typename Take>
        auto take_list(Cursor& cursor, Take take) -> std::vector<Item> {
            expect(cursor, '[');
            skip_ws(cursor);
            std::vector<Item> out;
            if (cursor.at < cursor.text.size() and cursor.text[cursor.at] == ']') {
                ++cursor.at;
                return out;
            }
            for (;;) {
                out.push_back(take(cursor));
                skip_ws(cursor);
                if (cursor.at < cursor.text.size() and cursor.text[cursor.at] == ']') {
                    ++cursor.at;
                    break;
                }
                expect(cursor, ',');
            }
            return out;
        }

        auto parse_blueprint(std::string_view text) -> mech::Blueprint {
            Cursor cursor{.text = text, .at = 0};
            expect(cursor, '{');
            auto name = take_string(cursor);
            expect(cursor, ',');
            auto author = take_string(cursor);
            skip_ws(cursor);
            std::vector<mech::quarks::Knot> knots;
            std::vector<mech::quarks::HalfChord> halfChords;
            std::vector<mech::quarks::Wall> walls;
            if (cursor.at < cursor.text.size() and cursor.text[cursor.at] == ',') {
                ++cursor.at;
                knots = take_list<mech::quarks::Knot>(cursor, take_knot);
                skip_ws(cursor);
                if (cursor.at < cursor.text.size() and cursor.text[cursor.at] == ',') {
                    ++cursor.at;
                    halfChords = take_list<mech::quarks::HalfChord>(cursor, take_half_chord);
                    skip_ws(cursor);
                    if (cursor.at < cursor.text.size() and cursor.text[cursor.at] == ',') {
                        ++cursor.at;
                        walls = take_list<mech::quarks::Wall>(cursor, take_wall);
                    }
                }
            }
            expect(cursor, '}');
            return mech::Blueprint{.name = std::move(name), .author = std::move(author), .frame = {.knots = std::move(knots), .halfChords = std::move(halfChords)}, .hull = {.walls = std::move(walls)}};
        }

        auto format_knot(const mech::quarks::Knot& knot) -> std::string {
            return std::format("[\"{}\", [{}, {}, {}], {}]", knotKindName(knot.kind), knot.pose.pos.x, knot.pose.pos.y, knot.pose.pos.z, knot.pose.ori);
        }

        auto format_half_chord(const mech::quarks::HalfChord& halfChord) -> std::string {
            const char* pole = halfChord.pole == mech::subframe::halfEdge::Pole::s ? "s" : "e";
            return std::format("[\"{}\", \"{}\", [{}, {}, {}], {}]", halfChordKindName(halfChord.kind), pole, halfChord.pose.pos.x, halfChord.pose.pos.y, halfChord.pose.pos.z, halfChord.pose.ori);
        }

        auto format_wall(const mech::quarks::Wall& wall) -> std::string {
            return std::format("[\"{}\", [{}, {}, {}], {}]", wallKindName(wall.kind), wall.pose.pos.x, wall.pose.pos.y, wall.pose.pos.z, wall.pose.ori);
        }

        template <typename Item, typename Format>
        void format_list(std::ostringstream& out, const std::vector<Item>& items, Format format) {
            if (items.empty()) {
                out << "    []";
                return;
            }
            out << "    [\n";
            for (std::size_t i = 0; i < items.size(); ++i) {
                out << "        " << format(items[i]);
                out << (i + 1 < items.size() ? ",\n" : "\n");
            }
            out << "    ]";
        }

        auto format_blueprint(const mech::Blueprint& data) -> std::string {
            std::ostringstream out;
            out << "{\n";
            out << "    \"" << data.name << "\",\n";
            out << "    \"" << data.author << "\",\n";
            format_list(out, data.frame.knots, format_knot);
            out << ",\n";
            format_list(out, data.frame.halfChords, format_half_chord);
            out << ",\n";
            format_list(out, data.hull.walls, format_wall);
            out << "\n}\n";
            return out.str();
        }

    } // namespace

    void Loader::Actions::load(Writing context, Id id) {
        const auto& unit = with<rmmr::resource::Unit>::get(context, id);
        const auto& loader = with<Loader>::get(context, id);
        const auto path = with<rmmr::resource::Manager>::resolve(context, unit, loader.file);

        std::ifstream input{path};
        if (not input)
            return (void)context.refuse(std::format("resource::blueprint::Loader::load: cannot open '{}'", path.string()));

        std::ostringstream buffer;
        buffer << input.rdbuf();
        try {
            auto parsed = parse_blueprint(buffer.str());
            with<Asset>::modify(context, id)->data = std::move(parsed);
        } catch (const std::exception& error) {
            return (void)context.refuse(std::format("resource::blueprint::Loader::load: '{}': {}", path.string(), error.what()));
        }
    }

    void Loader::Actions::save(Writing context, Id id) {
        const auto& unit = with<rmmr::resource::Unit>::get(context, id);
        const auto& loader = with<Loader>::get(context, id);
        const auto& asset = with<Asset>::get(context, id);
        const auto path = with<rmmr::resource::Manager>::resolve(context, unit, loader.file);

        std::ofstream output{path, std::ios::binary | std::ios::trunc};
        if (not output)
            return (void)context.refuse(std::format("resource::blueprint::Loader::save: cannot open '{}'", path.string()));

        try {
            output << format_blueprint(asset.data);
            if (not output)
                return (void)context.refuse(std::format("resource::blueprint::Loader::save: write failed '{}'", path.string()));
        } catch (const std::exception& error) {
            return (void)context.refuse(std::format("resource::blueprint::Loader::save: '{}': {}", path.string(), error.what()));
        }
    }

}

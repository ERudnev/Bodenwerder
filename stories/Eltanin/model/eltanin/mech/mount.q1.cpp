#include <eltanin/mech/mount.q1.h>

#include <rmmr/resources/manager.q1.h>
#include <rmmr/system/content/unit_name.h>

#include <array>
#include <cctype>
#include <cstdint>
#include <format>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string_view>

namespace eltanin::mech {

    using namespace fqsm::api;

    auto Attachment::flatMounted() const -> bool {
        if (points.size() <= 2)
            return true;

        using i64 = std::int64_t;
        const auto sub = [](const base::common_types::index3& a, const base::common_types::index3& b) -> std::array<i64, 3> {
            return {static_cast<i64>(a.x) - b.x, static_cast<i64>(a.y) - b.y, static_cast<i64>(a.z) - b.z};
        };
        const auto cross = [](const std::array<i64, 3>& a, const std::array<i64, 3>& b) -> std::array<i64, 3> {
            return {a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]};
        };
        const auto dot = [](const std::array<i64, 3>& a, const std::array<i64, 3>& b) -> i64 {
            return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
        };
        const auto nonzero = [](const std::array<i64, 3>& v) -> bool {
            return v[0] != 0 or v[1] != 0 or v[2] != 0;
        };

        const auto& origin = points[0];
        std::array<i64, 3> edge1{};
        bool haveEdge = false;
        for (std::size_t i = 1; i < points.size(); ++i) {
            edge1 = sub(points[i], origin);
            if (nonzero(edge1)) {
                haveEdge = true;
                break;
            }
        }
        if (not haveEdge)
            return true;

        std::array<i64, 3> normal{};
        bool haveNormal = false;
        for (std::size_t i = 1; i < points.size(); ++i) {
            normal = cross(edge1, sub(points[i], origin));
            if (nonzero(normal)) {
                haveNormal = true;
                break;
            }
        }
        if (not haveNormal)
            return true;

        for (const auto& point : points) {
            if (dot(sub(point, origin), normal) != 0)
                return false;
        }
        return true;
    }

    namespace {

        struct Cursor {
            std::string_view text;
            std::size_t at;
        };

        void skip_ws(Cursor& cursor) {
            while (cursor.at < cursor.text.size()) {
                const auto ch = cursor.text[cursor.at];
                if (std::isspace(static_cast<unsigned char>(ch))) {
                    ++cursor.at;
                    continue;
                }
                if (ch == '/' and cursor.at + 1 < cursor.text.size() and cursor.text[cursor.at + 1] == '/') {
                    cursor.at += 2;
                    while (cursor.at < cursor.text.size() and cursor.text[cursor.at] != '\n')
                        ++cursor.at;
                    continue;
                }
                break;
            }
        }

        void expect(Cursor& cursor, char ch) {
            skip_ws(cursor);
            if (cursor.at >= cursor.text.size() or cursor.text[cursor.at] != ch)
                throw std::runtime_error(std::format("mount: expected '{}'", ch));
            ++cursor.at;
        }

        auto peek(Cursor& cursor) -> char {
            skip_ws(cursor);
            return cursor.at < cursor.text.size() ? cursor.text[cursor.at] : '\0';
        }

        auto take_string(Cursor& cursor) -> string {
            skip_ws(cursor);
            expect(cursor, '"');
            string out;
            while (cursor.at < cursor.text.size() and cursor.text[cursor.at] != '"') {
                if (cursor.text[cursor.at] == '\\' and cursor.at + 1 < cursor.text.size()) {
                    ++cursor.at;
                    out.push_back(cursor.text[cursor.at]);
                    ++cursor.at;
                    continue;
                }
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
                throw std::runtime_error("mount: expected integer");
            return static_cast<integer>(std::stoi(std::string{cursor.text.substr(begin, cursor.at - begin)}));
        }

        auto take_key(Cursor& cursor) -> string {
            const auto key = take_string(cursor);
            expect(cursor, ':');
            return key;
        }

        void expect_key(Cursor& cursor, std::string_view key) {
            if (take_key(cursor) != key)
                throw std::runtime_error(std::format("mount: expected key '{}'", key));
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

        auto take_attachment(Cursor& cursor) -> Attachment {
            expect(cursor, '{');
            expect_key(cursor, "points");
            expect(cursor, '[');
            Attachment attachment{.points = {}};
            if (peek(cursor) != ']') {
                for (;;) {
                    attachment.points.push_back(take_index3(cursor));
                    skip_ws(cursor);
                    if (peek(cursor) == ']')
                        break;
                    expect(cursor, ',');
                }
            }
            expect(cursor, ']');
            expect(cursor, '}');
            return attachment;
        }

        auto take_number(Cursor& cursor) -> float {
            skip_ws(cursor);
            const auto begin = cursor.at;
            if (cursor.at < cursor.text.size() and (cursor.text[cursor.at] == '-' or cursor.text[cursor.at] == '+'))
                ++cursor.at;
            const auto digitsStart = cursor.at;
            while (cursor.at < cursor.text.size() and std::isdigit(static_cast<unsigned char>(cursor.text[cursor.at])))
                ++cursor.at;
            if (cursor.at < cursor.text.size() and cursor.text[cursor.at] == '.') {
                ++cursor.at;
                while (cursor.at < cursor.text.size() and std::isdigit(static_cast<unsigned char>(cursor.text[cursor.at])))
                    ++cursor.at;
            }
            if (begin == cursor.at or digitsStart == cursor.at)
                throw std::runtime_error("mount: expected number");
            return std::stof(std::string{cursor.text.substr(begin, cursor.at - begin)});
        }

        auto take_int_array(Cursor& cursor) -> vector<integer> {
            expect(cursor, '[');
            vector<integer> values;
            if (peek(cursor) != ']') {
                for (;;) {
                    values.push_back(take_int(cursor));
                    if (peek(cursor) == ']')
                        break;
                    expect(cursor, ',');
                }
            }
            expect(cursor, ']');
            return values;
        }

        auto take_collision(Cursor& cursor) -> Collision {
            expect(cursor, '{');
            expect_key(cursor, "thickness");
            const auto thickness = take_number(cursor);
            expect(cursor, ',');
            expect_key(cursor, "faces");
            expect(cursor, '[');
            vector<vector<integer>> faces;
            if (peek(cursor) != ']') {
                for (;;) {
                    faces.push_back(take_int_array(cursor));
                    if (peek(cursor) == ']')
                        break;
                    expect(cursor, ',');
                }
            }
            expect(cursor, ']');
            expect(cursor, '}');
            return Collision{.thickness = thickness, .faces = std::move(faces)};
        }

        auto take_temp_mesh(Cursor& cursor) -> Mount::TempMesh {
            expect(cursor, '{');
            expect_key(cursor, "pack");
            const auto packText = take_string(cursor);
            const auto parsed = rmmr::system::content::UnitName::parse(packText);
            if (not parsed)
                throw std::runtime_error(std::format("mount: bad pack Unit::Name '{}'", packText));
            expect(cursor, ',');
            expect_key(cursor, "entry");
            const auto entry = take_string(cursor);
            expect(cursor, '}');
            return Mount::TempMesh{
                .pack = rmmr::resource::Unit::Name::from(parsed->library, parsed->own),
                .entry = entry,
            };
        }

        auto take_role(Cursor& cursor) -> Role {
            const auto text = take_string(cursor);
            if (text == "custom") return Role::custom;
            if (text == "propulsion") return Role::propulsion;
            if (text == "power") return Role::power;
            if (text == "gyros") return Role::gyros;
            if (text == "weaponry") return Role::weaponry;
            if (text == "cargo") return Role::cargo;
            if (text == "logistic") return Role::logistic;
            if (text == "emissive") return Role::emissive;
            if (text == "control") return Role::control;
            if (text == "living") return Role::living;
            throw std::runtime_error(std::format("mount: unknown role '{}'", text));
        }

        auto role_text(Role role) -> std::string_view {
            switch (role) {
                case Role::custom: return "custom";
                case Role::propulsion: return "propulsion";
                case Role::power: return "power";
                case Role::gyros: return "gyros";
                case Role::weaponry: return "weaponry";
                case Role::cargo: return "cargo";
                case Role::logistic: return "logistic";
                case Role::emissive: return "emissive";
                case Role::control: return "control";
                case Role::living: return "living";
            }
            throw std::runtime_error("mount: bad role");
        }

        auto parse_mount(std::string_view text) -> Mount::Quantum {
            Cursor cursor{.text = text, .at = 0};
            expect(cursor, '{');
            expect_key(cursor, "name");
            auto name = take_string(cursor);
            expect(cursor, ',');
            expect_key(cursor, "author");
            auto author = take_string(cursor);
            expect(cursor, ',');
            expect_key(cursor, "attachment");
            auto attachment = take_attachment(cursor);
            expect(cursor, ',');
            expect_key(cursor, "collision");
            auto collision = take_collision(cursor);
            expect(cursor, ',');
            expect_key(cursor, "tempMesh");
            auto tempMesh = take_temp_mesh(cursor);
            base::maybe<Role> role;
            if (peek(cursor) == ',') {
                expect(cursor, ',');
                expect_key(cursor, "role");
                role = take_role(cursor);
            }
            expect(cursor, '}');
            return Mount::Quantum{
                .name = std::move(name),
                .author = std::move(author),
                .attachment = std::move(attachment),
                .collision = collision,
                .tempMesh = std::move(tempMesh),
                .role = role,
                .file = {},
            };
        }

        auto format_mount(const Mount::Quantum& data) -> std::string {
            std::ostringstream out;
            out << "{\n";
            out << "  \"name\": \"" << data.name << "\",\n";
            out << "  \"author\": \"" << data.author << "\",\n";
            out << "  \"attachment\": {\n";
            out << "    \"points\": [\n";
            for (std::size_t i = 0; i < data.attachment.points.size(); ++i) {
                const auto& point = data.attachment.points[i];
                out << "      [" << point.x << ", " << point.y << ", " << point.z << "]" << (i + 1 < data.attachment.points.size() ? ",\n" : "\n");
            }
            out << "    ]\n";
            out << "  },\n";
            out << "  \"collision\": {\n";
            out << "    \"thickness\": " << data.collision.thickness << ",\n";
            out << "    \"faces\": [\n";
            for (std::size_t face = 0; face < data.collision.faces.size(); ++face) {
                const auto& loop = data.collision.faces[face];
                out << "      [";
                for (std::size_t index = 0; index < loop.size(); ++index)
                    out << loop[index] << (index + 1 < loop.size() ? ", " : "");
                out << "]" << (face + 1 < data.collision.faces.size() ? ",\n" : "\n");
            }
            out << "    ]\n";
            out << "  },\n";
            out << "  \"tempMesh\": {\n";
            out << "    \"pack\": \"" << data.tempMesh.pack.text() << "\",\n";
            out << "    \"entry\": \"" << data.tempMesh.entry << "\"\n";
            out << "  }";
            if (data.role.has_value())
                out << ",\n  \"role\": \"" << role_text(*data.role) << "\"\n";
            else
                out << "\n";
            out << "}\n";
            return out.str();
        }

    } // namespace

    void Mount::Actions::load(Writing context, Id id) {
        const auto& unit = with<rmmr::resource::Unit>::get(context, id);
        const auto& mount = with<Mount>::get(context, id);
        const auto path = with<rmmr::resource::Manager>::resolve(context, unit, mount.file);

        std::ifstream input{path};
        if (not input)
            return (void)context.refuse(std::format("mech::Mount::load: cannot open '{}'", path.string()));

        std::ostringstream buffer;
        buffer << input.rdbuf();
        try {
            auto parsed = parse_mount(buffer.str());
            auto writable = with<Mount>::modify(context, id);
            const auto file = writable->file;
            *writable = std::move(parsed);
            writable->file = file;
        } catch (const std::exception& error) {
            return (void)context.refuse(std::format("mech::Mount::load: '{}': {}", path.string(), error.what()));
        }
    }

    void Mount::Actions::save(Writing context, Id id) {
        const auto& unit = with<rmmr::resource::Unit>::get(context, id);
        const auto& mount = with<Mount>::get(context, id);
        const auto path = with<rmmr::resource::Manager>::resolve(context, unit, mount.file);

        std::ofstream output{path, std::ios::binary | std::ios::trunc};
        if (not output)
            return (void)context.refuse(std::format("mech::Mount::save: cannot open '{}'", path.string()));

        try {
            output << format_mount(mount);
            if (not output)
                return (void)context.refuse(std::format("mech::Mount::save: write failed '{}'", path.string()));
        } catch (const std::exception& error) {
            return (void)context.refuse(std::format("mech::Mount::save: '{}': {}", path.string(), error.what()));
        }
    }

}

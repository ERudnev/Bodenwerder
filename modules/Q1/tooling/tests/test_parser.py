from __future__ import annotations

import json
import sys
from pathlib import Path

TOOLING_DIR = Path(__file__).resolve().parents[1]
if str(TOOLING_DIR) not in sys.path:
    sys.path.insert(0, str(TOOLING_DIR))

import parser as q1_parser


ROOT = TOOLING_DIR.parents[2]
ASPECTS = ROOT / "modules" / "Q1" / "golden" / "doctrine" / "aspects.q1"
ELEMENTARY = ROOT / "modules" / "Q1" / "golden" / "doctrine" / "elementary.q1"


def test_golden_aspects_parses() -> None:
    ast = q1_parser.parse_file(ASPECTS)
    assert ast["kind"] == "File"
    assert ast["declarations"][0]["kind"] == "NamespaceDecl"
    assert ast["declarations"][0]["name"] == "Q1_fQSM"


def test_golden_elementary_parses() -> None:
    ast = q1_parser.parse_file(ELEMENTARY)
    assert ast["kind"] == "File"
    assert ast["declarations"][0]["kind"] == "NamespaceDecl"
    assert ast["declarations"][0]["name"] == "Q1_iQSM"


def test_json_dump_top_level_shape_is_stable() -> None:
    ast = q1_parser.parse_file(ASPECTS)
    dumped = json.loads(json.dumps(ast, sort_keys=True))
    decls = dumped["declarations"]
    assert [decl["kind"] for decl in decls] == ["NamespaceDecl"]
    assert [decl["name"] for decl in decls] == ["Q1_fQSM"]
    inner = decls[0]["declarations"][0]
    assert inner["kind"] == "NamespaceDecl"
    assert inner["name"] == "Etalon"


def test_parse_one_line_entity_and_typeof() -> None:
    text = """
namespace Demo
  entity Note one text: string
  archetype Notebook
    >add_note(#Note, ~Note::text) -> #Note
"""
    ast = q1_parser.parse_text(text, source="<snippet>")
    outer = ast["declarations"][0]
    note = outer["declarations"][0]
    notebook = outer["declarations"][1]
    assert note["kind"] == "AspectDecl"
    assert note["blocks"][0]["role"] == "one"
    field = note["blocks"][0]["members"][0]
    assert field["name"] == "text"
    op = notebook["members"][0]
    assert op["params"][0]["type"]["kind"] == "IdType"
    assert op["params"][1]["type"]["kind"] == "MemberTypeOf"


def test_parse_affects_link_type() -> None:
    text = """
namespace Demo
  entity Sample
    one
      data_field: integer
  entity Reminder
    one
      target: affects<Sample>
      trigger_value: integer
"""
    ast = q1_parser.parse_text(text, source="<snippet>")
    reminder = ast["declarations"][0]["declarations"][1]
    field = reminder["blocks"][0]["members"][0]
    assert field["name"] == "target"
    assert field["type"]["kind"] == "AffectsType"
    assert field["type"]["target"]["kind"] == "NamedType"
    assert field["type"]["target"]["parts"] == ["Sample"]


def test_golden_reminder_uses_affects() -> None:
    ast = q1_parser.parse_file(ASPECTS)
    etalon = ast["declarations"][0]["declarations"][0]
    reminder = next(
        decl for decl in etalon["declarations"]
        if decl.get("kind") == "AspectDecl" and decl.get("name") == "Reminder"
    )
    target = next(
        m for m in reminder["blocks"][0]["members"]
        if m.get("kind") == "FieldDecl" and m.get("name") == "target"
    )
    assert target["type"]["kind"] == "AffectsType"
    assert target["type"]["target"]["parts"] == ["SampleEntity"]


def test_parse_reaction_scopes() -> None:
    text = """
namespace Demo
  entity A
    one
      x: integer
      !drop(-one)
      !local(=one)
      !ctx(>one)
    all
      !wide(~)
      !foreign(~Tag)
"""
    ast = q1_parser.parse_text(text, source="<snippet>")
    entity = ast["declarations"][0]["declarations"][0]
    one_reactions = [m for m in entity["blocks"][0]["members"] if m["kind"] == "ReactionDecl"]
    all_reactions = [m for m in entity["blocks"][1]["members"] if m["kind"] == "ReactionDecl"]
    assert [r["scope"]["raw"] for r in one_reactions] == ["-one", "=one", ">one"]
    assert [r["scope"]["raw"] for r in all_reactions] == ["~", "~Tag"]
    assert all(r.get("effect") is None for r in one_reactions + all_reactions)


def test_parse_steward_operations() -> None:
    text = """
namespace Demo
  entity SampleEntity
    one
      data_field: integer
  attribute SpeedUpdate of SampleEntity
    all
      *active_maintenance_call(~)
      *active_working_call(~SampleEntity, tick_count: integer)
"""
    ast = q1_parser.parse_text(text, source="<snippet>")
    speed = ast["declarations"][0]["declarations"][1]
    ops = [m for m in speed["blocks"][0]["members"] if m["kind"] == "StewardOp"]
    assert [op["name"] for op in ops] == ["active_maintenance_call", "active_working_call"]
    assert ops[0]["scope"]["raw"] == "~"
    assert ops[0]["params"] == []
    assert ops[0]["return_type"] is None
    assert ops[1]["scope"]["raw"] == "~SampleEntity"
    assert ops[1]["scope"]["extra_source"] == "SampleEntity"
    assert len(ops[1]["params"]) == 1
    assert ops[1]["params"][0]["kind"] == "NamedParam"
    assert ops[1]["params"][0]["name"] == "tick_count"
    assert ops[1]["params"][0]["type"]["name"] == "integer"


def test_parse_steward_rejects_missing_scope() -> None:
    text = """
namespace Demo
  entity A
    all
      *no_scope()
"""
    try:
        q1_parser.parse_text(text, source="<snippet>")
        raise AssertionError("expected ParseError")
    except q1_parser.ParseError as exc:
        assert "scope" in str(exc).lower()


def test_golden_aspects_parses_speed_update_steward_ops() -> None:
    ast = q1_parser.parse_file(ASPECTS)
    decls = ast["declarations"][0]["declarations"][0]["declarations"]
    speed = next(d for d in decls if d.get("name") == "SpeedUpdate")
    all_block = next(b for b in speed["blocks"] if b["role"] == "all")
    stewards = [m for m in all_block["members"] if m["kind"] == "StewardOp"]
    assert [s["name"] for s in stewards] == ["active_maintenance_call", "active_working_call"]
    assert stewards[0]["scope"]["raw"] == "~"
    assert stewards[1]["scope"]["extra_source"] == "SampleEntity"
    assert stewards[1]["params"][0]["name"] == "tick_count"


def test_parse_reaction_effect_tails() -> None:
    text = """
namespace Demo
  entity Clock
    one
      t: time
  entity Sketch
    one
      value: string
      !normalize(=one)
      !watch_clock(~Clock)->=field_update()
      !watch_self(~)
      !watch_sample(~SampleEntity)->>reflex()
      !watch_trigger(~SampleEntity)->>field_action()
    all
      >field_action()
"""
    ast = q1_parser.parse_text(text, source="<snippet>")
    sketch = ast["declarations"][0]["declarations"][1]
    one = [m for m in sketch["blocks"][0]["members"] if m["kind"] == "ReactionDecl"]
    by_name = {r["name"]: r for r in one}
    assert by_name["normalize"]["effect"] is None
    assert by_name["watch_self"]["effect"] is None
    clock = by_name["watch_clock"]["effect"]
    assert clock["effect_kind"] == "command"
    assert clock["op_prefix"] == "="
    assert clock["name"] == "field_update"
    reflex = by_name["watch_sample"]["effect"]
    assert reflex["effect_kind"] == "effector"
    assert reflex["op_prefix"] == ">"
    assert reflex["name"] == "reflex"
    action = by_name["watch_trigger"]["effect"]
    assert action["name"] == "field_action"
    assert by_name["watch_sample"]["scope"]["raw"] == "~SampleEntity"


def test_parse_reaction_rejects_plain_value_return() -> None:
    text = """
namespace Demo
  entity A
    one
      !bad(~)->integer
"""
    try:
        q1_parser.parse_text(text, source="<snippet>")
        assert False, "expected ParseError"
    except q1_parser.ParseError as exc:
        assert "->>" in str(exc) or "->=" in str(exc)

def test_parse_external_type_expression() -> None:
    text = """
namespace Demo
  using ExternalDomainType @external(similar to OpenGL texture handle)
  struct Resource
    handle: @external(opengl_window)
"""
    ast = q1_parser.parse_text(text, source="<snippet>")
    decls = ast["declarations"][0]["declarations"]
    alias = decls[0]
    assert alias["mode"] == "external"
    assert alias["target"]["kind"] == "ExternalType"
    assert alias["target"]["description"] == "similar to OpenGL texture handle"
    field = decls[1]["members"][0]
    assert field["type"]["kind"] == "ExternalType"
    assert field["type"]["description"] == "opengl_window"


def test_golden_elementary_external_type_alias() -> None:
    ast = q1_parser.parse_file(ELEMENTARY)
    typization = ast["declarations"][0]["declarations"][0]["declarations"][0]
    external = next(
        decl for decl in typization["declarations"] if decl.get("name") == "ExternalDomainType"
    )
    assert external["mode"] == "external"
    assert external["target"]["kind"] == "ExternalType"
    assert "OpenGL texture handle" in external["target"]["description"]


def test_parse_quantum_type_of_expression() -> None:
    text = """
namespace rmmr
  namespace scene
    manipulation Interface of Core
      >createRawNode(#, one<Node>) -> #Node
      >createCamera(#, one<Node>, one<Camera>) -> #Camera
"""
    ast = q1_parser.parse_text(text, source="<snippet>")
    iface = ast["declarations"][0]["declarations"][0]["declarations"][0]
    assert iface["category"] == "manipulation"
    assert iface["owner"] == "Core"
    raw_node = iface["members"][0]
    create_camera = iface["members"][1]
    assert raw_node["params"][0]["type"]["kind"] == "IdType"
    assert raw_node["params"][0]["type"]["target"] is None
    assert raw_node["params"][1]["type"]["kind"] == "QuantumTypeOf"
    assert raw_node["params"][1]["type"]["target"]["raw"] == "Node"
    assert create_camera["params"][2]["type"]["kind"] == "QuantumTypeOf"
    assert create_camera["params"][2]["type"]["target"]["raw"] == "Camera"


def test_parse_typeof_using_alias() -> None:
    text = """
namespace Demo
  using AliasByField as ~Struct::field1
  struct Struct
    field1: integer
"""
    ast = q1_parser.parse_text(text, source="<snippet>")
    alias = ast["declarations"][0]["declarations"][0]
    assert alias["kind"] == "TypeAliasDecl"
    assert alias["name"] == "AliasByField"
    assert alias["mode"] == "typeof"
    assert alias["target"]["kind"] == "MemberTypeOf"
    assert alias["target"]["target"] == ["Struct"]
    assert alias["target"]["member"] == "field1"


def test_parse_feature_declaration() -> None:
    text = """
namespace Demo
  feature Tag of SampleEntity
    all
      modulus: integer
"""
    ast = q1_parser.parse_text(text, source="<snippet>")
    tag = ast["declarations"][0]["declarations"][0]
    assert tag["kind"] == "AspectDecl"
    assert tag["category"] == "feature"
    assert tag["name"] == "Tag"
    assert tag["owner"] == "SampleEntity"


def test_parse_attribute_declaration() -> None:
    text = """
namespace Demo
  attribute Tag of SampleEntity
    all
      modulus: integer
"""
    ast = q1_parser.parse_text(text, source="<snippet>")
    tag = ast["declarations"][0]["declarations"][0]
    assert tag["kind"] == "AspectDecl"
    assert tag["category"] == "attribute"
    assert tag["name"] == "Tag"
    assert tag["owner"] == "SampleEntity"


def test_parse_qualified_owner_declaration() -> None:
    text = """
namespace rmmr
  namespace controller
    attribute Camera of scene::Camera
      one
        clock: double
    component Dispatcher of system::Device
      one
        clock: double
    group<Window> of system::Device
"""
    ast = q1_parser.parse_text(text, source="<snippet>")
    controller = ast["declarations"][0]["declarations"][0]
    decls = controller["declarations"]
    camera = decls[0]
    dispatcher = decls[1]
    windows = decls[2]
    assert camera["owner"] == "scene::Camera"
    assert dispatcher["owner"] == "system::Device"
    assert windows["owner"] == "system::Device"


def test_parse_entity_local_struct() -> None:
    text = """
namespace rmmr
  namespace asset
    entity Geometry
      struct Channel
        using Id as @external(int)
        using Layout as integer
      one
        layout: Channel::Layout
"""
    ast = q1_parser.parse_text(text, source="<snippet>")
    asset = ast["declarations"][0]["declarations"][0]
    geometry = asset["declarations"][0]
    assert geometry["local_structs"][0]["name"] == "Channel"
    assert geometry["local_structs"][0]["members"][0]["name"] == "Id"


def test_parse_import_and_entity_local_type() -> None:
    text = """
import "window"

namespace rmmr
  entity Device
    using WindowHandle @external(OpenGL window C++ pointer aka "GLFwindow")
    one
      window: WindowHandle
      !deinit(-one)
"""
    ast = q1_parser.parse_text(text, source="<snippet>")
    assert ast["declarations"][0]["kind"] == "ImportDecl"
    assert ast["declarations"][0]["path"] == "window"
    device = ast["declarations"][1]["declarations"][0]
    assert device["local_types"][0]["name"] == "WindowHandle"
    assert device["local_types"][0]["mode"] == "external"


def test_parse_builtin_container_types() -> None:
    text = """
namespace Demo
  entity Box
    one
      seq: vector<integer>
      keys: set<string>
      bag: uset<boolean>
      pairs: map<string, integer>
      fast: umap<string, float>
"""
    ast = q1_parser.parse_text(text, source="<snippet>")
    box = ast["declarations"][0]["declarations"][0]
    fields = {m["name"]: m["type"] for m in box["blocks"][0]["members"] if m["kind"] == "FieldDecl"}

    assert fields["seq"]["kind"] == "ContainerType"
    assert fields["seq"]["name"] == "vector"
    assert fields["seq"]["params"][0]["name"] == "integer"

    assert fields["keys"]["name"] == "set"
    assert fields["bag"]["name"] == "uset"

    assert fields["pairs"]["name"] == "map"
    assert len(fields["pairs"]["params"]) == 2
    assert fields["pairs"]["params"][0]["name"] == "string"
    assert fields["pairs"]["params"][1]["name"] == "integer"

    assert fields["fast"]["name"] == "umap"
    assert fields["fast"]["params"][1]["name"] == "float"


def test_parse_filepath_and_filename_builtin_types() -> None:
    text = """
namespace Demo
  entity Asset
    one
      root: filepath
      name: filename
      >load(path: filepath, file: filename) -> #
"""
    ast = q1_parser.parse_text(text, source="<snippet>")
    asset = ast["declarations"][0]["declarations"][0]
    fields = {m["name"]: m["type"] for m in asset["blocks"][0]["members"] if m["kind"] == "FieldDecl"}

    assert fields["root"] == {"kind": "BuiltinType", "raw": "filepath", "name": "filepath"}
    assert fields["name"] == {"kind": "BuiltinType", "raw": "filename", "name": "filename"}

    factory = next(m for m in asset["blocks"][0]["members"] if m["kind"] == "FactoryOp")
    assert factory["params"][0]["type"]["name"] == "filepath"
    assert factory["params"][1]["type"]["name"] == "filename"


def test_parse_container_id_type_fields() -> None:
    text = """
namespace Demo
  entity A

  entity B
    one
      brother: #
      parents: vector<#A>
      siblings: vector<#>
"""
    ast = q1_parser.parse_text(text, source="<snippet>")
    decls = ast["declarations"][0]["declarations"]
    b = decls[1]
    fields = {m["name"]: m["type"] for m in b["blocks"][0]["members"] if m["kind"] == "FieldDecl"}

    assert fields["brother"]["kind"] == "IdType"
    assert fields["brother"]["target"] is None

    assert fields["parents"]["kind"] == "ContainerType"
    assert fields["parents"]["name"] == "vector"
    assert fields["parents"]["params"][0]["kind"] == "IdType"
    assert fields["parents"]["params"][0]["target"] == "A"

    assert fields["siblings"]["kind"] == "ContainerType"
    assert fields["siblings"]["params"][0]["kind"] == "IdType"
    assert fields["siblings"]["params"][0]["target"] is None


def test_parse_nested_container_types() -> None:
    text = """
namespace Demo
  entity A
    one
      nested: vector<vector<#A>>
      keyed: map<string, vector<#>>
"""
    ast = q1_parser.parse_text(text, source="<snippet>")
    a = ast["declarations"][0]["declarations"][0]
    fields = {m["name"]: m["type"] for m in a["blocks"][0]["members"] if m["kind"] == "FieldDecl"}

    nested = fields["nested"]
    assert nested["name"] == "vector"
    inner = nested["params"][0]
    assert inner["name"] == "vector"
    assert inner["params"][0]["target"] == "A"

    keyed = fields["keyed"]
    assert keyed["name"] == "map"
    assert keyed["params"][1]["name"] == "vector"
    assert keyed["params"][1]["params"][0]["target"] is None


def test_parse_nested_struct_in_struct() -> None:
    text = """
namespace rmmr
  namespace asset
    struct Uniform
      using Id as integer
      struct Binding
        id: Id
        type: integer
"""
    ast = q1_parser.parse_text(text, source="<snippet>")
    asset = ast["declarations"][0]["declarations"][0]
    uniform = asset["declarations"][0]
    binding = uniform["members"][1]
    assert binding["kind"] == "StructDecl"
    assert binding["name"] == "Binding"
    assert binding["members"][0]["name"] == "id"


def test_parse_query_operation_all_scope_param() -> None:
    text = """
namespace rmmr
  namespace system
    entity Device
      all
        ?poll_events(~)
"""
    ast = q1_parser.parse_text(text, source="<snippet>")
    device = ast["declarations"][0]["declarations"][0]["declarations"][0]
    op = device["blocks"][0]["members"][0]
    assert op["kind"] == "QueryOp"
    assert op["name"] == "poll_events"
    assert op["params"][0]["type"]["kind"] == "AllScope"
    assert op["params"][0]["type"]["extra_source"] is None


def test_parse_typeof_still_distinct_from_all_scope() -> None:
    text = """
namespace Demo
  using Alias as ~Struct::field1
  struct Struct
    field1: integer
"""
    ast = q1_parser.parse_text(text, source="<snippet>")
    alias = ast["declarations"][0]["declarations"][0]
    assert alias["target"]["kind"] == "MemberTypeOf"


def test_parse_param_name_binding_qualifiers() -> None:
    text = """
namespace Demo
  struct S
    x: float
    y: float
    ?add_to(>target: S)
    =add_from(?source: S)
    >build_from(?source: S) -> S
"""
    ast = q1_parser.parse_text(text, source="<snippet>")
    struct_decl = ast["declarations"][0]["declarations"][0]
    add_to = struct_decl["members"][2]
    add_from = struct_decl["members"][3]
    build_from = struct_decl["members"][4]
    assert add_to["kind"] == "QueryOp"
    assert add_to["name"] == "add_to"
    assert add_to["params"][0]["name"] == "target"
    assert add_to["params"][0]["binding"] == "mut"
    assert add_from["kind"] == "CommandOp"
    assert add_from["params"][0]["name"] == "source"
    assert add_from["params"][0]["binding"] == "read"
    assert build_from["kind"] == "FactoryOp"
    assert build_from["params"][0]["name"] == "source"
    assert build_from["params"][0]["binding"] == "read"


def test_parse_template_operation() -> None:
    text = """
namespace Demo
  component Shelf of Host
    one
      ?find<R as feature of Unit>(Unit::Name)-> #R?
"""
    ast = q1_parser.parse_text(text, source="<snippet>")
    component = ast["declarations"][0]["declarations"][0]
    op = component["blocks"][0]["members"][0]
    assert op["kind"] == "QueryOp"
    assert op["name"] == "find"
    assert op["template_params"] == [
        {"kind": "TemplateParam", "name": "R", "constraint": "feature of Unit"}
    ]
    assert op["params"][0]["type"]["raw"] == "Unit::Name"
    assert op["return_type"]["kind"] == "OptionalType"
    assert op["return_type"]["inner"]["kind"] == "IdType"
    assert op["return_type"]["inner"]["target"] == "R"


def test_golden_elementary_param_binding_ops() -> None:
    ast = q1_parser.parse_file(ELEMENTARY)
    typization = ast["declarations"][0]["declarations"][0]["declarations"][0]
    struct_with_methods = next(
        member for member in typization["declarations"] if member.get("name") == "StructWithMethods"
    )
    add_to = next(
        member for member in struct_with_methods["members"] if member.get("name") == "add_to"
    )
    add_from = next(
        member for member in struct_with_methods["members"] if member.get("name") == "add_from"
    )
    build_from = next(
        member for member in struct_with_methods["members"] if member.get("name") == "build_from"
    )
    assert add_to["params"][0]["binding"] == "mut"
    assert add_from["kind"] == "CommandOp"
    assert add_from["params"][0]["binding"] == "read"
    assert build_from["params"][0]["binding"] == "read"


def test_parse_struct_field_roles_block_and_inline() -> None:
    text = """
namespace Demo
  struct Mineral
    one name: string
    one density: float
    always table: vector<Mineral> = @external(Mayers)
"""
    ast = q1_parser.parse_text(text, source="<snippet>")
    mineral = ast["declarations"][0]["declarations"][0]
    members = {m["name"]: m for m in mineral["members"] if m["kind"] in {"FieldDecl", "ConstField", "TypeAliasDecl"}}
    assert members["name"]["kind"] == "FieldDecl"
    assert members["name"]["role"] == "one"
    assert members["density"]["role"] == "one"
    table = members["table"]
    assert table["kind"] == "ConstField"
    assert table["role"] == "always"
    assert table["value"] is None
    assert table["initializer"]["kind"] == "ExternalType"
    assert table["initializer"]["description"] == "Mayers"


def test_golden_elementary_multistyle_fields() -> None:
    ast = q1_parser.parse_file(ELEMENTARY)
    typization = ast["declarations"][0]["declarations"][0]["declarations"][0]
    multi = next(decl for decl in typization["declarations"] if decl.get("name") == "MultistyleFieldsSyntax")
    by_name = {m["name"]: m for m in multi["members"] if m.get("name")}
    assert by_name["myStaticConst"]["kind"] == "ConstField"
    assert by_name["myStaticConst"]["role"] == "always"
    assert by_name["myStaticConst"]["value"] == 7
    assert by_name["myOtherStaticConst"]["value"] == 8
    assert by_name["field3"]["kind"] == "FieldDecl"
    assert by_name["field3"]["role"] == "one"
    assert by_name["staticMutable1"]["role"] == "all"
    table = by_name["myTable"]
    assert table["kind"] == "ConstField"
    assert table["initializer"]["description"] == "Mayers"


def test_parse_aspect_inline_field_role() -> None:
    text = """
namespace Demo
  entity Rock
    one mass: float
    always max_mass: integer = 9
"""
    ast = q1_parser.parse_text(text, source="<snippet>")
    rock = ast["declarations"][0]["declarations"][0]
    roles = [block["role"] for block in rock["blocks"]]
    assert roles == ["one", "always"]
    assert rock["blocks"][0]["members"][0]["name"] == "mass"
    assert rock["blocks"][1]["members"][0]["kind"] == "ConstField"
    assert rock["blocks"][1]["members"][0]["value"] == 9


def test_parse_struct_inheritance() -> None:
    text = """
namespace Demo
  struct Simple
    fieldA: integer
  struct Complex of Simple
    fieldExtended: string
"""
    ast = q1_parser.parse_text(text, source="<snippet>")
    decls = ast["declarations"][0]["declarations"]
    simple = decls[0]
    complex = decls[1]
    assert simple["kind"] == "StructDecl"
    assert simple["name"] == "Simple"
    assert simple["base"] is None
    assert simple["members"][0]["name"] == "fieldA"
    assert complex["kind"] == "StructDecl"
    assert complex["name"] == "Complex"
    assert complex["base"] == "Simple"
    assert complex["members"][0]["name"] == "fieldExtended"


def test_parse_struct_inheritance_qualified_base() -> None:
    text = """
namespace Demo
  namespace Other
    struct Simple
      fieldA: integer
  struct Complex of Other::Simple
    fieldExtended: string
"""
    ast = q1_parser.parse_text(text, source="<snippet>")
    complex = ast["declarations"][0]["declarations"][1]
    assert complex["name"] == "Complex"
    assert complex["base"] == "Other::Simple"


def test_parse_nested_struct_inheritance() -> None:
    text = """
namespace Demo
  struct Uniform
    struct Binding
      id: integer
    struct Extra of Binding
      extra: integer
"""
    ast = q1_parser.parse_text(text, source="<snippet>")
    uniform = ast["declarations"][0]["declarations"][0]
    binding = uniform["members"][0]
    extra = uniform["members"][1]
    assert binding["name"] == "Binding"
    assert binding["base"] is None
    assert extra["name"] == "Extra"
    assert extra["base"] == "Binding"


def test_parse_struct_inheritance_rejects_malformed_header() -> None:
    text = """
namespace Demo
  struct Complex of
    fieldExtended: string
"""
    try:
        q1_parser.parse_text(text, source="<snippet>")
        raise AssertionError("expected ParseError")
    except q1_parser.ParseError as exc:
        assert "Malformed struct" in str(exc)


def test_golden_elementary_struct_inheritance() -> None:
    ast = q1_parser.parse_file(ELEMENTARY)
    typization = ast["declarations"][0]["declarations"][0]["declarations"][0]
    by_name = {decl["name"]: decl for decl in typization["declarations"] if decl.get("kind") == "StructDecl"}
    assert by_name["Simple"]["base"] is None
    assert by_name["Complex"]["base"] == "Simple"
    assert by_name["Complex"]["members"][0]["name"] == "fieldExtended"


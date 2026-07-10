from __future__ import annotations

import json
import sys
from pathlib import Path

TOOLING_DIR = Path(__file__).resolve().parents[1]
if str(TOOLING_DIR) not in sys.path:
    sys.path.insert(0, str(TOOLING_DIR))

import parser as q1_parser


ROOT = TOOLING_DIR.parents[2]
ASPECTS = ROOT / "modules" / "Q1" / "golden" / "Etalon.q1" / "aspects.q1.types"
ELEMENTARY = ROOT / "modules" / "Q1" / "golden" / "Etalon.q1" / "elementary.q1.types"


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

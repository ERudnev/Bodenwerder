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
  type ExternalDomainType @external(similar to OpenGL texture handle)
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

from __future__ import annotations

import argparse
import json
import re
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Any


class ParseError(Exception):
    def __init__(self, message: str, line: int | None = None) -> None:
        self.message = message
        self.line = line
        super().__init__(self.__str__())

    def __str__(self) -> str:
        if self.line is None:
            return self.message
        return f"Line {self.line}: {self.message}"


@dataclass(frozen=True)
class Line:
    number: int
    indent: int
    content: str
    comment: str | None


def _node(kind: str, line: int, **kwargs: Any) -> dict[str, Any]:
    out: dict[str, Any] = {"kind": kind, "line": line}
    out.update(kwargs)
    return out


def _split_comment(text: str) -> tuple[str, str | None]:
    marker = text.find("//")
    if marker == -1:
        return text.rstrip(), None
    return text[:marker].rstrip(), text[marker:].rstrip()


def _load_lines(text: str) -> list[Line]:
    lines: list[Line] = []
    for number, raw in enumerate(text.splitlines(), start=1):
        indent = len(raw) - len(raw.lstrip(" "))
        stripped = raw[indent:]
        content, comment = _split_comment(stripped)
        if not content.strip():
            continue
        lines.append(Line(number=number, indent=indent, content=content.strip(), comment=comment))
    return lines


class Cursor:
    def __init__(self, lines: list[Line]) -> None:
        self.lines = lines
        self.index = 0

    def peek(self) -> Line | None:
        if self.index >= len(self.lines):
            return None
        return self.lines[self.index]

    def pop(self) -> Line:
        line = self.peek()
        if line is None:
            raise ParseError("Unexpected end of file")
        self.index += 1
        return line


IDENT_RE = re.compile(r"^[A-Za-z_][A-Za-z0-9_]*$")
QUALIFIED_RE = re.compile(r"^[A-Za-z_][A-Za-z0-9_]*(?:::[A-Za-z_][A-Za-z0-9_]*)*$")
IMPORT_RE = re.compile(r'^import\s+"([^"]+)"$')


def _external_type_span(text: str) -> int:
    if not text.startswith("@external("):
        raise ParseError(f"Expected @external(...), got {text!r}")
    depth = 0
    for index, ch in enumerate(text):
        if ch == "(":
            depth += 1
        elif ch == ")":
            depth -= 1
            if depth == 0:
                return index + 1
    raise ParseError(f"Unclosed @external(...): {text!r}")


def _split_type_and_field_directives(text: str) -> tuple[str, list[dict[str, Any]]]:
    stripped = text.strip()
    if stripped.startswith("@external("):
        type_end = _external_type_span(stripped)
        type_text = stripped[:type_end]
        remainder = stripped[type_end:].strip()
        if not remainder:
            return type_text, []
        if not remainder.startswith("@"):
            raise ParseError(f"Unexpected tokens after external type: {remainder!r}")
        _, annotations = _extract_annotations(remainder)
        return type_text, annotations
    return _extract_annotations(stripped)


def _extract_annotations(text: str) -> tuple[str, list[dict[str, Any]]]:
    base = text.rstrip()
    if base.startswith("@external("):
        return base, []
    annotations: list[dict[str, Any]] = []
    while True:
        marker = base.find(" @")
        if marker == -1 and not base.startswith("@"):
            break
        start = 0 if base.startswith("@") else marker + 1
        rest = base[start:]
        if rest.startswith("@external("):
            break
        prefix = base[:start].rstrip()
        annotations = _parse_annotations(rest) + annotations
        base = prefix
    return base.rstrip(), annotations


def _parse_annotations(text: str) -> list[dict[str, Any]]:
    annotations: list[dict[str, Any]] = []
    pos = 0
    while pos < len(text):
        while pos < len(text) and text[pos].isspace():
            pos += 1
        if pos >= len(text):
            break
        if text[pos] != "@":
            raise ParseError(f"Malformed annotation tail: {text!r}")
        pos += 1
        name_start = pos
        while pos < len(text) and (text[pos].isalnum() or text[pos] == "_"):
            pos += 1
        name = text[name_start:pos]
        value = None
        if pos < len(text) and text[pos] == "(":
            depth = 1
            pos += 1
            body_start = pos
            while pos < len(text) and depth > 0:
                if text[pos] == "(":
                    depth += 1
                elif text[pos] == ")":
                    depth -= 1
                pos += 1
            if depth != 0:
                raise ParseError(f"Unclosed annotation body: {text!r}")
            value = text[body_start:pos - 1]
        annotations.append({"name": name, "value": value})
    return annotations


def _split_top_level(text: str, delimiter: str) -> list[str]:
    parts: list[str] = []
    depth_angle = 0
    depth_paren = 0
    current: list[str] = []
    for ch in text:
        if ch == "<":
            depth_angle += 1
        elif ch == ">":
            depth_angle -= 1
        elif ch == "(":
            depth_paren += 1
        elif ch == ")":
            depth_paren -= 1
        if ch == delimiter and depth_angle == 0 and depth_paren == 0:
            parts.append("".join(current).strip())
            current = []
            continue
        current.append(ch)
    parts.append("".join(current).strip())
    return [part for part in parts if part]


def _split_named_param(text: str) -> tuple[str, str] | None:
    depth_angle = 0
    depth_paren = 0
    for index, ch in enumerate(text):
        if ch == "<":
            depth_angle += 1
        elif ch == ">":
            depth_angle -= 1
        elif ch == "(":
            depth_paren += 1
        elif ch == ")":
            depth_paren -= 1
        elif ch == ":" and depth_angle == 0 and depth_paren == 0:
            prev_is_colon = index > 0 and text[index - 1] == ":"
            next_is_colon = index + 1 < len(text) and text[index + 1] == ":"
            if not prev_is_colon and not next_is_colon:
                return text[:index].strip(), text[index + 1:].strip()
    return None


def parse_type_expr(text: str) -> dict[str, Any]:
    raw = text.strip()
    if not raw:
        raise ParseError("Empty type expression")
    if raw.endswith("?"):
        return {"kind": "OptionalType", "raw": raw, "inner": parse_type_expr(raw[:-1])}
    if raw.startswith("@external("):
        end = _external_type_span(raw)
        description = raw[len("@external("):end - 1]
        remainder = raw[end:].strip()
        if remainder:
            raise ParseError(f"Unexpected tokens after external type: {remainder!r}")
        return {"kind": "ExternalType", "raw": raw[:end], "description": description}
    if raw.startswith("anchor<") and raw.endswith(">"):
        return {"kind": "AnchorType", "raw": raw, "target": parse_type_expr(raw[7:-1])}
    if raw.startswith("control<") and raw.endswith(">"):
        return {"kind": "ControlType", "raw": raw, "target": parse_type_expr(raw[8:-1])}
    if raw == "#":
        return {"kind": "IdType", "raw": raw, "target": None}
    if raw.startswith("#"):
        return {"kind": "IdType", "raw": raw, "target": raw[1:]}
    if raw.startswith("~") and "::" in raw[1:]:
        target, member = raw[1:].rsplit("::", 1)
        return {
            "kind": "MemberTypeOf",
            "raw": raw,
            "target": [part.strip() for part in target.split("::")],
            "member": member.strip(),
        }
    builtin = {"integer", "float", "string", "index2", "boolean", "vec2", "vec3", "vec4", "quat", "mat4"}
    if raw in builtin:
        return {"kind": "BuiltinType", "raw": raw, "name": raw}
    if not QUALIFIED_RE.match(raw):
        raise ParseError(f"Unsupported type expression: {raw!r}")
    return {"kind": "NamedType", "raw": raw, "parts": raw.split("::")}


def _parse_params(text: str) -> list[dict[str, Any]]:
    if not text.strip():
        return []
    params: list[dict[str, Any]] = []
    for item in _split_top_level(text, ","):
        named = _split_named_param(item)
        if named is not None:
            name, type_text = named
            params.append({"kind": "NamedParam", "name": name, "type": parse_type_expr(type_text)})
        else:
            params.append({"kind": "AnonymousParam", "type": parse_type_expr(item.strip())})
    return params


def _parse_operation(line: Line) -> dict[str, Any]:
    prefix = line.content[0]
    rest = line.content[1:].strip()
    name_end = rest.find("(")
    if name_end == -1 or not rest.endswith(")") and "->" not in rest:
        raise ParseError("Malformed operation", line.number)
    name = rest[:name_end].strip()
    if not IDENT_RE.match(name):
        raise ParseError(f"Invalid operation name: {name!r}", line.number)
    depth = 0
    close_index = None
    for index, ch in enumerate(rest[name_end:], start=name_end):
        if ch == "(":
            depth += 1
        elif ch == ")":
            depth -= 1
            if depth == 0:
                close_index = index
                break
    if close_index is None:
        raise ParseError("Unclosed operation parameter list", line.number)
    params_text = rest[name_end + 1:close_index]
    tail = rest[close_index + 1:].strip()
    return_type = None
    if tail:
        if not tail.startswith("->"):
            raise ParseError("Unexpected tokens after operation parameter list", line.number)
        return_type = parse_type_expr(tail[2:].strip())
    kind_map = {"?": "QueryOp", "=": "CommandOp", ">": "FactoryOp"}
    return _node(
        kind_map[prefix],
        line.number,
        name=name,
        params=_parse_params(params_text),
        return_type=return_type,
        comment=line.comment,
    )


def _parse_reaction_scope(text: str, line: int) -> dict[str, Any]:
    if text in {"-one", "=one", ">one"}:
        meaning = {"-one": "delete", "=one": "change", ">one": "world_change"}[text]
        return {"kind": "OneScope", "raw": text, "event": meaning}
    if text == "~":
        return {"kind": "AllScope", "raw": text, "extra_source": None, "implicit_owner": True}
    if text.startswith("~"):
        extra = text[1:].strip()
        if not IDENT_RE.match(extra):
            raise ParseError(f"Invalid all-reaction scope: {text!r}", line)
        return {"kind": "AllScope", "raw": text, "extra_source": extra, "implicit_owner": True}
    raise ParseError(f"Unsupported reaction scope: {text!r}", line)


def _parse_reaction(line: Line) -> dict[str, Any]:
    text = line.content[1:].strip()
    name_end = text.find("(")
    if name_end == -1 or not text.endswith(")"):
        raise ParseError("Malformed reaction", line.number)
    name = text[:name_end].strip()
    if not IDENT_RE.match(name):
        raise ParseError(f"Invalid reaction name: {name!r}", line.number)
    scope_text = text[name_end + 1:-1].strip()
    return _node(
        "ReactionDecl",
        line.number,
        name=name,
        scope=_parse_reaction_scope(scope_text, line.number),
        comment=line.comment,
    )


def _parse_field(line: Line, allow_const: bool) -> dict[str, Any]:
    name, rest = line.content.split(":", 1)
    field_name = name.strip()
    base, annotations = _split_type_and_field_directives(rest.strip())
    if allow_const and "=" in base:
        type_text, literal_text = base.split("=", 1)
        return _node(
            "ConstField",
            line.number,
            name=field_name,
            type=parse_type_expr(type_text.strip()),
            value=int(literal_text.strip()),
            annotations=annotations,
            comment=line.comment,
        )
    return _node(
        "FieldDecl",
        line.number,
        name=field_name,
        type=parse_type_expr(base.strip()),
        annotations=annotations,
        comment=line.comment,
    )


def _parse_type_alias(line: Line) -> dict[str, Any]:
    _, rest = line.content.split("type", 1)
    rest = rest.strip()
    if " " not in rest:
        raise ParseError("Malformed type declaration", line.number)
    name, rhs = rest.split(" ", 1)
    rhs = rhs.strip()
    if not IDENT_RE.match(name):
        raise ParseError(f"Invalid type name: {name!r}", line.number)
    if rhs.startswith("@external("):
        return _node(
            "TypeAliasDecl",
            line.number,
            name=name,
            mode="external",
            target=parse_type_expr(rhs),
            annotations=[],
            comment=line.comment,
        )
    base, annotations = _extract_annotations(rhs)
    if base.startswith("as "):
        return _node(
            "TypeAliasDecl",
            line.number,
            name=name,
            mode="alias",
            target=parse_type_expr(base[3:].strip()),
            annotations=annotations,
            comment=line.comment,
        )
    if base.startswith("~"):
        return _node(
            "TypeAliasDecl",
            line.number,
            name=name,
            mode="typeof",
            target=parse_type_expr(base),
            annotations=annotations,
            comment=line.comment,
        )
    raise ParseError("Unsupported type declaration form", line.number)


def _child_indent(cursor: Cursor, parent_indent: int) -> int | None:
    line = cursor.peek()
    if line is None or line.indent <= parent_indent:
        return None
    return line.indent


def _parse_generic_members(cursor: Cursor, indent: int, allow_const: bool) -> list[dict[str, Any]]:
    members: list[dict[str, Any]] = []
    while True:
        line = cursor.peek()
        if line is None or line.indent < indent:
            break
        if line.indent > indent:
            raise ParseError("Unexpected indentation", line.number)
        line = cursor.pop()
        if line.content.startswith("type "):
            members.append(_parse_type_alias(line))
        elif line.content[:1] in "?=>":
            members.append(_parse_operation(line))
        elif line.content.startswith("!"):
            members.append(_parse_reaction(line))
        elif ":" in line.content:
            members.append(_parse_field(line, allow_const=allow_const))
        else:
            raise ParseError(f"Unsupported member syntax: {line.content!r}", line.number)
    return members


def _parse_import(line: Line) -> dict[str, Any]:
    match = IMPORT_RE.fullmatch(line.content)
    if not match:
        raise ParseError("Malformed import; expected import \"logical/path\"", line.number)
    return _node("ImportDecl", line.number, path=match.group(1), comment=line.comment)


def _parse_aspect_local_types(cursor: Cursor, parent_indent: int) -> list[dict[str, Any]]:
    indent = _child_indent(cursor, parent_indent)
    if indent is None:
        return []
    local_types: list[dict[str, Any]] = []
    while True:
        line = cursor.peek()
        if line is None or line.indent < indent:
            break
        if line.indent > indent:
            raise ParseError("Unexpected indentation in aspect body", line.number)
        if not line.content.startswith("type "):
            break
        local_types.append(_parse_type_alias(cursor.pop()))
    return local_types


def _parse_aspect_blocks(cursor: Cursor, parent_indent: int) -> list[dict[str, Any]]:
    indent = _child_indent(cursor, parent_indent)
    if indent is None:
        return []
    blocks: list[dict[str, Any]] = []
    while True:
        line = cursor.peek()
        if line is None or line.indent < indent:
            break
        if line.indent > indent:
            raise ParseError("Unexpected indentation in aspect body", line.number)
        line = cursor.pop()
        role = line.content
        if role not in {"always", "one", "all"}:
            raise ParseError(f"Expected aspect block, got {role!r}", line.number)
        member_indent = _child_indent(cursor, line.indent)
        members = [] if member_indent is None else _parse_generic_members(
            cursor, member_indent, allow_const=(role == "always")
        )
        blocks.append(_node("AspectBlock", line.number, role=role, members=members, comment=line.comment))
    return blocks


def _parse_archetype_members(cursor: Cursor, parent_indent: int) -> list[dict[str, Any]]:
    indent = _child_indent(cursor, parent_indent)
    if indent is None:
        return []
    return _parse_generic_members(cursor, indent, allow_const=False)


def _parse_struct(cursor: Cursor, line: Line) -> dict[str, Any]:
    _, name = line.content.split("struct", 1)
    name = name.strip()
    if not IDENT_RE.match(name):
        raise ParseError(f"Invalid struct name: {name!r}", line.number)
    indent = _child_indent(cursor, line.indent)
    members = [] if indent is None else _parse_generic_members(cursor, indent, allow_const=False)
    return _node("StructDecl", line.number, name=name, members=members, comment=line.comment)


def _parse_aspect(cursor: Cursor, line: Line, category: str) -> dict[str, Any]:
    text = line.content
    if category == "entity":
        match = re.fullmatch(r"entity\s+([A-Za-z_][A-Za-z0-9_]*)(?:\s+one\s+(.+))?", text)
        if not match:
            raise ParseError("Malformed entity declaration", line.number)
        name, inline_one = match.groups()
        blocks: list[dict[str, Any]]
        local_types: list[dict[str, Any]]
        if inline_one:
            local_types = []
            fake_line = Line(line.number, line.indent, inline_one.strip(), line.comment)
            blocks = [_node("AspectBlock", line.number, role="one", members=[_parse_field(fake_line, allow_const=False)])]
        else:
            local_types = _parse_aspect_local_types(cursor, line.indent)
            blocks = _parse_aspect_blocks(cursor, line.indent)
        return _node(
            "AspectDecl",
            line.number,
            category=category,
            name=name,
            owner=None,
            element=None,
            local_types=local_types,
            blocks=blocks,
            comment=line.comment,
        )
    if category in {"attribute", "component"}:
        match = re.fullmatch(rf"{category}\s+([A-Za-z_][A-Za-z0-9_]*)\s+of\s+([A-Za-z_][A-Za-z0-9_]*)", text)
        if not match:
            raise ParseError(f"Malformed {category} declaration", line.number)
        name, owner = match.groups()
        return _node(
            "AspectDecl",
            line.number,
            category=category,
            name=name,
            owner=owner,
            element=None,
            local_types=_parse_aspect_local_types(cursor, line.indent),
            blocks=_parse_aspect_blocks(cursor, line.indent),
            comment=line.comment,
        )
    if category == "group":
        match = re.fullmatch(r"group<([A-Za-z_][A-Za-z0-9_]*)>\s+of\s+([A-Za-z_][A-Za-z0-9_]*)", text)
        if not match:
            raise ParseError("Malformed group declaration", line.number)
        element, owner = match.groups()
        return _node(
            "AspectDecl",
            line.number,
            category=category,
            name=f"{element}_group",
            owner=owner,
            element=element,
            local_types=_parse_aspect_local_types(cursor, line.indent),
            blocks=_parse_aspect_blocks(cursor, line.indent),
            comment=line.comment,
        )
    if category == "archetype":
        match = re.fullmatch(r"archetype\s+([A-Za-z_][A-Za-z0-9_]*)", text)
        if not match:
            raise ParseError("Malformed archetype declaration", line.number)
        name = match.group(1)
        return _node(
            "AspectDecl",
            line.number,
            category=category,
            name=name,
            owner=None,
            element=None,
            members=_parse_archetype_members(cursor, line.indent),
            comment=line.comment,
        )
    raise ParseError(f"Unsupported aspect category: {category!r}", line.number)


def _parse_namespace(cursor: Cursor, line: Line) -> dict[str, Any]:
    _, name = line.content.split("namespace", 1)
    name = name.strip()
    if not IDENT_RE.match(name):
        raise ParseError(f"Invalid namespace name: {name!r}", line.number)
    indent = _child_indent(cursor, line.indent)
    declarations = [] if indent is None else _parse_declarations(cursor, indent)
    return _node("NamespaceDecl", line.number, name=name, declarations=declarations, comment=line.comment)


def _parse_declaration(cursor: Cursor, indent: int) -> dict[str, Any]:
    line = cursor.pop()
    text = line.content
    if text.startswith("import "):
        if indent != 0:
            raise ParseError("Import must appear at file top level", line.number)
        return _parse_import(line)
    if text.startswith("namespace "):
        return _parse_namespace(cursor, line)
    if text.startswith("type "):
        return _parse_type_alias(line)
    if text.startswith("struct "):
        return _parse_struct(cursor, line)
    if text.startswith("entity "):
        return _parse_aspect(cursor, line, "entity")
    if text.startswith("attribute "):
        return _parse_aspect(cursor, line, "attribute")
    if text.startswith("component "):
        return _parse_aspect(cursor, line, "component")
    if text.startswith("group<"):
        return _parse_aspect(cursor, line, "group")
    if text.startswith("archetype "):
        return _parse_aspect(cursor, line, "archetype")
    raise ParseError(f"Unsupported declaration: {text!r}", line.number)


def _parse_declarations(cursor: Cursor, indent: int) -> list[dict[str, Any]]:
    declarations: list[dict[str, Any]] = []
    while True:
        line = cursor.peek()
        if line is None or line.indent < indent:
            break
        if line.indent > indent:
            raise ParseError("Unexpected indentation", line.number)
        declarations.append(_parse_declaration(cursor, indent))
    return declarations


def parse_text(text: str, source: str = "<memory>") -> dict[str, Any]:
    lines = _load_lines(text)
    cursor = Cursor(lines)
    declarations = _parse_declarations(cursor, 0)
    if cursor.peek() is not None:
        raise ParseError("Trailing unparsed input", cursor.peek().number)
    return _node("File", 1, source=source, declarations=declarations)


def parse_file(path: str | Path) -> dict[str, Any]:
    file_path = Path(path)
    return parse_text(file_path.read_text(encoding="utf-8"), source=str(file_path))


def _json_default(value: Any) -> Any:
    raise TypeError(f"Object of type {type(value).__name__} is not JSON serializable")


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="Parse a narrow subset of Q1 and optionally dump JSON.")
    parser.add_argument("path", help="Path to a .q1.types file")
    parser.add_argument("--dump-json", action="store_true", help="Print parsed AST as JSON")
    args = parser.parse_args(argv)

    try:
        ast = parse_file(args.path)
    except ParseError as exc:
        print(str(exc), file=sys.stderr)
        return 1

    if args.dump_json:
        json.dump(ast, sys.stdout, indent=2, sort_keys=True, ensure_ascii=False, default=_json_default)
        print()
    else:
        print(f"Parsed {args.path}: {len(ast['declarations'])} top-level declarations")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

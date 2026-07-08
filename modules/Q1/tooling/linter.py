from __future__ import annotations

import argparse
import json
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Any

if __package__ in {None, ""}:
    sys.path.insert(0, str(Path(__file__).resolve().parent))

from parser import ParseError, parse_file, parse_text


BUILTINS = {"integer", "float", "string", "index2", "boolean", "vec2", "vec3", "vec4", "quat", "mat4"}


@dataclass
class Diagnostic:
    severity: str
    line: int
    code: str
    message: str

    def to_dict(self) -> dict[str, Any]:
        return {
            "severity": self.severity,
            "line": self.line,
            "code": self.code,
            "message": self.message,
        }


def warn(diags: list[Diagnostic], line: int, code: str, message: str) -> None:
    diags.append(Diagnostic("warning", line, code, message))


@dataclass
class Symbol:
    name: str
    qualified: tuple[str, ...]
    namespace: tuple[str, ...]
    node: dict[str, Any]


def collect_symbols(ast: dict[str, Any]) -> tuple[dict[tuple[str, ...], Symbol], list[Diagnostic]]:
    symbols: dict[tuple[str, ...], Symbol] = {}
    diags: list[Diagnostic] = []

    def visit_decls(decls: list[dict[str, Any]], namespace: tuple[str, ...]) -> None:
        local: dict[str, dict[str, Any]] = {}
        for decl in decls:
            if decl["kind"] == "ImportDecl":
                continue
            if decl["kind"] == "NamespaceDecl":
                visit_decls(decl["declarations"], namespace + (decl["name"],))
                continue
            name = decl["name"]
            if name in local:
                warn(diags, decl["line"], "duplicate-name", f"Duplicate declaration name in namespace: {name}")
            local[name] = decl
            qualified = namespace + (name,)
            if qualified in symbols:
                warn(diags, decl["line"], "duplicate-qualified-name", f"Duplicate qualified name: {'::'.join(qualified)}")
            symbols[qualified] = Symbol(name=name, qualified=qualified, namespace=namespace, node=decl)

    visit_decls(ast["declarations"], ())
    return symbols, diags


def _import_targets(ast: dict[str, Any]) -> list[str]:
    return [decl["path"] for decl in ast["declarations"] if decl["kind"] == "ImportDecl"]


def _resolve_import_path(source_file: Path, logical_path: str) -> Path:
    return source_file.parent / f"{logical_path}.q1.types"


def collect_import_symbols(
    ast: dict[str, Any],
    source_file: Path,
    seen: set[Path] | None = None,
) -> tuple[dict[tuple[str, ...], Symbol], list[Diagnostic]]:
    if seen is None:
        seen = set()
    source_file = source_file.resolve()
    if source_file in seen:
        return {}, []
    seen.add(source_file)

    symbols: dict[tuple[str, ...], Symbol] = {}
    diags: list[Diagnostic] = []

    for logical_path in _import_targets(ast):
        imported_path = _resolve_import_path(source_file, logical_path)
        if not imported_path.is_file():
            import_line = next(
                decl["line"] for decl in ast["declarations"] if decl["kind"] == "ImportDecl" and decl["path"] == logical_path
            )
            warn(diags, import_line, "missing-import", f"Imported module not found: {imported_path}")
            continue
        try:
            imported_ast = parse_file(imported_path)
        except ParseError as exc:
            import_line = next(
                decl["line"] for decl in ast["declarations"] if decl["kind"] == "ImportDecl" and decl["path"] == logical_path
            )
            warn(diags, import_line, "import-parse-error", f"Failed to parse import {logical_path!r}: {exc}")
            continue

        imported_symbols, imported_diags = collect_symbols(imported_ast)
        diags.extend(imported_diags)
        symbols.update(imported_symbols)

        transitive_symbols, transitive_diags = collect_import_symbols(imported_ast, imported_path, seen)
        diags.extend(transitive_diags)
        symbols.update(transitive_symbols)

    return symbols, diags


def resolve_name(parts: list[str], namespace: tuple[str, ...], symbols: dict[tuple[str, ...], Symbol]) -> Symbol | None:
    if not parts:
        return None
    candidates: list[tuple[str, ...]] = []
    if len(parts) == 1:
        for cut in range(len(namespace), -1, -1):
            candidates.append(namespace[:cut] + tuple(parts))
    else:
        for cut in range(len(namespace), -1, -1):
            candidates.append(namespace[:cut] + tuple(parts))
    for candidate in candidates:
        if candidate in symbols:
            return symbols[candidate]
    return None


def decl_fields(node: dict[str, Any]) -> set[str]:
    fields: set[str] = set()
    if node["kind"] == "StructDecl":
        for member in node["members"]:
            if member["kind"] in {"FieldDecl", "ConstField"}:
                fields.add(member["name"])
    elif node["kind"] == "AspectDecl":
        for block in node.get("blocks", []):
            for member in block["members"]:
                if member["kind"] in {"FieldDecl", "ConstField"}:
                    fields.add(member["name"])
    return fields


def lint_type_expr(
    expr: dict[str, Any],
    namespace: tuple[str, ...],
    symbols: dict[tuple[str, ...], Symbol],
    diags: list[Diagnostic],
    line: int,
    entity_local_types: dict[str, dict[str, Any]] | None = None,
) -> None:
    kind = expr["kind"]
    if kind == "BuiltinType":
        return
    if kind == "ExternalType":
        return
    if kind == "OptionalType":
        lint_type_expr(expr["inner"], namespace, symbols, diags, line, entity_local_types)
        return
    if kind in {"AnchorType", "ControlType", "QuantumTypeOf"}:
        lint_type_expr(expr["target"], namespace, symbols, diags, line, entity_local_types)
        if kind == "QuantumTypeOf" and expr["target"]["kind"] == "NamedType":
            if not resolve_name(expr["target"]["parts"], namespace, symbols):
                warn(diags, line, "unknown-quantum-type", f"Unknown aspect in one<...>: {expr['target']['raw']}")
        return
    if kind == "IdType":
        target = expr.get("target")
        if target and not resolve_name([target], namespace, symbols):
            warn(diags, line, "unknown-id-target", f"Unknown id target: {target}")
        return
    if kind == "NamedType":
        raw = expr["raw"]
        if raw in BUILTINS:
            return
        if entity_local_types and raw in entity_local_types:
            if entity_local_types[raw]["target"] is not None:
                lint_type_expr(entity_local_types[raw]["target"], namespace, symbols, diags, line, entity_local_types)
            return
        if not resolve_name(expr["parts"], namespace, symbols):
            warn(diags, line, "unknown-type", f"Unknown type reference: {raw}")
        return
    if kind == "MemberTypeOf":
        symbol = resolve_name(expr["target"], namespace, symbols)
        target_name = "::".join(expr["target"])
        if not symbol:
            warn(diags, line, "unknown-typeof-target", f"Unknown type-of target: {target_name}")
            return
        if expr["member"] not in decl_fields(symbol.node):
            warn(diags, line, "unknown-typeof-member", f"Unknown member {expr['member']} on {target_name}")
        return


def lint_members(
    members: list[dict[str, Any]],
    namespace: tuple[str, ...],
    symbols: dict[tuple[str, ...], Symbol],
    diags: list[Diagnostic],
    context: str,
    block_role: str | None = None,
    entity_local_types: dict[str, dict[str, Any]] | None = None,
) -> None:
    seen: dict[str, dict[str, Any]] = {}
    for member in members:
        if member["kind"] in {"FieldDecl", "ConstField", "TypeAliasDecl", "QueryOp", "CommandOp", "FactoryOp", "ReactionDecl"}:
            name = member.get("name")
            if name:
                key = f"{member['kind']}::{name}"
                if key in seen:
                    warn(diags, member["line"], "duplicate-member", f"Duplicate member in {context}: {name}")
                seen[key] = member

        if member["kind"] == "FieldDecl":
            lint_type_expr(member["type"], namespace, symbols, diags, member["line"], entity_local_types)
        elif member["kind"] == "ConstField":
            lint_type_expr(member["type"], namespace, symbols, diags, member["line"], entity_local_types)
        elif member["kind"] == "TypeAliasDecl":
            if member["target"] is not None:
                lint_type_expr(member["target"], namespace, symbols, diags, member["line"], entity_local_types)
        elif member["kind"] in {"QueryOp", "CommandOp", "FactoryOp"}:
            for param in member["params"]:
                lint_type_expr(param["type"], namespace, symbols, diags, member["line"], entity_local_types)
            if member["return_type"] is not None:
                lint_type_expr(member["return_type"], namespace, symbols, diags, member["line"], entity_local_types)
        elif member["kind"] == "ReactionDecl":
            scope = member["scope"]
            if block_role == "one" and scope["kind"] != "OneScope":
                warn(diags, member["line"], "reaction-scope-mismatch", "Reaction in `one` block should use one-scope syntax")
            if block_role == "all" and scope["kind"] != "AllScope":
                warn(diags, member["line"], "reaction-scope-mismatch", "Reaction in `all` block should use all-scope syntax")
            if block_role == "always":
                warn(diags, member["line"], "reaction-in-always", "Reactions are not expected inside `always` blocks")
            if scope["kind"] == "AllScope" and scope.get("extra_source"):
                if not resolve_name([scope["extra_source"]], namespace, symbols):
                    warn(diags, member["line"], "unknown-reaction-source", f"Unknown all-reaction source: {scope['extra_source']}")


def lint_ast(ast: dict[str, Any], source_file: Path | None = None) -> list[Diagnostic]:
    symbols, diags = collect_symbols(ast)
    if source_file is not None:
        imported_symbols, import_diags = collect_import_symbols(ast, source_file)
        diags.extend(import_diags)
        symbols.update(imported_symbols)

    def visit_decls(decls: list[dict[str, Any]], namespace: tuple[str, ...]) -> None:
        for decl in decls:
            kind = decl["kind"]
            if kind == "ImportDecl":
                continue
            if kind == "NamespaceDecl":
                visit_decls(decl["declarations"], namespace + (decl["name"],))
                continue

            if kind == "StructDecl":
                lint_members(decl["members"], namespace, symbols, diags, f"struct {decl['name']}")
                for member in decl["members"]:
                    if member["kind"] == "ReactionDecl":
                        warn(diags, member["line"], "reaction-in-struct", "Struct body should not contain `!` reactions")
                continue

            if kind == "TypeAliasDecl":
                if decl["target"] is not None:
                    lint_type_expr(decl["target"], namespace, symbols, diags, decl["line"])
                continue

            if kind == "AspectDecl":
                owner = decl.get("owner")
                if owner and not resolve_name([owner], namespace, symbols):
                    warn(diags, decl["line"], "unknown-owner", f"Unknown owner aspect: {owner}")
                element = decl.get("element")
                if element and not resolve_name([element], namespace, symbols):
                    warn(diags, decl["line"], "unknown-group-element", f"Unknown group element: {element}")

                entity_local_types = {local_type["name"]: local_type for local_type in decl.get("local_types", [])}
                for local_type in entity_local_types.values():
                    if local_type["target"] is not None:
                        lint_type_expr(local_type["target"], namespace, symbols, diags, local_type["line"], entity_local_types)

                if decl["category"] == "archetype":
                    members = decl.get("members", [])
                    lint_members(members, namespace, symbols, diags, f"archetype {decl['name']}")
                    for member in members:
                        if member["kind"] == "CommandOp":
                            warn(diags, member["line"], "command-in-archetype", "Current golden archetype subset does not use `=` operations")
                        if member["kind"] == "ReactionDecl":
                            warn(diags, member["line"], "reaction-in-archetype", "Current golden archetype subset does not use reactions")
                        if member["kind"] in {"FieldDecl", "ConstField", "TypeAliasDecl"}:
                            warn(diags, member["line"], "data-in-archetype", "Current golden archetype subset is operation-only")
                    continue

                block_roles: set[str] = set()
                for block in decl.get("blocks", []):
                    role = block["role"]
                    if role in block_roles:
                        warn(diags, block["line"], "duplicate-block-role", f"Duplicate aspect block role: {role}")
                    block_roles.add(role)
                    lint_members(
                        block["members"],
                        namespace,
                        symbols,
                        diags,
                        f"{decl['category']} {decl['name']} block {role}",
                        block_role=role,
                        entity_local_types=entity_local_types,
                    )
                    if role == "always":
                        for member in block["members"]:
                            if member["kind"] not in {"ConstField", "QueryOp"}:
                                warn(diags, member["line"], "unexpected-member-in-always", f"{member['kind']} is not part of the current `always` subset")
                    if role == "one":
                        for member in block["members"]:
                            if member["kind"] not in {"FieldDecl", "QueryOp", "CommandOp", "ReactionDecl"}:
                                warn(diags, member["line"], "unexpected-member-in-one", f"{member['kind']} is not part of the current `one` subset")
                    if role == "all":
                        for member in block["members"]:
                            if member["kind"] not in {"FieldDecl", "QueryOp", "CommandOp", "FactoryOp", "ReactionDecl"}:
                                warn(diags, member["line"], "unexpected-member-in-all", f"{member['kind']} is not part of the current `all` subset")

    visit_decls(ast["declarations"], ())
    return diags


def lint_file(path: str | Path) -> tuple[dict[str, Any] | None, list[Diagnostic], ParseError | None]:
    try:
        ast = parse_file(path)
    except ParseError as exc:
        return None, [], exc
    return ast, lint_ast(ast, Path(path)), None


def lint_text(text: str, source: str = "<memory>") -> tuple[dict[str, Any] | None, list[Diagnostic], ParseError | None]:
    try:
        ast = parse_text(text, source=source)
    except ParseError as exc:
        return None, [], exc
    source_file = Path(source) if source != "<memory>" and Path(source).is_file() else None
    return ast, lint_ast(ast, source_file), None


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description="Conservative linter for the current golden subset of Q1.")
    ap.add_argument("path", help="Path to a .q1.types file")
    ap.add_argument("--dump-json", action="store_true", help="Dump diagnostics as JSON")
    args = ap.parse_args(argv)

    ast, diagnostics, error = lint_file(args.path)
    if error is not None:
        if args.dump_json:
            json.dump({"parse_error": str(error), "diagnostics": []}, sys.stdout, indent=2, sort_keys=True, ensure_ascii=False)
            print()
        else:
            print(str(error), file=sys.stderr)
        return 1

    if args.dump_json:
        json.dump(
            {
                "source": str(Path(args.path)),
                "diagnostics": [diag.to_dict() for diag in diagnostics],
            },
            sys.stdout,
            indent=2,
            sort_keys=True,
            ensure_ascii=False,
        )
        print()
    else:
        for diag in diagnostics:
            print(f"{diag.severity.upper()}:{diag.line}:{diag.code}: {diag.message}")
        print(f"Linted {args.path}: {len(diagnostics)} diagnostics")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

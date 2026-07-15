from __future__ import annotations

import sys
from pathlib import Path

TOOLING_DIR = Path(__file__).resolve().parents[1]
if str(TOOLING_DIR) not in sys.path:
    sys.path.insert(0, str(TOOLING_DIR))

import linter as q1_linter


ROOT = TOOLING_DIR.parents[2]
ASPECTS = ROOT / "modules" / "Q1" / "golden" / "Etalon.q1" / "aspects.q1.types"


def test_linter_runs_on_golden_aspects_without_crash() -> None:
    ast, diagnostics, error = q1_linter.lint_file(ASPECTS)
    assert error is None
    assert ast is not None
    assert isinstance(diagnostics, list)


def test_struct_reaction_is_warning_not_crash() -> None:
    text = """
namespace Demo
  struct Bad
    field: integer
    !oops(=one)
"""
    ast, diagnostics, error = q1_linter.lint_text(text, source="<snippet>")
    assert error is None
    assert ast is not None
    codes = {diag.code for diag in diagnostics}
    assert "reaction-in-struct" in codes


def test_feature_unknown_owner_is_warning() -> None:
    text = """
namespace Demo
  feature Attr of Missing
    one
      x: integer
"""
    ast, diagnostics, error = q1_linter.lint_text(text, source="<snippet>")
    assert error is None
    assert ast is not None
    codes = {diag.code for diag in diagnostics}
    assert "unknown-owner" in codes


def test_unknown_owner_is_warning() -> None:
    text = """
namespace Demo
  attribute Attr of Missing
    one
      x: integer
"""
    ast, diagnostics, error = q1_linter.lint_text(text, source="<snippet>")
    assert error is None
    assert ast is not None
    codes = {diag.code for diag in diagnostics}
    assert "unknown-owner" in codes


def test_lint_container_id_fields_clean() -> None:
    text = """
namespace Demo
  entity A

  entity B
    one
      brother: #
      parents: vector<#A>
      siblings: vector<#>
"""
    ast, diagnostics, error = q1_linter.lint_text(text, source="<snippet>")
    assert error is None
    assert ast is not None
    assert diagnostics == []


def test_lint_nested_struct_member_type() -> None:
    text = """
namespace rmmr
  namespace asset
    struct Uniform
      using Id as integer
      struct Binding
        id: Id

  namespace resource
    entity Material
      one
        bindings: vector<Uniform::Binding>
"""
    ast, diagnostics, error = q1_linter.lint_text(text, source="<snippet>")
    assert error is None
    assert ast is not None
    assert diagnostics == []


def test_lint_query_all_scope_param_clean() -> None:
    text = """
namespace rmmr
  namespace system
    entity Device
      all
        ?poll_events(~)
"""
    ast, diagnostics, error = q1_linter.lint_text(text, source="<snippet>")
    assert error is None
    assert ast is not None
    assert diagnostics == []


def test_lint_query_all_scope_param_warns_outside_all_block() -> None:
    text = """
namespace Demo
  entity Device
    one
      ?poll_events(~)
"""
    ast, diagnostics, error = q1_linter.lint_text(text, source="<snippet>")
    assert error is None
    assert ast is not None
    codes = {diag.code for diag in diagnostics}
    assert "all-scope-outside-all-block" in codes


def test_lint_container_unknown_id_target_warns() -> None:
    text = """
namespace Demo
  entity B
    one
      refs: vector<#Missing>
"""
    ast, diagnostics, error = q1_linter.lint_text(text, source="<snippet>")
    assert error is None
    assert ast is not None
    codes = {diag.code for diag in diagnostics}
    assert "unknown-id-target" in codes


def test_lint_all_builtin_containers_accept_type_params() -> None:
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
    ast, diagnostics, error = q1_linter.lint_text(text, source="<snippet>")
    assert error is None
    assert ast is not None
    assert diagnostics == []


def test_lint_filepath_and_filename_builtin_types() -> None:
    text = """
namespace Demo
  entity Asset
    one
      root: filepath
      name: filename
"""
    ast, diagnostics, error = q1_linter.lint_text(text, source="<snippet>")
    assert error is None
    assert ast is not None
    assert diagnostics == []

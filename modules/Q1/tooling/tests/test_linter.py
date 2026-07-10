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

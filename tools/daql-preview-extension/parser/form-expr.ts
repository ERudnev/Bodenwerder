import type { FormArg, FormExpr } from "../model/document";

const IDENT_START = /[A-Za-z_]/;
const IDENT_PART = /[A-Za-z0-9_]/;

function readIdent(text: string, pos: number): { name: string; end: number } | null {
  if (pos >= text.length || !IDENT_START.test(text[pos])) {
    return null;
  }
  let end = pos + 1;
  while (end < text.length && IDENT_PART.test(text[end])) {
    end++;
  }
  return { name: text.slice(pos, end), end };
}

function skipWhitespace(text: string, pos: number): number {
  while (pos < text.length && /\s/.test(text[pos])) {
    pos++;
  }
  return pos;
}

function parseFormExprInner(text: string, pos: number): { expr: FormExpr; end: number } | null {
  pos = skipWhitespace(text, pos);
  if (text[pos] !== "@") {
    return null;
  }
  pos++;

  const ident = readIdent(text, pos);
  if (!ident) {
    return null;
  }

  const expr: FormExpr = { name: ident.name };
  pos = ident.end;

  pos = skipWhitespace(text, pos);
  if (pos < text.length && text[pos] === "(") {
    const argsResult = parseFormArgs(text, pos);
    if (argsResult) {
      expr.args = argsResult.args;
      pos = argsResult.end;
    }
  }

  return { expr, end: pos };
}

function parseFormArgs(text: string, pos: number): { args: FormArg[]; end: number } | null {
  if (text[pos] !== "(") {
    return null;
  }
  pos++;
  const args: FormArg[] = [];

  while (pos < text.length) {
    pos = skipWhitespace(text, pos);
    if (text[pos] === ")") {
      return { args, end: pos + 1 };
    }

    if (args.length > 0) {
      if (text[pos] === ",") {
        pos++;
        pos = skipWhitespace(text, pos);
      }
    }

    if (text[pos] === "~") {
      let end = pos + 1;
      while (end < text.length && /[0-9.]/.test(text[end])) {
        end++;
      }
      args.push({ kind: "quality", value: text.slice(pos, end) });
      pos = end;
      continue;
    }

    if (text.slice(pos, pos + 3) === "inf") {
      args.push({ kind: "quality", value: "inf" });
      pos += 3;
      continue;
    }

    const nested = parseFormExprInner(text, pos);
    if (nested) {
      args.push({ kind: "form", expr: nested.expr });
      pos = nested.end;
      continue;
    }

    break;
  }

  return null;
}

export function parseFormExpr(text: string): FormExpr | null {
  const trimmed = text.trim();
  if (!trimmed.startsWith("@")) {
    return null;
  }
  const result = parseFormExprInner(trimmed, 0);
  if (!result) {
    return null;
  }
  const rest = trimmed.slice(result.end).trim();
  if (rest.length > 0 && rest !== "{") {
    return null;
  }
  return result.expr;
}

export function formatFormExpr(expr: FormExpr): string {
  let s = `@${expr.name}`;
  if (expr.args && expr.args.length > 0) {
    const parts = expr.args.map((arg) => {
      if (arg.kind === "quality") {
        return arg.value;
      }
      return formatFormExpr(arg.expr);
    });
    s += `(${parts.join(", ")})`;
  }
  return s;
}

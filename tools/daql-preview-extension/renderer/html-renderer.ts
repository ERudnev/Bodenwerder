import type {
  Artifact,
  ArtifactBody,
  Document,
  FormArg,
  FormExpr,
  Inline,
  RuleItem,
  Section,
  TopLevelNode,
} from "../model/document";
import type { Renderer } from "./index";
import { formatFormExpr } from "../parser/form-expr";

function escapeHtml(text: string): string {
  return text
    .replace(/&/g, "&amp;")
    .replace(/</g, "&lt;")
    .replace(/>/g, "&gt;")
    .replace(/"/g, "&quot;");
}

function formClassName(name: string): string {
  return `daql-form--${name.toLowerCase()}`;
}

function renderInline(inline: Inline): string {
  switch (inline.kind) {
    case "text":
      return escapeHtml(inline.value);
    case "formRef": {
      const label = inline.qualifier
        ? `${inline.qualifier}::${inline.name}`
        : inline.name;
      return `<span class="daql-form-ref" title="@${escapeHtml(label)}">${escapeHtml(label)}</span>`;
    }
    case "artifactRef":
      return `<span class="daql-artifact-ref" title="&${escapeHtml(inline.name)}">${escapeHtml(inline.name)}</span>`;
    case "code":
      return `<code class="daql-inline-code">${escapeHtml(inline.value)}</code>`;
  }
}

function renderInlines(inlines: Inline[]): string {
  return inlines.map(renderInline).join("");
}

function renderQualityBadge(args: FormArg[] | undefined): string {
  if (!args) {
    return "";
  }
  const quality = args.find((a) => a.kind === "quality");
  if (!quality || quality.kind !== "quality") {
    return "";
  }
  return `<span class="daql-quality">${escapeHtml(quality.value)}</span>`;
}

function renderFormBadge(expr: FormExpr): string {
  const label = formatFormExpr(expr);
  const cls = formClassName(expr.name);
  const quality = renderQualityBadge(expr.args);
  return `<span class="daql-form ${cls}">${escapeHtml(label)}</span>${quality}`;
}

function renderLines(lines: Inline[][]): string {
  return lines.map((line) => `<p>${renderInlines(line)}</p>`).join("\n");
}

function renderSection(section: Section): string {
  const content = renderLines(section.lines);
  return `<section class="daql-field">
  <h3 class="daql-field-label">${escapeHtml(section.label)}</h3>
  <div class="daql-field-content">${content}</div>
</section>`;
}

function renderRuleItem(item: RuleItem): string {
  const text = `<span class="daql-rule-index">${escapeHtml(item.index)}</span> <span class="daql-rule-text">${renderInlines(item.text)}</span>`;
  if (item.children.length === 0) {
    return text;
  }
  const children = `<ol class="daql-rule-tree">${item.children
    .map((child) => `<li>${renderRuleItem(child)}</li>`)
    .join("")}</ol>`;
  return `${text}${children}`;
}

function renderRuleItems(items: RuleItem[]): string {
  return `<ol class="daql-rule-tree">${items
    .map((item) => `<li>${renderRuleItem(item)}</li>`)
    .join("")}</ol>`;
}

function renderBody(body: ArtifactBody): string {
  if (body.kind === "sections") {
    return body.sections.map(renderSection).join("\n");
  }

  const prologue =
    body.prologue.length > 0
      ? `<div class="daql-rule-prologue">${renderLines(body.prologue)}</div>`
      : "";
  const tree = body.items.length > 0 ? renderRuleItems(body.items) : "";
  return `${prologue}${tree}`;
}

function renderArtifact(artifact: Artifact): string {
  return `<article class="daql-artifact">
  <header class="daql-artifact-header">
    <span class="daql-artifact-name">${escapeHtml(artifact.name)}</span>
    ${renderFormBadge(artifact.form)}
  </header>
  <div class="daql-artifact-body">${renderBody(artifact.body)}</div>
</article>`;
}

function renderTopLevelNode(node: TopLevelNode): string {
  switch (node.kind) {
    case "comment":
      return `<div class="daql-comment">${escapeHtml(node.text)}</div>`;
    case "prose":
      return `<p class="daql-prose">${renderInlines(node.inlines)}</p>`;
    case "using":
      return `<div class="daql-using"><span class="daql-using-kw">#using</span> namespace <strong>${escapeHtml(node.namespace)}</strong> <span class="daql-using-file">(${escapeHtml(node.file)})</span></div>`;
    case "namespace": {
      const artifacts = node.artifacts.map(renderArtifact).join("\n");
      return `<section class="daql-namespace">
  <h1 class="daql-namespace-title">namespace ${escapeHtml(node.name)}</h1>
  ${artifacts}
</section>`;
    }
  }
}

export class HtmlRenderer implements Renderer {
  render(doc: Document): string {
    const body = doc.nodes.map(renderTopLevelNode).join("\n");
    return `<article class="daql-document">\n${body}\n</article>`;
  }
}

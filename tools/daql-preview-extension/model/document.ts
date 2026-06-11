export type Inline =
  | { kind: "text"; value: string }
  | { kind: "formRef"; name: string; qualifier?: string }
  | { kind: "artifactRef"; name: string }
  | { kind: "code"; value: string };

export type FormArg =
  | { kind: "quality"; value: string }
  | { kind: "form"; expr: FormExpr };

export interface FormExpr {
  name: string;
  args?: FormArg[];
}

export interface Section {
  label: string;
  lines: Inline[][];
}

export interface RuleItem {
  index: string;
  text: Inline[];
  children: RuleItem[];
}

export type ArtifactBody =
  | { kind: "sections"; sections: Section[] }
  | { kind: "rule"; prologue: Inline[][]; items: RuleItem[] };

export interface Artifact {
  name: string;
  form: FormExpr;
  body: ArtifactBody;
}

export type TopLevelNode =
  | { kind: "comment"; text: string }
  | { kind: "prose"; inlines: Inline[] }
  | { kind: "using"; namespace: string; file: string }
  | { kind: "namespace"; name: string; artifacts: Artifact[] };

export interface Document {
  nodes: TopLevelNode[];
}

import type { Document } from "../model/document";

export interface Renderer {
  render(doc: Document): string;
}

export { HtmlRenderer } from "./html-renderer";

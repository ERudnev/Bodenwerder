import type { Document } from "../model/document";

export interface Parser {
  parse(source: string): Document;
}

export { TomeParser } from "./tome-parser";

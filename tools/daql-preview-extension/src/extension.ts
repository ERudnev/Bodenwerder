import * as vscode from "vscode";
import { TomeParser } from "../parser";
import { HtmlRenderer } from "../renderer";
import { DaqlPreviewProvider } from "./preview/DaqlPreviewProvider";

const PREVIEW_VIEW_TYPE = "kqm.preview";
const KQM_LANGUAGE_ID = "kqm";

const parser = new TomeParser();
const renderer = new HtmlRenderer();

export function activate(context: vscode.ExtensionContext): void {
  const provider = new DaqlPreviewProvider(context.extensionUri, parser, renderer);

  context.subscriptions.push(
    vscode.window.registerCustomEditorProvider(PREVIEW_VIEW_TYPE, provider),
  );

  context.subscriptions.push(
    vscode.workspace.onDidOpenTextDocument((document) => {
      void ensureKqmLanguage(document);
    }),
  );

  for (const document of vscode.workspace.textDocuments) {
    void ensureKqmLanguage(document);
  }

  context.subscriptions.push(
    vscode.commands.registerCommand("kqm.showPreview", async (uri?: vscode.Uri) => {
      const resource = uri ?? getActiveTomeUri();
      if (!resource) {
        return;
      }
      await vscode.commands.executeCommand("vscode.openWith", resource, PREVIEW_VIEW_TYPE);
    }),
  );

  context.subscriptions.push(
    vscode.commands.registerCommand("kqm.showPreviewToSide", async (uri?: vscode.Uri) => {
      const resource = uri ?? getActiveTomeUri();
      if (!resource) {
        return;
      }
      await vscode.commands.executeCommand("vscode.openWith", resource, PREVIEW_VIEW_TYPE, {
        viewColumn: vscode.ViewColumn.Beside,
        preserveFocus: true,
      });
    }),
  );

  context.subscriptions.push(
    vscode.commands.registerCommand("kqm.openWithTextEditor", async (uri?: vscode.Uri) => {
      const resource = uri ?? getActiveTomeUri();
      if (!resource) {
        return;
      }
      await vscode.commands.executeCommand(
        "vscode.openWith",
        resource,
        "default",
        vscode.ViewColumn.Active,
      );
    }),
  );
}

async function ensureKqmLanguage(document: vscode.TextDocument): Promise<void> {
  if (!isTomeUri(document.uri)) {
    return;
  }
  if (document.languageId === KQM_LANGUAGE_ID) {
    return;
  }
  await vscode.languages.setTextDocumentLanguage(document, KQM_LANGUAGE_ID);
}

function isTomeUri(uri: vscode.Uri): boolean {
  return uri.fsPath.toLowerCase().endsWith(".tome");
}

function getActiveTomeUri(): vscode.Uri | undefined {
  const editor = vscode.window.activeTextEditor;
  if (editor && isTomeUri(editor.document.uri)) {
    return editor.document.uri;
  }

  const tab = vscode.window.tabGroups.activeTabGroup.activeTab;
  if (!tab) {
    return undefined;
  }

  if (tab.input instanceof vscode.TabInputCustom) {
    const input = tab.input;
    if (input.viewType === PREVIEW_VIEW_TYPE && isTomeUri(input.uri)) {
      return input.uri;
    }
  }

  if (tab.input instanceof vscode.TabInputText && isTomeUri(tab.input.uri)) {
    return tab.input.uri;
  }

  return undefined;
}

export function deactivate(): void {}

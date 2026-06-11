import * as vscode from "vscode";
import type { Parser } from "../../parser";
import type { Renderer } from "../../renderer";

const DEBOUNCE_MS = 100;

interface DebouncedUpdater {
  schedule(): void;
  cancel(): void;
}

export class DaqlPreviewProvider implements vscode.CustomTextEditorProvider {
  constructor(
    private readonly extensionUri: vscode.Uri,
    private readonly parser: Parser,
    private readonly renderer: Renderer,
  ) {}

  async resolveCustomTextEditor(
    document: vscode.TextDocument,
    webviewPanel: vscode.WebviewPanel,
    _token: vscode.CancellationToken,
  ): Promise<void> {
    webviewPanel.webview.options = {
      enableScripts: false,
      localResourceRoots: [vscode.Uri.joinPath(this.extensionUri, "media")],
    };

    const updatePreview = this.createDebouncedUpdater(document, webviewPanel);

    const changeSubscription = vscode.workspace.onDidChangeTextDocument((event) => {
      if (event.document.uri.toString() === document.uri.toString()) {
        updatePreview.schedule();
      }
    });

    webviewPanel.onDidDispose(() => {
      changeSubscription.dispose();
      updatePreview.cancel();
    });

    this.renderPreview(document, webviewPanel);
  }

  private createDebouncedUpdater(
    document: vscode.TextDocument,
    webviewPanel: vscode.WebviewPanel,
  ): DebouncedUpdater {
    let timer: ReturnType<typeof setTimeout> | undefined;

    return {
      schedule() {
        if (timer) {
          clearTimeout(timer);
        }
        timer = setTimeout(() => {
          timer = undefined;
          this.renderPreview(document, webviewPanel);
        }, DEBOUNCE_MS);
      },
      cancel() {
        if (timer) {
          clearTimeout(timer);
          timer = undefined;
        }
      },
    };
  }

  private renderPreview(
    document: vscode.TextDocument,
    webviewPanel: vscode.WebviewPanel,
  ): void {
    const source = document.getText();
    const model = this.parser.parse(source);
    const content = this.renderer.render(model);
    webviewPanel.webview.html = this.wrapHtml(content, webviewPanel.webview);
  }

  private wrapHtml(bodyContent: string, webview: vscode.Webview): string {
    const cssUri = webview.asWebviewUri(
      vscode.Uri.joinPath(this.extensionUri, "media", "preview.css"),
    );

    return `<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <meta http-equiv="Content-Security-Policy" content="default-src 'none'; style-src ${webview.cspSource} 'unsafe-inline'; img-src ${webview.cspSource};">
  <link rel="stylesheet" href="${cssUri}">
</head>
<body>
${bodyContent}
</body>
</html>`;
  }
}

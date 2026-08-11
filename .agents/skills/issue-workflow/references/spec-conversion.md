# Single Source-Spec Conversion

Read this file only when the selected source lacks a current Markdown derivative. Convert one source, never the whole directory.

## Normal Work

Reuse `.local-tools/ConvertSpecs.ps1` for the selected file.

## Dry Run or Stale Launcher

Convert to a temporary path, run targeted `rg`, then remove the temporary output. Do not install another converter.

1. Call `codex_app.load_workspace_dependencies` for its Python 3.12 path.
2. Run that Python with:

```powershell
-c "import runpy,sys; sys.path.insert(0,r'<repo>/.local-tools/markitdown/.venv/Lib/site-packages'); sys.argv=['markitdown',r'<source>','-o',r'<temp>']; runpy.run_module('markitdown.__main__',run_name='__main__')"
```

If the dependency loader is unavailable, use `C:/Users/RJW-DESKTOP/AppData/Local/pipx/pipx/venvs/code-review-graph/Scripts/python.exe` with the same arguments.

Never unzip or parse DOCX/XML as a substitute. Report MarkItDown unavailable when both existing runtimes fail.

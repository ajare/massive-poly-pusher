# PipelineEditor Command-Line Interface

Run commands from the deployed PipelineEditor output directory so runtime DLLs and `editor.ini` are available. `editor.ini` resolves the external repository-level `resources` directory; no resource tree is copied into the binary output.

## Help

```text
PipelineEditor.exe --help
```

Lists validation, package-export, smoke-test, and graphical-startup options.

## Validation

```text
PipelineEditor.exe --validate <pipeline.xml>
PipelineEditor.exe --validate --warnings-as-errors <pipeline.xml>
```

Validation loads strict native PBR pipeline XML (or imports a standalone RenderGraph through the compatibility loader), resolves strict resource libraries, validates concrete resources and graph semantics, and validates the referenced preview scene when present.

Exit status:

- `0`: no errors (and no warnings when `--warnings-as-errors` is active).
- `1`: parser or validation failure.
- `2`: command syntax error.

Diagnostics are written to stderr as `CODE: message`. Parse/loading exceptions use `MPP-PIPELINE-CLI-002`; no graphical error dialog is opened by validation mode.

## Package export

```text
PipelineEditor.exe --export-package <pipeline.xml> <package.mpppackage>
```

Exports the source workspace as a self-contained package without opening the editor UI. This is intended for automation and integration testing; interactive **File > Export Package...** exports the current unsaved in-memory workspace.

Exit status is `0` on success, `1` when the workspace or a dependency is invalid/missing, and `2` for invalid syntax.

## Graphical startup

```text
PipelineEditor.exe [options] [pipeline.xml]
```

Options:

- `--width <pixels>`: startup width, minimum 640.
- `--height <pixels>`: startup height, minimum 480.
- `--recovery-seconds <seconds>`: recovery interval, minimum 5.
- `--smoke-test`: render a loaded valid workspace for 30 stable frames, cleanly shut down, and return. Intended for active-context integration tests.

Settings not supplied on the command line are read from `PipelineEditor.cfg`. The required `editor.ini` beside the executable contains `[Editor] resourcesLocation`; relative locations are resolved from the INI file. Its strict `[mpp]` section supplies global anti-aliasing defaults for validation, preview rendering, CLI validation, and package export; see [ANTI_ALIASING_CONFIGURATION.md](ANTI_ALIASING_CONFIGURATION.md). A positional pipeline opens as one workspace together with its referenced scene and libraries. With no positional pipeline, the configured resource tree's Full template and default scene open as an untitled workspace; saving therefore requires a new destination and cannot overwrite the shipped template.

## Examples

```text
PipelineEditor.exe --validate ..\..\..\..\..\..\resources\shared\pbr\templates\Full.pipeline.xml
PipelineEditor.exe --validate --warnings-as-errors my.pipeline.xml
PipelineEditor.exe --smoke-test ..\..\..\..\..\..\resources\shared\pbr\templates\Shadows.pipeline.xml
PipelineEditor.exe --export-package ..\..\..\..\..\..\resources\shared\pbr\templates\Full.pipeline.xml full.mpppackage
```

See [PIPELINE_EDITOR_DIAGNOSTICS.md](PIPELINE_EDITOR_DIAGNOSTICS.md) for code families and [PBR_PIPELINE_XML_SPECIFICATION.md](PBR_PIPELINE_XML_SPECIFICATION.md) for document rules.

# PipelineEditor Command-Line Interface

Run commands from the deployed PipelineEditor output directory so `resources/shared` and runtime DLLs are available.

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

## Graphical startup

```text
PipelineEditor.exe [options] [pipeline.xml]
```

Options:

- `--width <pixels>`: startup width, minimum 640.
- `--height <pixels>`: startup height, minimum 480.
- `--recovery-seconds <seconds>`: recovery interval, minimum 5.
- `--smoke-test`: render a loaded valid workspace for 30 stable frames, cleanly shut down, and return. Intended for active-context integration tests.

Settings not supplied on the command line are read from `PipelineEditor.cfg`. A positional pipeline opens as one workspace together with its referenced scene and libraries. With no positional pipeline, the shipped Full template and default scene open as an untitled workspace; saving therefore requires a new destination and cannot overwrite the shipped template.

## Examples

```text
PipelineEditor.exe --validate resources/shared/pbr/templates/Full.pipeline.xml
PipelineEditor.exe --validate --warnings-as-errors my.pipeline.xml
PipelineEditor.exe --smoke-test resources/shared/pbr/templates/Shadows.pipeline.xml
```

See [PIPELINE_EDITOR_DIAGNOSTICS.md](PIPELINE_EDITOR_DIAGNOSTICS.md) for code families and [PBR_PIPELINE_XML_SPECIFICATION.md](PBR_PIPELINE_XML_SPECIFICATION.md) for document rules.

# MassivePolyPusher

An OpenGL 2d and 3d renderer.

## Cloning

Run `git clone --recurse-submodules -j8 https://bitbucket.org/wtmrsh/massivepolypusher.git`.  You will need git version 2.13 for `--recurse-submodules` and 2.8+ for `-j8`.

## Building

First build the submodules:

`msbuild ext\utils\build\vs2017\Utils.sln -target:UtilsTests:Rebuild -p:Platform=Win32 -p:Configuration=Release`

Then the main project. The current editor/tool configuration is VS2026 x64:

`msbuild build\vs2026\MassivePolyPusher.sln -target:Build -p:Platform=x64 -p:Configuration=Release`

`PipelineEditor` is a separate executable under `pipeline-editor\build\vs2026\bin\x64\<Configuration>`. Its post-build deployment copies `editor.ini`, which references the repository-level `resources` directory beside the root `build` directory.

PipelineEditor reads MassivePolyPusher defaults from the `[mpp]` section of `editor.ini`; DemoSuite reads the same section from `demosuite.ini` beside its executable. Supported phase-one settings are `msaa=off|2x|4x|8x`, `ssaa=off|2x|4x|8x`, and Boolean `taa`/`fxaa`. All default to off. Invalid keys, values, or GPU-incompatible startup dimensions/sample counts fail startup instead of silently falling back. These settings establish typed global defaults. PBR pipelines now declare explicit named outputs with inheritable per-output anti-aliasing overrides; legacy render-graph `<samples>` fields are rejected with a migration diagnostic. Physical MSAA, SSAA, TAA, and FXAA rendering is added in subsequent anti-aliasing phases.

## PBR PipelineEditor

Start with `resources/shared/pbr/templates/Minimal.pipeline.xml`, `Shadows.pipeline.xml`, `Full.pipeline.xml`, or `Empty.pipeline.xml`. The reusable default scene is `resources/shared/pbr/DefaultPbrPreview.scene.xml`. PipelineEditor loads the repository-level `resources` tree through the deployed `editor.ini`; resources are not copied into its binary output directory.

Documentation:

- [PipelineEditor authoring guide](doc/PIPELINE_EDITOR_AUTHORING_GUIDE.md)
- [PBR pipeline XML specification](doc/PBR_PIPELINE_XML_SPECIFICATION.md)
- [Preview scene XML specification](doc/PBR_SCENE_XML_SPECIFICATION.md)
- [Diagnostics catalogue](doc/PIPELINE_EDITOR_DIAGNOSTICS.md)
- [CLI validation and smoke tests](doc/PIPELINE_EDITOR_CLI.md)

## Pipeline packages

PipelineEditor can export the current valid pipeline and preview scene, including unsaved edits, through **File > Export Package...**. The resulting self-contained `.mpppackage` is a standard ZIP archive containing `manifest.xml`, `pipeline.xml`, `scene.xml`, localized external resources, and referenced assets.

Load a package without any built-in DemoSuite content:

```text
DemoSuite.exe --package path\to\workspace.mpppackage
```

Run `PipelineEditor.exe --help` or `DemoSuite.exe --help` to list their command-line options. DemoSuite validates and extracts the package to a temporary directory; an invalid package is reported as an error and does not fall back to the built-in scene. For automated runtime smoke testing, append `--package-smoke-test`; it renders 30 frames and exits.

Blender-style camera controls are available in the PipelineEditor viewport and in DemoSuite package mode: middle-drag or **Alt+left-drag** orbits; Shift modifies either drag to pan; Ctrl modifies either drag to dolly. This Alt+left-drag fallback supports trackpads without a middle button or scroll wheel. A mouse wheel also dollies.

PipelineEditor can capture only the viewport rendering commands with RenderDoc. Use the camera toolbar button for the default **Capture and Open** action, its dropdown for capture-only, or press **Ctrl+F12**. RenderDoc and capture-directory paths are stored in the deployed `editor.ini` and editable under **Edit > Preferences**. Captures default to `pipeline-editor/captures`, which is ignored by Git. Capture actions are disabled while a preview generation is rebuilding or being installed.

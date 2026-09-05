# MassivePolyPusher

An OpenGL 2d and 3d renderer.

## Cloning

Run `git clone --recurse-submodules -j8 https://bitbucket.org/wtmrsh/massivepolypusher.git`.  You will need git version 2.13 for `--recurse-submodules` and 2.8+ for `-j8`.

## Building

### CMake (Linux)

Install CMake 3.22 or newer, a C++20-capable compiler, and the OpenGL development files. On Debian or Ubuntu, the minimum packages can be installed with:

```sh
sudo apt update
sudo apt install build-essential cmake git-lfs libgl-dev
```

Clone with submodules, or initialize them in an existing checkout. The demo package and other large assets use Git LFS, so fetch those files before building:

```sh
git lfs install
git submodule update --init --recursive
git lfs pull
```

Configure and build a Release build from the repository root:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

Executables and shared libraries are written to `build/bin/Release`. The Linux build produces `DemoSuite`, `PipelineEditor`, `ModelConvert`, and `ProgramBuilder`. Run the editor with `./build/bin/Release/PipelineEditor`, the default packaged demo with `./build/bin/Release/DemoSuite`, or the standalone particle demo without a package with `./build/bin/Release/DemoSuite --particles`. PipelineEditor uses SDL for native messages, process launching, and dynamic RenderDoc loading on Linux; configure the `[RenderDoc]` executable as the path to `qrenderdoc` to enable captures.

If DemoSuite reports `Package ZIP directory is missing`, the copied package is probably a Git LFS pointer rather than the package data. Run `git lfs pull`, then run `cmake --build build --parallel` again so CMake recopies the package into `build/bin/Release`.

### CMake (VS2026 x64)

```bat
cmake -S . -B build -G "Visual Studio 18 2026" -A x64
cmake --build build --config Release --parallel
```

The CMake build includes all engine, tool, and application targets, builds SDL from `ext/sdl`, Assimp from `ext/assimp`, GLEW from `ext/glew`, and Utils from `ext/utils`, and places executables and runtime DLLs under `build/bin/<Configuration>`. Because the GLEW Git repository omits generated headers and `glew.c`, the first configure downloads the hash-verified official source archive matching the pinned submodule release. DemoSuite and PipelineEditor resolve their checked-in `../../../resources` settings directly to the repository-level `resources` directory; resource assets are not copied or symlinked into the build tree.

### Existing Visual Studio projects

The checked-in Visual Studio projects predate the source-built dependency integration. Use the CMake-generated VS2026 solution for builds that include the SDL, Assimp, GLEW, and Utils submodules.

The CMake-built `PipelineEditor` executable is under `build\bin\<Configuration>`. Its post-build deployment copies `editor.ini`, whose relative path resolves directly to the repository-level `resources` directory beside `build`.

PipelineEditor reads MassivePolyPusher defaults from the `[mpp]` section of `editor.ini`; DemoSuite reads the same section from `demosuite.ini` beside its executable. See [`doc/ANTI_ALIASING_CONFIGURATION.md`](doc/ANTI_ALIASING_CONFIGURATION.md) for configuration, named-output authoring, ordering, constraints, errors, and package behavior. Supported settings are `msaa=off|2x|4x|8x`, `ssaa=off|2x|4x|8x`, Boolean `taa`/`fxaa`, and `particlePoolCapacity=262144..1048576`. Anti-aliasing defaults to off and particle capacity defaults to 262,144. Invalid keys, values, or GPU-incompatible startup dimensions/sample counts fail startup instead of silently falling back. These settings establish typed global defaults. PBR pipelines now declare explicit named outputs with inheritable per-output anti-aliasing overrides; legacy render-graph `<samples>` fields are rejected with a migration diagnostic. Screen and offscreen graph outputs now pass through a shared transactional renderer-owned output chain, with immutable physical plans and retained prior generations on allocation/resize failure. MSAA now uses renderer-private multisample raster attachments with automatic colour/depth resolves before sampled reads and output processing. SSAA uses total-sample √2/2/√8 raster scaling for viewport-relative graph resources followed by separable alpha-preserving Lanczos downsampling to the logical screen/offscreen output. TAA runs at supersampled resolution using shared eight-sample Halton camera jitter, resolved-depth reprojection, depth rejection, 3×3 neighbourhood clamping, and transactional per-output colour/depth histories before SSAA downsampling. A fixed high-quality LDR FXAA pass runs last at logical output resolution, with contrast thresholds, directional edge search, subpixel refinement, and centre-alpha preservation. FXAA may vary per named output.

## PBR PipelineEditor

Start with `resources/shared/pbr/templates/Minimal.pipeline.xml`, `Shadows.pipeline.xml`, `Full.pipeline.xml`, or `Empty.pipeline.xml`. The reusable default scene is `resources/shared/pbr/DefaultPbrPreview.scene.xml`. PipelineEditor loads the repository-level `resources` tree through the deployed `editor.ini`; resources are not copied into its binary output directory.

Documentation:

- [PipelineEditor authoring guide](doc/PIPELINE_EDITOR_AUTHORING_GUIDE.md)
- [PBR pipeline XML specification](doc/PBR_PIPELINE_XML_SPECIFICATION.md)
- [Preview scene XML specification](doc/PBR_SCENE_XML_SPECIFICATION.md)
- [Diagnostics catalogue](doc/PIPELINE_EDITOR_DIAGNOSTICS.md)
- [CLI validation and smoke tests](doc/PIPELINE_EDITOR_CLI.md)

## Particle Editor

The cross-platform `ParticleEditor` target creates, opens, previews, and atomically saves canonical `*.particle.yaml` assets. Multiple tabs keep independent command histories, diagnostics, file revisions, dirty state, and last-valid preview state. Save and close warnings protect invalid, externally changed, and unsaved documents. The hierarchy provides undoable emitter-template add, duplicate, rename, reorder, and remove operations with unique names and a derived effect budget. The inspector authors core spawn shapes, emission and random ranges plus independently toggled gravity, drag, noise, curl-noise, turbulence, vector-field, and collision modules. Appearance authoring covers logical texture resources, atlas playback, colour, emissive, fading, billboard orientation, blending, sorting, culling, and distortion; mesh particles select logical model resources with an optional ordinary-material override, while bounded lighting exposes emitter-level proxy, PBR injection, and volumetric controls. The editor-only `resourceRoot` and `resourceLibrary` settings in `particle-editor.ini` populate typed resource selectors without storing filesystem paths in particle effects. Unresolved logical names remain editable and serialize unchanged, with field-specific diagnostics. Its graphical and numeric editors cover every scalar curve and the colour gradient, with ordered normalized keys, colour picking, and gesture-level undo. Valid changes reach the MPP preview after a short debounce through the canonical serialized resource stream; safe spawn and appearance changes update live particles while structural changes restart the preview, and invalid edits retain the last valid result. The editor starts with a deterministic bounded version-2 particle effect and can switch its live simulation between PBR and legacy particle graphs. Both modes share a bounds-sized studio, paired material presets, floor grid, orbit camera, and optional manipulable orbit light. Preview-only vector-field and SDF resources, SDF transform/scale/isovalue, and stable floor/wall collisions provide the global behaviour inputs, while active-graph depth is retained for screen-space collision. Missing required inputs are warned about without changing authored modules. These preview choices are stored in the platform editor-preferences directory and never in particle YAML. Its dockable shell includes the document tabs, Particle Effect inspector, MPP viewport, toolbar, status bar, and diagnostics view. The deployed `particle-editor.ini` resolves the shared resource tree from `build/bin/<Configuration>`.

Run its context-free document and preview-preference tests or production particle-file validation without creating a window:

```text
ParticleEditor.exe --document-tests
ParticleEditor.exe --preview-tests
ParticleEditor.exe --resource-tests
ParticleEditor.exe --validate path/to/effect.particle.yaml
```

Validation prints stable production diagnostic codes and returns a non-zero process status when the particle effect is invalid.

## Pipeline packages

PipelineEditor can export the current valid pipeline and preview scene, including unsaved edits, through **File > Export Package...**. The resulting self-contained `.mpppackage` is a standard ZIP archive containing `manifest.xml`, `pipeline.xml`, `scene.xml`, localized external resources, and referenced assets.

Load a package without any built-in DemoSuite content:

```text
DemoSuite.exe --package path\to\workspace.mpppackage
```

Run `PipelineEditor.exe --help` or `DemoSuite.exe --help` to list their command-line options. DemoSuite validates and extracts the package to a temporary directory; an invalid package is reported as an error and does not fall back to the built-in scene. For automated runtime smoke testing, append `--package-smoke-test`; it renders 30 frames and exits.

Blender-style camera controls are available in the PipelineEditor viewport and in DemoSuite package mode: middle-drag or **Alt+left-drag** orbits; Shift modifies either drag to pan; Ctrl modifies either drag to dolly. This Alt+left-drag fallback supports trackpads without a middle button or scroll wheel. A mouse wheel also dollies.

PipelineEditor can capture only the viewport rendering commands with RenderDoc. Use the camera toolbar button for the default **Capture and Open** action, its dropdown for capture-only, or press **Ctrl+F12**. RenderDoc and capture-directory paths are stored in the deployed `editor.ini` and editable under **Edit > Preferences**. Captures default to `pipeline-editor/captures`, which is ignored by Git. Capture actions are disabled while a preview generation is rebuilding or being installed.

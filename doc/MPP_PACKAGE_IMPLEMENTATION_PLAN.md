# Pipeline Package Implementation Plan

## Goal

Export the PipelineEditor’s current in-memory PBR workspace as a self-contained standard-ZIP `.mpppackage`, then load it exclusively in DemoSuite with `--package <file>`.

## Confirmed requirements

- Export includes unsaved editor changes.
- A package contains the scene, pipeline, materials, all transitive payloads, and localized external-library resources.
- Missing dependencies make export fail with diagnostics; no incomplete archive is written.
- Export is available as **File > Export Package...** only when a valid pipeline and preview scene are loaded.
- DemoSuite loads a package through `--package <file>`, does not create built-in demo content in package mode, and exits with an error for invalid packages.
- The `.mpppackage` container is a standard ZIP archive.
- Named-output anti-aliasing overrides are serialized in the packaged pipeline. `inherit` values use the loading DemoSuite's defaults; explicit values are host-independent. See [ANTI_ALIASING_CONFIGURATION.md](ANTI_ALIASING_CONFIGURATION.md).

## Work items

1. **Package infrastructure** — complete
   - [x] Add a small, dependency-free ZIP store reader/writer with CRC-32 validation.
   - [x] Define and validate a strict versioned package manifest.
   - [x] Safely extract archives to a unique temporary directory.
2. **Editor export** — in progress
   - [x] Gather and validate direct pipeline/scene payloads; package each MPP model with its complete source directory so model-internal relative dependencies are retained.
   - [x] Localize external resources and rewrite direct resource references.
   - [x] Rewrite file references to package-relative paths and serialize in-memory documents.
   - [x] Add File > Export Package... and export diagnostics.
3. **DemoSuite loading** — complete
   - [x] Parse `--package <file>` before application setup.
   - [x] Extract and validate the version-1 manifest, then instantiate pipeline and scene runtimes.
   - [x] Run only the package scene/pipeline and clean temporary files on shutdown.
4. **Validation and documentation** — complete
   - [x] Add non-GPU ZIP package round-trip, overwrite, and unsafe-path tests.
   - [x] Run end-to-end positive and negative package loading tests (missing document, invalid manifest, compressed ZIP, and corrupt CRC).
   - [x] Document export and command-line loading.

## Post-validation hardening — complete

- [x] Enforce ZIP entry-count, per-entry-size, and total-uncompressed-size limits.
- [x] Include individual package pipeline/scene runtime diagnostic codes and messages in DemoSuite failures.
- [ ] Manual visual comparison is intentionally deferred.

## Progress log

- 2026-08-08: Created `feature/pipeline-package`; recorded agreed package behavior.
- 2026-08-08: Added `mpp::app::ZipArchive`, a ZIP-store writer and safe extractor with CRC validation. It is now built by MppAppSupport.
- 2026-08-08: Added in-memory PipelineEditor export, external-resource localization, package-relative payload paths, and the File > Export Package command.
- 2026-08-08: Added DemoSuite `--package` extraction and exclusive `PackageScene` runtime path. Build validation is blocked in this environment because MSBuild and a C++ compiler are unavailable.
- 2026-08-08: Extended export to retain each MPP model directory hierarchy, preserving model-internal relative texture and shader references.
- 2026-08-08: Documented package export and DemoSuite command-line loading in the repository README.
- 2026-08-08: Added a non-GPU ZIP package round-trip/overwrite test to the document-foundation test suite and made package replacement atomic on Windows.
- 2026-08-08: Preserved shader source directories during export so shader-relative includes are bundled with their entry shaders.
- 2026-08-08: Hardened ZIP extraction against duplicate entries and added unsafe-path rejection coverage; export now reports concrete validation diagnostics.
- 2026-08-08: Built the complete VS2026 x64 Release solution successfully with MSBuild (after correcting ZIP stream-position and mixed-`auto` declarations found by the compiler).
- 2026-08-08: Completed package infrastructure with strict version-1 manifest I/O and unique temporary extraction/export directories.
- 2026-08-08: Added non-UI `--export-package` and `--package-smoke-test` commands, exported the Full PBR workspace, verified it with Python's standard ZIP reader, and rendered it successfully for 30 DemoSuite frames (exit 0). Fixed ZIP local-header parsing and PackageScene's base scene type during this end-to-end test.
- 2026-08-08: Negative DemoSuite package tests correctly rejected a missing pipeline document, invalid manifest, compressed ZIP, and CRC-corrupt payload (each exited 1).
- 2026-08-08: Added ZIP-bomb limits (4,096 entries, 512 MiB per entry, 2 GiB total) and detailed DemoSuite package diagnostics. Manual visual comparison remains intentionally deferred.
- 2026-08-08: Added tested `--help`/`-h` command-line option summaries to PipelineEditor and DemoSuite.
- 2026-08-08: Fixed the document-foundation ZIP test leaving its input handle open before an atomic replacement, which caused an Access Denied failure on Windows.
- 2026-08-08: Made DemoSuite help attach to the parent console and write directly to `CONOUT$`, with a message-box fallback for launches without a console; it terminates with `ExitProcess(0)` after help is written.
- 2026-08-08: Added package-mode Blender camera controls: MMB/Alt+left orbit, Shift-pan, Ctrl-dolly, and wheel dolly; Alt+left combinations support trackpads without MMB or a wheel.
- 2026-08-08: Fixed package rendering binding graph input and output to the same screen target, which could stall the GPU and event loop. Package graphs now render to their runtime presentation texture and copy it to the screen. Also made smoke-test option order independent.
- 2026-08-08: Fixed Debug render-graph GPU tests labeling generated framebuffer names before first bind; strict OpenGL validation reported `GL_INVALID_VALUE`. GPU test failures now include their stage and source location.
- 2026-08-08: Made all `RenderSystem::renderText` overloads establish an orthographic, depth-disabled screen-space state. Package presentation is copied first, then nearest-filtered text overlays render directly to the backbuffer so no fullscreen pass resamples them.
- 2026-08-08: Routed SDL relative mouse motion through `InputManager` independently of ImGui. Package orbit/pan/dolly now consume per-frame SDL deltas instead of polling absolute cursor positions.
- 2026-08-08: Added the same Blender-style MMB and Alt+left trackpad camera controls to the PipelineEditor viewport and documented them inline.
- 2026-08-08: Fixed invisible debug text by replacing the fragile point-sprite glyph path with triangle glyphs and correcting undefined `gl_PointSize` use. Added a GPU readback regression test proving `renderText()` produces visible pixels after render-graph execution.
- 2026-08-08: Fixed triangle text uploading primitive count instead of vertex count and limiting six-vertex glyphs against a one-vertex capacity. Complete glyph buffers are now uploaded using the existing nearest-filtered font atlas.
- 2026-08-08: Reimplemented formatted-text `[#RRGGBBAA]` token parsing for both point and triangle glyphs. The shader applies requested RGB independently while using atlas alpha for coverage; a GPU regression test verifies that `80` alpha is translucent relative to `FF`.
- 2026-08-08: Re-enabled point-sprite text when the driver supports 16-pixel points. The 2D setup now explicitly enables program point size and point sprites; Debug GPU readback confirms point text visibility and formatted alpha, while triangles remain the capability fallback.
- 2026-08-08: Added PipelineEditor viewport-only RenderDoc capture with a camera split-button, Ctrl+F12 capture-and-open shortcut, editable `editor.ini` paths, regeneration gating, timestamped ignored captures, and optional qrenderdoc launch.

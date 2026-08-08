# PipelineEditor Authoring Guide

## Starting a workspace

Build `PipelineEditor` for VS2026 x64 and run it from its output directory. The `editor.ini` beside the executable resolves the repository-level `resources` directory, which remains outside the output tree. Use **File > New** to create an untitled workspace from:

- **Minimal PBR Pipeline** — HDR PBR scene and tone-map presentation.
- **PBR Shadows Pipeline** — minimal pipeline plus directional shadow depth.
- **Full PBR Pipeline** — shadows, HDR scene, bloom, and tone mapping.
- **Empty Pipeline** — presentation import/image with no authored passes or scene.

The first three use the shared default scene and declare an explicit `Main` output with inherited anti-aliasing settings. Anti-aliasing defaults come from `[mpp]` in `editor.ini`; image-level `<samples>` authoring is no longer supported. A new pipeline and scene are untitled copies: explicit Save As operations never overwrite shipped templates.

## Layout and hierarchy

PipelineEditor uses one resizable, maximizable native window with a fixed toolbar directly below the menu bar. The hierarchy selects passes, images, imports, resources, environment, bindings, overrides, models, lights, camera, and layers. The inspector edits the selected item. The right dock contains only the viewport; diagnostics, allocations, and statistics share the lower-left dock. **Window > Reset Layout** restores the default arrangement.

Passes, local resources, and scene models support drag ordering. A scene model chooses its logical material binding from the pipeline bindings combo and chooses any number of declared render layers from the multi-select combo; undeclared existing assignments remain visible until removed. Invalid reorder and delete operations are intentionally allowed, diagnosed, and undoable. **Pipeline > Auto-order Pass Dependencies** is an explicit command; ordering is never silently changed.

## Editing workflow

1. Select a hierarchy item and edit its inspector.
2. Read Diagnostics before rebuilding. Required imports, fallback classification, format support, stable values, reflection, layers, and bindings are validated continuously.
3. Use **Apply/Rebuild** to request immediate preparation, or wait for the edit debounce.
4. CPU parsing, validation, dependency reads, and image decoding run in a cancellable worker generation. GPU validation and installation run on the render thread.
5. If a generation fails, the viewport remains on the last complete valid generation and displays **STALE PREVIEW**.
6. Save the pipeline, scene, or both. Invalid working documents can be saved only after confirmation.

Continuous inspector changes coalesce into undoable commands. Undo/redo selection is reset when topology changes to avoid stale identities.

## Resources and bindings

Author only concrete `PbrMaterial`, `Program`, `Texture`, and `Sampler` resources. Selecting a PBR material lists enabled material-aware scene passes that use it through the active scene bindings plus enabled material-independent passes that always run. Material passes are highlighted in amber and independent passes in blue in the Pipeline Hierarchy. External libraries are read-only and use `Library::Resource`; select an external child and choose **Make Local Copy** before editing it. Renaming or deleting local resources updates direct and nested references.

Pipeline environments own IBL/background resources. Select **Bloom Settings** in the hierarchy to enable or disable bloom and choose how many authored horizontal/vertical blur pairs execute; requesting more pairs than the graph contains is diagnosed. The pass hierarchy retains the complete authored chain but immediately marks globally disabled bloom passes and blur pairs beyond the selected count as **[bypassed]**; those passes execute only the compatibility copy needed to preserve graph values. Neutral fallbacks are explicit diagnostics, not hidden scene state. Logical preview bindings keep scenes independent from concrete resources. Instance overrides may reduce enabled material capabilities but cannot enable shader features that were specialized out.

## Viewport controls

- Left-drag empty viewport space: orbit.
- Right-drag: pan (middle-drag is also supported).
- Mouse wheel: zoom.
- **Reset View**: restore the authored camera.
- **Frame Selection**: frame the selected model.
- **Save Current View**: write camera position/target/clipping to the scene as an undoable edit.

Enable **Inspect selected image** to display an authored graph image/value. Select value version and mip, then choose colour, R/G/B, alpha, luminance, linear depth, HDR tone-map, or HDR heat-map visualization. Attachment-only images use display-safe diagnostic targets. Physical MSAA processing is owned by named pipeline outputs rather than graph-image fields. SSAA similarly scales viewport-relative graph resources from the effective named-output setting, leaves absolute resources such as shadow maps unchanged, and Lanczos-downsamples into the logical preview output. TAA runs before that downsample and requires matching resolved depth; PipelineEditor marks regenerated/reloaded camera views as cuts, while interactive teleports and projection discontinuities are detected conservatively. FXAA is the final logical-resolution LDR pass and may be enabled or disabled independently for each named output.

Statistics report FPS, submitted triangles, scene inventory, pass CPU duration, and asynchronous GPU duration. GPU timing is shown as available, pending, or unsupported and never blocks the frame.

## Files, recovery, and external changes

Pipeline and scene dirty state is independent. Save All saves an untitled scene first so the pipeline can store its relative reference. Writes use atomic temporary replacement. Recovery copies are separate from explicit saves and are removed after successful save/discard handling.

Dependency watching includes pipeline/scene XML, libraries, texture and shader payloads, and model files. Clean workspaces hot-reload transactionally. Dirty workspaces show the conflict banner and require **Reload**, **Overwrite**, or **Keep Local**; external libraries are never overwritten.

## Related documentation

- [PBR_PIPELINE_XML_SPECIFICATION.md](PBR_PIPELINE_XML_SPECIFICATION.md)
- [PBR_SCENE_XML_SPECIFICATION.md](PBR_SCENE_XML_SPECIFICATION.md)
- [PIPELINE_EDITOR_DIAGNOSTICS.md](PIPELINE_EDITOR_DIAGNOSTICS.md)
- [PIPELINE_EDITOR_CLI.md](PIPELINE_EDITOR_CLI.md)
- [PBR_MATERIAL_AUTHORING.md](PBR_MATERIAL_AUTHORING.md)
- [RENDER_GRAPH_SPECIFICATION.md](RENDER_GRAPH_SPECIFICATION.md)

# PipelineEditor Diagnostics Catalogue

Diagnostics have a stable code, severity, message, source path, and optional subject. The message may include an authored identity or GPU detail; automation should match the code. Errors block preview installation. Warnings permit installation unless CLI `--warnings-as-errors` is used.

## Code families

| Family | Meaning |
|---|---|
| `MPP-PIPELINE-001`–`029` | Pipeline version/name, resources, imports, environment, bindings, overrides, paths, uniqueness, and portability. |
| `MPP-PIPELINE-RESOURCE-001` | A concrete local or external resource payload failed parser-level validation. |
| `MPP-PIPELINE-RUNTIME-001`–`007` | Runtime resource resolution, required imports, fallback, environment, binding, and override failures. |
| `MPP-PIPELINE-CLI-001` | Referenced preview scene is missing. |
| `MPP-PIPELINE-CLI-002` | Strict XML parsing/loading failed before semantic diagnostics could be produced. |
| `MPP-GRAPH-001` | Render graph structure, stable-value dependency, order, attachment, format, allocation, or active-GPU validation. |
| `MPP-PASS-001`–`012` | Pass factory metadata, required/optional slot, parameter, output, raster, blend, or reflection validation. |
| `MPP-SCENE-001`–`030` | Scene version/name, identity, model, primitive, transform, layer, light, camera, binding, path, and migration validation. |
| `MPP-SCENE-RUNTIME-001`–`007` | Scene resource creation, model loading/placeholder, material/environment resolution, collision, and runtime installation. |

## Frequently actionable codes

| Code | Meaning and action |
|---|---|
| `MPP-PIPELINE-001` | Unsupported pipeline version. Migrate to version 1. |
| `MPP-PIPELINE-011` | Optional import lacks an explicit fallback. Make it required or select a compatible fallback. |
| `MPP-PIPELINE-RESOURCE-001` | Concrete resource XML is invalid. Inspect the nested parser message and resource inspector. |
| `MPP-PIPELINE-CLI-001` | `PreviewScene/file` does not resolve from the pipeline location. |
| `MPP-PIPELINE-CLI-002` | Root/core XML, enum, or resource-library parsing failed. Unknown core fields are rejected. |
| `MPP-GRAPH-001` | Graph topology/order/allocation is invalid. Inspect authored pass order and stable value references. |
| `MPP-SCENE-007` | Camera values or clip distances are invalid. Use finite values, positive near, and far greater than near. |
| `MPP-SCENE-010` | Model file is missing. A placeholder will render and is excluded from triangle totals. |
| `MPP-SCENE-014` | More than eight authored PBR lights. Remove or disable excess lights. |
| `MPP-SCENE-021` | A model references an undeclared render layer. Add it under top-level `Layers`. |
| `MPP-SCENE-023` | A point light requests shadows; version 1 supports directional shadows only. |
| `MPP-SCENE-024` | More than one directional shadow light. Select one shadow owner. |
| `MPP-SCENE-030` | Legacy scene omitted `Layers`; inferred declarations will be canonicalized on save. |
| `MPP-SCENE-RUNTIME-003` | Model load failed and a diagnosed placeholder was installed. |
| `MPP-SCENE-RUNTIME-007` | Scene environment binding does not match the pipeline environment binding. |

## Parse errors versus semantic errors

Strict parser failures do not produce partially trusted documents. The CLI wraps these as `MPP-PIPELINE-CLI-002`; the graphical editor reports the operation error and retains the previous workspace. Semantic errors are collected together and displayed by code.

Invalid regression fixtures and their expected codes are catalogued in `pipeline-editor/resources/invalid/EXPECTED_DIAGNOSTICS.md`.

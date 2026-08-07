# Invalid fixture expectations

| Fixture | Expected stable diagnostic or parser result |
|---|---|
| `MissingOptionalFallback.pipeline.xml` | `MPP-PIPELINE-011` |
| `InvalidLocalSampler.pipeline.xml` | `MPP-PIPELINE-RESOURCE-001` |
| `UnknownCoreField.pipeline.xml` | `MPP-PIPELINE-CLI-002` |
| `UnsupportedVersion.pipeline.xml` | `MPP-PIPELINE-001` |
| `InvalidGraphReference.pipeline.xml` | `MPP-PIPELINE-CLI-002` |
| `InvalidScene.pipeline.xml` | `MPP-SCENE-007` |
| `PointShadow.scene.xml` | `MPP-SCENE-023` |
| `UndeclaredLayer.scene.xml` | `MPP-SCENE-021` |

Standalone scene fixtures are exercised by parser/runtime tests or through a referencing pipeline. CLI fixture tests must assert non-zero status and the expected code rather than matching implementation-specific exception text.

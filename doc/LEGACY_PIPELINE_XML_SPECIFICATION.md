# Legacy pipeline ambient occlusion

`LegacyPipeline` XML and YAML documents serialize ambient occlusion as:

```xml
<AmbientOcclusion>
    <method>gtao</method>
    <GTAO><normalSource>mrt</normalSource></GTAO>
</AmbientOcclusion>
```

`AmbientOcclusion/GTAO/normalSource` accepts `depth` (the default when omitted) and `mrt`. `depth` preserves the historical depth-reconstructed GTAO graph, attachment count, and hardware requirements. Historical root-level `SSAO` sections still select SSAO and always use depth-reconstructed normals.

With `mrt`, the loader automatically adds an `RG16F` sampled shading-normal attachment to `MPP.LegacyScene` at colour-output location 2 and binds it as the GTAO raw pass `NORMALS` sampler. Legacy scene shaders must write an octahedrally encoded **view-space shading normal** at location 2. Location 0 remains scene colour and location 1 is an automatically reserved attachment, so MRT GTAO requires three colour attachments and three draw buffers. There is no depth fallback in MRT mode: missing attachments, bindings, shader output, or device capability are validation errors.

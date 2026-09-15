---
status: accepted
---

# A Batch reports the resources it cannot delete, and a pipeline releases a frame's scene models when the frame ends

A `Batch` declares a `Model` and a `Material` resource and owns their name
registrations, but both are also acquired by other wranglers -- above all the
`SceneModel3d` that wraps the Model. `Batch::~Batch` deleted a resource only
when its reference count was already zero. If any wrangler outlived the Batch,
the delete was skipped with no diagnostic: the resource stayed `Loaded` and its
name stayed registered, so the next object that declared the same name failed
one teardown later, far from the ordering mistake that caused it.

The wrangler that outlived the Batch was held by the `RenderPipeline` itself.
`RenderPipeline::renderGraphForward` registers the generated graph's passes
with `RenderGraphExecutor::setPassCallback`, and the shadow and scene callbacks
captured `shadowModels` and `models` -- `std::vector<SceneModel3dPtr>` -- and
the `ScenePtr` by value. Those callbacks live in the executor, a member of the
pipeline, and were only cleared at the start of the next frame's graph build.
A pipeline therefore kept the last frame's `SceneModel3d` objects alive for as
long as the pipeline existed. Destroying that pipeline dropped exactly one
`shared_ptr<SceneModel3d>` per model, which is why releasing the pipeline first
made the Batch's delete succeed and releasing it last stranded the Model.

Two changes:

- `RenderPipeline` now releases the graph pass callbacks as soon as the frame's
  graph has executed, via a scope guard that covers every exit including
  throws. A scene model's lifetime is bounded by the frame that drew it rather
  than by the pipeline, and a pipeline holds no owning reference to a
  `SceneModel3d` between frames.
- `Batch::~Batch` keeps the reference-count guard -- deleting a resource
  another wrangler is still using would destroy something still in use -- but
  reports the skip instead of hiding it, naming the resource, its outstanding
  reference count, and each remaining holder via
  `describeOutstandingResourceReferences`. `ResourceManager::declareResource`
  names the same holders when a live name is redeclared, and a `SceneModel3d`
  is named after the model it wraps so the report identifies the holder rather
  than only its class.

A `SceneModel3d` must still not be destroyed while the frame that drew it is
executing; the supported order is to release the pipeline (and evict it from
`RenderSystem`) after the frame completes, and before the batches whose models
it rendered. The Batch report makes a broken order visible immediately.

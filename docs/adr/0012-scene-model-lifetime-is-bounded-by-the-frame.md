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
A pipeline therefore kept the last frame's `SceneModel3d` objects alive until
the next generated frame replaced the callbacks or the pipeline was destroyed.
Scene and shadow callbacks could each hold copies. Releasing the pipeline first
removed those owners, allowing the Batch's delete to succeed.

Two changes:

- `RenderPipeline` clears the frame-context pointer on scope exit, including
  failures during particle/trail simulation before graph execution. The same
  guard releases generated graph callbacks when the frame ends. It is installed
  before the XML/generated branch and callback registration. XML graphs retain
  their factory-created passes: `clearPassCallbacks()` also clears that pass
  cache, so callback cleanup is enabled only for generated graphs. Generated
  callbacks no longer retain `SceneModel3d` objects between frames.
- `Batch::~Batch` keeps the reference-count guard -- deleting a resource
  another wrangler is still using would destroy something still in use -- but
  reports the skip instead of hiding it, naming the resource, its outstanding
  reference count, and each remaining holder via
  `describeOutstandingResourceReferences`. `ResourceManager::declareResource`
  names the same holders when a live name is redeclared, and a `SceneModel3d`
  is named after the model it wraps so the report identifies the holder rather
  than only its class.

Finish rendering before tearing down scene objects. Remove the relevant
`SceneModel3d` objects from their Scene and release all application-held owners
before destroying the Batch that declared their Model. The pipeline may remain
alive and cached after the frame completes; destroying or evicting it first is
not required for Batch cleanup. An intentional external owner can still block
deletion, which is reported rather than forcibly deleted. There is no deferred
deletion: releasing that late owner does not automatically remove a stranded
resource's name registration.

# Issue #14: resource lifetime regressions

## Focused macOS checks

Run from a checkout with the repository's header dependencies available:

```sh
GLEW_INCLUDE_DIR=/path/to/glew-2.3.1/include bash tools/tests/resource-lifetime/run-macos.sh
```

The runner defaults to `ext/glew/include` if `GLEW_INCLUDE_DIR` is unset. It needs
Apple Clang with C++20 support (including `std::format`), uses temporary build
files, and does not require a graphics context. GLEW 2.3.1 headers were used for
the macOS validation. Export macros missing from the project's Apple setup are
supplied as compiler definitions; no production headers are rewritten.

- **Ownership:** production executor callback storage, SceneModel3d, Resource,
  and the private frame guard; 30 lifecycles covering normal exit, an exception,
  and partial callback registration failure. Each has scene and six shadow
  callback captures. Baseline mode deliberately omits the guard and succeeds
  only when it reproduces retention. Fixed mode requires immediate expiry while
  the executor lives. A persistent-mode check verifies callbacks are preserved
  when cleanup is disabled. It does not exercise factory-pass history.
- **Diagnostics:** the production `runResourceLifetimeTests` helper test runs
  in both ownership modes.
- **Batch:** actual Batch control flow and reference bookkeeping, with explicit
  ResourceManager/RenderSystem doubles and inert Model/material GPU methods.
  Covers never-created destruction, three partial creation failures, retained
  Model/Material diagnostics, and ordinary cleanup. Declaration failure is
  injected: this fixture does not test the actual manager's collision logic.
- **Instrumentation:** ownership and Batch executables use AddressSanitizer
  and UndefinedBehaviorSanitizer. The static logger is replaced by a no-op.
- **Compilation:** changed production translation units and GPU tests receive
  syntax checks with actual repository headers, without the Batch doubles.

These fixtures do not call `RenderPipeline::render()` or execute a GPU graph.
They do not establish graphics correctness or live GPU resource counts.

## Renderer integration checks

`runRenderGraphGpuTests`, invoked by `DemoSuite --particle-tests` after the
particle GPU suite, includes:

- Eight same-name creation/render/destruction cycles of two TriangleBatches
  through a cached generated legacy pipeline with GTAO and a named output.
- Immediate weak-pointer expiry before another render or pipeline eviction,
  Model/Material registry cleanup, and stable warmed resource counts.
- Duplicate live-name rejection and never-created Batch destruction.
- Off-camera point-shadow caster ownership and an injected scene-pass failure.
- The resource-lifetime diagnostic helper test.

Run these on a complete supported OpenGL build. Also run the downstream preview
for at least ten cycles with each pipeline/renderer teardown order, checking
flat resource counts, successful name reuse, and no GL errors.

## Limits

An application can legitimately retain a SceneModel3d after its Batch is gone.
The diagnostic reports that remaining Resource acquirer; it does not identify
which application object owns a `shared_ptr` to the SceneModel3d. No deferred
resource deletion is implemented. General mesh-queue recovery after rendering
exceptions is outside this fix; the injected rendering failure occurs before
mesh enqueueing.

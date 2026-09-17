# Serializer lifetime regression

`FileStream` stores factories as `std::function<Serializer*()>` and constructs a
`SerializerPtr` from their results. The derived type is already erased when
`shared_ptr` selects its deleter. Consequently, `Serializer` must have a public
virtual destructor; otherwise releasing the last owner has undefined behavior.
A `make_shared<Derived>()` test would miss this particular ownership bug.

The regression compiles the real `Serializer` and `StructuredData` sources. It
first checks the virtual-destructor contract so an unfixed revision fails
predictably without deliberately executing undefined behavior. It then checks
40 lifecycles covering normal return, load exceptions, and shared ownership in
both cases. Each verifies exactly one derived destructor and release of a
derived-owned payload. Checks remain enabled in Release builds.

## Standalone, no graphics or submodules

From the repository root, with CMake 3.22+ and a C++20 compiler:

```sh
cmake -S tools/tests/serializer-lifetime -B build-serializer-lifetime \
  -DCMAKE_BUILD_TYPE=Release
cmake --build build-serializer-lifetime --config Release --parallel 4
ctest --test-dir build-serializer-lifetime -C Release --output-on-failure
```

This route requires no macOS API/export shims. On Windows, use current MSVC
(VS2022); older MSVC still encounters the production source's pre-existing VLD
include. Windows execution has not been verified for this regression.

For GCC or Clang sanitizers:

```sh
cmake -S tools/tests/serializer-lifetime -B build-serializer-lifetime-sanitized \
  -DCMAKE_BUILD_TYPE=Debug -DMPP_SERIALIZER_TEST_SANITIZERS=ON
cmake --build build-serializer-lifetime-sanitized --config Debug --parallel 4
UBSAN_OPTIONS=halt_on_error=1 ctest --test-dir build-serializer-lifetime-sanitized \
  -C Debug --output-on-failure
```

The sanitizer option instruments only this executable and its two production
source files, not the renderer or the full application. ASan/UBSan do not by
themselves establish that every application resource is leak-free.

## Normal project build

The root project registers the same test with CTest when `BUILD_TESTING=ON`:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON
cmake --build build --config Release --target mpp_serializer_lifetime_tests
ctest --test-dir build -C Release -R '^mpp_serializer_lifetime$' --output-on-failure
```

This route still requires the normal project dependencies/platform support.
The standalone route is preferable for testing the lifetime contract alone.

After rebuilding the actual applications, also run these existing headless
integration suites from the repository root (adjust the binary directory):

```sh
./build/bin/Release/ParticleEditor --core-particle-tests
./build/bin/Release/ParticleEditor --document-tests
./build/bin/Release/ParticleEditor --resource-tests
./build/bin/Release/PipelineEditor --validate resources/shared/pbr/templates/Minimal.pipeline.yaml
```

The pipeline command runs its existing context-free suites before validating the
file, including render-graph XML/YAML resource parsing. These exercise the real
`FileStream` path, whereas the focused lifetime test isolates the ownership
contract. None of these commands creates a GPU context.

**Rebuild dependents:** adding a virtual destructor changes the serializer
vtable. Do not mix old serializer binaries or external subclasses with rebuilt
ones. This fixes deletion semantics; it does not constitute a full macOS port.

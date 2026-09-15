#!/usr/bin/env bash
set -euo pipefail

test_dir="$(cd -- "$(dirname -- "$0")" && pwd)"
repo_dir="$(cd -- "$test_dir/../../.." && pwd)"
if [[ "$(uname -s)" != Darwin ]]; then
    echo "This standalone runner uses the macOS linker. Run DemoSuite --particle-tests on supported renderer builds." >&2
    exit 1
fi
glew_include="${GLEW_INCLUDE_DIR:-$repo_dir/ext/glew/include}"
if [[ ! -f "$glew_include/GL/glew.h" ]]; then
    echo "Set GLEW_INCLUDE_DIR to a GLEW include directory containing GL/glew.h (validated with GLEW 2.3.1)." >&2
    exit 1
fi
output_dir="$(mktemp -d "${TMPDIR:-/tmp}/mpp-resource-lifetime.XXXXXX")"
trap 'rm -rf -- "$output_dir"' EXIT
compiler="${CXX:-clang++}"
flags=(-std=c++20 -D_MPPAPI= -D_MPPMESHAPI= -D_MPPPROGRAMAPI= -DUTILS_API=
    -DGLEW_NO_GLU -Wno-unknown-pragmas -Wno-expansion-to-defined
    -Wno-inconsistent-missing-override -Wno-unqualified-std-cast-call
    -I "$repo_dir/mpp/include" -I "$repo_dir/mpp/src"
    -I "$repo_dir/mpp-mesh/include" -I "$repo_dir/mpp-program/include"
    -I "$repo_dir/mpp-data/include" -I "$repo_dir/vendor/include"
    -I "$repo_dir/ext/utils/include" -I "$glew_include")
sanitizers=(-fsanitize=address,undefined -fno-omit-frame-pointer)
common=("$repo_dir/mpp/src/Resource.cpp" "$repo_dir/mpp/src/ResourceWrangler.cpp"
    "$repo_dir/mpp/src/SceneModel3d.cpp" "$repo_dir/mpp/src/MppException.cpp")

# Baseline intentionally omits the guard to prove the ownership fixture detects
# the original callback retention. Both modes use the current production classes.
for mode in baseline fixed; do
    mode_flags=(-UISSUE14_FIXED)
    if [[ "$mode" == fixed ]]; then mode_flags=(-DISSUE14_FIXED); fi
    "$compiler" "${flags[@]}" "${sanitizers[@]}" "${mode_flags[@]}" -Wl,-dead_strip \
        "$test_dir/ownership.cpp" "$repo_dir/mpp/src/RenderGraphExecutor.cpp" \
        "$repo_dir/mpp/src/ResourceLifetimeTests.cpp" "${common[@]}" \
        -o "$output_dir/ownership-$mode"
    "$output_dir/ownership-$mode"
done

# Only this fixture substitutes the resource manager and GPU backend. It runs
# actual Batch control flow, Resource accounting, and diagnostic formatting.
"$compiler" -I "$test_dir/batch-shims" "${flags[@]}" "${sanitizers[@]}" \
    -O2 -flto -Wl,-dead_strip "$test_dir/batch.cpp" "$repo_dir/mpp/src/Batch.cpp" \
    "${common[@]}" "$repo_dir/mpp/src/ResourceStream.cpp" \
    "$repo_dir/mpp/src/ModelStream.cpp" "$repo_dir/mpp/src/ProgrammaticModelStream.cpp" \
    "$repo_dir/mpp-mesh/src/MeshSpecification.cpp" "$repo_dir/mpp-mesh/src/VertexData.cpp" \
    "$repo_dir/mpp-mesh/src/MeshDefinition.cpp" "$repo_dir/mpp-mesh/src/VertexBufferDefinition.cpp" \
    "$repo_dir/mpp-mesh/src/VertexBufferAttributeLayout.cpp" "$repo_dir/mpp-mesh/src/Vertex.cpp" \
    -o "$output_dir/batch"
"$output_dir/batch"

# No test doubles are on the include path for production syntax checks.
"$compiler" "${flags[@]}" -fsyntax-only \
    "$repo_dir/mpp/src/Batch.cpp" "$repo_dir/mpp/src/QuadBatch.cpp" \
    "$repo_dir/mpp/src/Resource.cpp" "$repo_dir/mpp/src/ResourceManager.cpp" \
    "$repo_dir/mpp/src/SceneModel3d.cpp" "$repo_dir/mpp/src/ResourceLifetimeTests.cpp" \
    "$repo_dir/mpp/src/RenderPipeline.cpp" "$repo_dir/mpp/src/RenderGraphGpuTests.cpp"
echo "PASS production syntax checks (actual repository headers)"

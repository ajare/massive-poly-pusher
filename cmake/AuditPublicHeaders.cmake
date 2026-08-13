if(NOT DEFINED MPP_SOURCE_DIR)
    message(FATAL_ERROR "MPP_SOURCE_DIR is required")
endif()

set(public_roots
    mpp/include
    mpp-data/include
    mpp-helper/include
    mpp-mesh/include
    mpp-program/include
    mpp-resource-parsers/include
    mpp-mesh-specification-parser/include)
set(violations "")

foreach(root IN LISTS public_roots)
    file(GLOB_RECURSE headers
        "${MPP_SOURCE_DIR}/${root}/*.h"
        "${MPP_SOURCE_DIR}/${root}/*.hpp")
    foreach(header IN LISTS headers)
        # These are explicitly opt-in integration headers, not part of the
        # normal public include graph.
        get_filename_component(name "${header}" NAME)
        if(name STREQUAL "backward.hpp" OR name STREQUAL "VertexHalfTypeSpecification.h")
            continue()
        endif()
        file(STRINGS "${header}" forbidden_includes REGEX
            "^[ \\t]*#[ \\t]*include[ \\t]*[<\"](GL/|utils/|half/|Windows\\.h|windows\\.h|SDL|assimp/|imgui)")
        if(forbidden_includes)
            list(APPEND violations "${header}: ${forbidden_includes}")
        endif()
    endforeach()
endforeach()

if(violations)
    list(JOIN violations "\n" message_text)
    message(FATAL_ERROR "Public headers expose implementation dependencies:\n${message_text}")
endif()

message(STATUS "MPP public-header dependency audit passed")

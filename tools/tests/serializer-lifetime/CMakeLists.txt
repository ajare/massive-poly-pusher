cmake_minimum_required(VERSION 3.22)

# Also buildable independently: no renderer, submodules, or graphics context.
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(MppSerializerLifetimeTests LANGUAGES CXX)
    enable_testing()
endif()

get_filename_component(MPP_SERIALIZER_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
add_executable(mpp_serializer_lifetime_tests
    serializer_lifetime_tests.cpp
    "${MPP_SERIALIZER_ROOT}/mpp-resource-parsers/src/Serializer.cpp"
    "${MPP_SERIALIZER_ROOT}/mpp-data/src/StructuredData.cpp")
target_include_directories(mpp_serializer_lifetime_tests PRIVATE
    "${MPP_SERIALIZER_ROOT}/mpp-resource-parsers/include"
    "${MPP_SERIALIZER_ROOT}/mpp-data/include")
target_compile_features(mpp_serializer_lifetime_tests PRIVATE cxx_std_20)
target_compile_definitions(mpp_serializer_lifetime_tests PRIVATE MPP_DATA_STATIC_LIB)

option(MPP_SERIALIZER_TEST_SANITIZERS "Instrument serializer lifetime tests with ASan/UBSan" OFF)
if(MPP_SERIALIZER_TEST_SANITIZERS)
    if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        message(FATAL_ERROR "Serializer test sanitizers require Clang or GCC")
    endif()
    target_compile_options(mpp_serializer_lifetime_tests PRIVATE
        -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(mpp_serializer_lifetime_tests PRIVATE -fsanitize=address,undefined)
endif()

add_test(NAME mpp_serializer_lifetime COMMAND mpp_serializer_lifetime_tests)
set_tests_properties(mpp_serializer_lifetime PROPERTIES TIMEOUT 30)

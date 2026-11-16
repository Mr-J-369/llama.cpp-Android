include(CheckCXXCompilerFlag)
include("../cmake/common.cmake")

add_compile_definitions(GGML_SCHED_MAX_COPIES=${GGML_SCHED_MAX_COPIES})

if (GGML_SANITIZE_THREAD)
    add_compile_options(-fsanitize=thread)
    link_libraries     (-fsanitize=thread)
endif()

if (GGML_SANITIZE_ADDRESS)
    add_compile_options(-fsanitize=address -fno-omit-frame-pointer)
    link_libraries     (-fsanitize=address)
endif()

if (GGML_SANITIZE_UNDEFINED)
    add_compile_options(-fsanitize=undefined)
    link_libraries     (-fsanitize=undefined)
endif()

if (GGML_FATAL_WARNINGS)
    list(APPEND C_FLAGS   -Werror)
    list(APPEND CXX_FLAGS -Werror)
endif()

if (GGML_ALL_WARNINGS)
    list(APPEND WARNING_FLAGS -Wall -Wextra -Wpedantic -Wcast-qual -Wno-unused-function)
    list(APPEND C_FLAGS       -Wshadow -Wstrict-prototypes -Wpointer-arith -Wmissing-prototypes
                              -Werror=implicit-int -Werror=implicit-function-declaration)
    list(APPEND CXX_FLAGS     -Wmissing-declarations -Wmissing-noreturn)

    list(APPEND C_FLAGS   ${WARNING_FLAGS})
    list(APPEND CXX_FLAGS ${WARNING_FLAGS})

    ggml_get_flags(${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION})

    add_compile_options("$<$<COMPILE_LANGUAGE:C>:${C_FLAGS};${GF_C_FLAGS}>"
                        "$<$<COMPILE_LANGUAGE:CXX>:${CXX_FLAGS};${GF_CXX_FLAGS}>")
endif()

if (GGML_LTO)
    include(CheckIPOSupported)
    check_ipo_supported(RESULT result OUTPUT output)
    if (result)
        set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
    else()
        message(WARNING "IPO is not supported: ${output}")
    endif()
endif()

if (GGML_CCACHE AND NOT CMAKE_C_COMPILER_LAUNCHER AND NOT CMAKE_CXX_COMPILER_LAUNCHER)
    find_program(GGML_CCACHE_FOUND ccache)
    find_program(GGML_SCCACHE_FOUND sccache)

    if (GGML_CCACHE_FOUND OR GGML_SCCACHE_FOUND)
        if(GGML_CCACHE_FOUND)
            set(GGML_CCACHE_VARIANT ccache)
        else()
            set(GGML_CCACHE_VARIANT sccache)
        endif()
        set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE "${GGML_CCACHE_VARIANT}")
        set(ENV{CCACHE_SLOPPINESS} time_macros)
        message(STATUS "${GGML_CCACHE_VARIANT} found, compilation results will be cached. Disable with GGML_CCACHE=OFF.")
    else()
        message(STATUS "Warning: ccache not found - consider installing it for faster compilation or disable this warning with GGML_CCACHE=OFF")
    endif ()
endif()

message(STATUS "CMAKE_SYSTEM_PROCESSOR: ${CMAKE_SYSTEM_PROCESSOR}")
ggml_get_system_arch()
message(STATUS "GGML_SYSTEM_ARCH: ${GGML_SYSTEM_ARCH}")

if (GGML_STATIC)
    set(CMAKE_FIND_LIBRARY_SUFFIXES ".a;.so")
    add_link_options(-static)
endif()
if (GGML_GPROF)
    add_compile_options(-pg)
endif()

# POSIX conformance & GNU extensions
add_compile_definitions(_XOPEN_SOURCE=600)
add_compile_definitions(_GNU_SOURCE)

# ggml

if (GGML_BACKEND_DL AND NOT BUILD_SHARED_LIBS)
    message(FATAL_ERROR "GGML_BACKEND_DL requires BUILD_SHARED_LIBS")
endif()

add_library(ggml-base
            ../include/ggml.h
            ../include/ggml-alloc.h
            ../include/ggml-backend.h
            ../include/ggml-cpp.h
            ../include/ggml-opt.h
            ../include/gguf.h
            ggml.c
            ggml.cpp
            ggml-alloc.c
            ggml-backend.cpp
            ggml-backend-meta.cpp
            ggml-opt.cpp
            ggml-threading.cpp
            ggml-threading.h
            ggml-quants.c
            ggml-quants.h
            gguf.cpp)

set_target_properties(ggml-base PROPERTIES
    VERSION ${GGML_VERSION}
    SOVERSION ${GGML_VERSION_MAJOR}
)

configure_file(ggml-version.h.in ${CMAKE_CURRENT_BINARY_DIR}/ggml-version.h @ONLY)

target_include_directories(ggml-base PRIVATE . ${CMAKE_CURRENT_BINARY_DIR})

if (GGML_BACKEND_DL)
    target_compile_definitions(ggml-base PUBLIC GGML_BACKEND_DL)
endif()

if (GGML_SCHED_NO_REALLOC)
    target_compile_definitions(ggml-base PUBLIC GGML_SCHED_NO_REALLOC)
endif()

if (GGML_OPENMP)
    find_package(OpenMP)
    if (OpenMP_FOUND)
        set(GGML_OPENMP_TARGET_C OpenMP::OpenMP_C)
        set(GGML_OPENMP_TARGET_CXX OpenMP::OpenMP_CXX)
        set(GGML_OPENMP_ENABLED "ON" CACHE INTERNAL "")
    else()
        set(GGML_OPENMP_ENABLED "OFF" CACHE INTERNAL "")
        message(WARNING "OpenMP not found")
    endif()
else()
    set(GGML_OPENMP_ENABLED "OFF" CACHE INTERNAL "")
endif()

if (GGML_OPENMP_ENABLED)
    target_compile_definitions(ggml-base PRIVATE GGML_USE_OPENMP)
    target_link_libraries(ggml-base PRIVATE ${GGML_OPENMP_TARGET_C} ${GGML_OPENMP_TARGET_CXX})
endif()

add_library(ggml
            ggml-backend-dl.cpp
            ggml-backend-reg.cpp)
add_library(ggml::ggml ALIAS ggml)

set_target_properties(ggml PROPERTIES
    VERSION ${GGML_VERSION}
    SOVERSION ${GGML_VERSION_MAJOR}
)

if (GGML_BACKEND_DIR)
    if (NOT GGML_BACKEND_DL)
        message(FATAL_ERROR "GGML_BACKEND_DIR requires GGML_BACKEND_DL")
    endif()
    target_compile_definitions(ggml PUBLIC GGML_BACKEND_DIR="${GGML_BACKEND_DIR}")
endif()

target_link_libraries(ggml PUBLIC ggml-base)
target_link_libraries(ggml PRIVATE dl)

function(ggml_add_backend_library backend)
    if (GGML_BACKEND_DL)
        add_library(${backend} MODULE ${ARGN})
        set_target_properties(${backend} PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
        target_compile_definitions(${backend} PRIVATE GGML_BACKEND_DL)
        add_dependencies(ggml ${backend})
        if (GGML_BACKEND_DIR)
            install(TARGETS ${backend} LIBRARY DESTINATION ${GGML_BACKEND_DIR})
        else()
            install(TARGETS ${backend} LIBRARY DESTINATION ${CMAKE_INSTALL_BINDIR})
        endif()
    else()
        add_library(${backend} ${ARGN})
        target_link_libraries(ggml PUBLIC ${backend})
        install(TARGETS ${backend} LIBRARY)
    endif()

    target_link_libraries(${backend} PRIVATE ggml-base)
    target_include_directories(${backend} PRIVATE ..)

    if (${BUILD_SHARED_LIBS})
        target_compile_definitions(${backend} PRIVATE GGML_BACKEND_BUILD)
        target_compile_definitions(${backend} PUBLIC  GGML_BACKEND_SHARED)
    endif()

    set_target_properties(${backend} PROPERTIES
        VERSION ${GGML_VERSION}
        SOVERSION ${GGML_VERSION_MAJOR}
    )

    if(NOT GGML_AVAILABLE_BACKENDS)
        set(GGML_AVAILABLE_BACKENDS "${backend}"
            CACHE INTERNAL "List of backends for cmake package")
    else()
        list(FIND GGML_AVAILABLE_BACKENDS "${backend}" has_backend)
        if(has_backend EQUAL -1)
            set(GGML_AVAILABLE_BACKENDS "${GGML_AVAILABLE_BACKENDS};${backend}"
                CACHE INTERNAL "List of backends for cmake package")
        endif()
    endif()
endfunction()

function(ggml_add_backend backend)
    string(TOUPPER "GGML_${backend}" backend_id)
    if (${backend_id})
        string(TOLOWER "ggml-${backend}" backend_target)
        add_subdirectory(${backend_target})
        message(STATUS "Including ${backend} backend")
        if (NOT GGML_BACKEND_DL)
            string(TOUPPER "GGML_USE_${backend}" backend_use)
            target_compile_definitions(ggml PUBLIC ${backend_use})
        endif()
    endif()
endfunction()

function(ggml_add_cpu_backend_variant tag_name)
    set(GGML_CPU_TAG_NAME ${tag_name})
    if (GGML_SYSTEM_ARCH STREQUAL "ARM")
        foreach (feat ${ARGN})
            set(GGML_INTERNAL_${feat} ON)
        endforeach()
    endif()

    ggml_add_cpu_backend_variant_impl(${tag_name})
endfunction()

ggml_add_backend(CPU)

if (GGML_CPU_ALL_VARIANTS)
    if (NOT GGML_BACKEND_DL)
        message(FATAL_ERROR "GGML_CPU_ALL_VARIANTS requires GGML_BACKEND_DL")
    elseif (GGML_CPU_ARM_ARCH)
        message(FATAL_ERROR "Cannot use both GGML_CPU_ARM_ARCH and GGML_CPU_ALL_VARIANTS")
    endif()
    if (GGML_SYSTEM_ARCH STREQUAL "ARM")
        # Android-specific backends with SoC-compatible feature sets
        ggml_add_cpu_backend_variant(android_armv8.0_1)
        ggml_add_cpu_backend_variant(android_armv8.2_1    DOTPROD)
        ggml_add_cpu_backend_variant(android_armv8.2_2    DOTPROD FP16_VECTOR_ARITHMETIC)
        ggml_add_cpu_backend_variant(android_armv8.6_1    DOTPROD FP16_VECTOR_ARITHMETIC MATMUL_INT8)
        ggml_add_cpu_backend_variant(android_armv9.0_1    DOTPROD MATMUL_INT8 FP16_VECTOR_ARITHMETIC SVE2)
        ggml_add_cpu_backend_variant(android_armv9.2_1    DOTPROD MATMUL_INT8 FP16_VECTOR_ARITHMETIC SVE SME)
        ggml_add_cpu_backend_variant(android_armv9.2_2    DOTPROD MATMUL_INT8 FP16_VECTOR_ARITHMETIC SVE SVE2 SME)
    else()
        message(FATAL_ERROR "GGML_CPU_ALL_VARIANTS not supported with ${GGML_SYSTEM_ARCH} on ${CMAKE_SYSTEM_NAME}")
    endif()
elseif (GGML_CPU)
    ggml_add_cpu_backend_variant_impl("")
endif()

ggml_add_backend(OpenCL)
ggml_add_backend(Hexagon)

foreach (target ggml-base ggml)
    target_include_directories(${target} PUBLIC    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../include> $<INSTALL_INTERFACE:include>)
    target_compile_features   (${target} PRIVATE c_std_11 cxx_std_17) # don't bump
endforeach()

target_link_libraries(ggml-base PRIVATE Threads::Threads)

if (DEFINED MATH_LIBRARY)
    target_link_libraries(ggml-base PRIVATE ${MATH_LIBRARY})
else()
    target_link_libraries(ggml-base PRIVATE m)
endif()

target_link_libraries(ggml-base PRIVATE dl)

if (BUILD_SHARED_LIBS)
    foreach (target ggml-base ggml)
        set_target_properties(${target} PROPERTIES
            POSITION_INDEPENDENT_CODE ON
            C_VISIBILITY_PRESET hidden
            CXX_VISIBILITY_PRESET hidden
        )
        target_compile_definitions(${target} PRIVATE GGML_BUILD)
        target_compile_definitions(${target} PUBLIC  GGML_SHARED)
    endforeach()
endif()

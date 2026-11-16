function(ggml_add_cpu_backend_features cpu_name arch)
    # The feature detection code is compiled as a separate target so that
    # it can be built without the architecture flags
    set(GGML_CPU_FEATS_NAME ${cpu_name}-feats)
    add_library(${GGML_CPU_FEATS_NAME} OBJECT ggml-cpu/arch/${arch}/cpu-feats.cpp)
    target_include_directories(${GGML_CPU_FEATS_NAME} PRIVATE . ../include)
    target_compile_definitions(${GGML_CPU_FEATS_NAME} PRIVATE ${ARGN})
    target_compile_definitions(${GGML_CPU_FEATS_NAME} PRIVATE GGML_BACKEND_DL GGML_BACKEND_BUILD GGML_BACKEND_SHARED)
    set_target_properties(${GGML_CPU_FEATS_NAME} PROPERTIES POSITION_INDEPENDENT_CODE ON)
    target_compile_options(${GGML_CPU_FEATS_NAME} PRIVATE -fno-lto)
    target_link_libraries(${cpu_name} PRIVATE ${GGML_CPU_FEATS_NAME})
endfunction()

function(ggml_add_cpu_backend_variant_impl tag_name)
    if (tag_name)
        set(GGML_CPU_NAME ggml-cpu-${tag_name})
    else()
        set(GGML_CPU_NAME ggml-cpu)
    endif()

    ggml_add_backend_library(${GGML_CPU_NAME})

    list (APPEND GGML_CPU_SOURCES
        ggml-cpu/ggml-cpu.c
        ggml-cpu/ggml-cpu.cpp
        ggml-cpu/repack.cpp
        ggml-cpu/repack.h
        ggml-cpu/hbm.cpp
        ggml-cpu/hbm.h
        ggml-cpu/quants.c
        ggml-cpu/quants.h
        ggml-cpu/traits.cpp
        ggml-cpu/traits.h
        ggml-cpu/tiled/tiled.cpp
        ggml-cpu/tiled/tiled.h
        ggml-cpu/tiled/tiled-kernel.cpp
        ggml-cpu/tiled/tiled-kernel.h
        ggml-cpu/ggml-cpu-impl.h
        ggml-cpu/common.h
        ggml-cpu/binary-ops.h
        ggml-cpu/binary-ops.cpp
        ggml-cpu/unary-ops.h
        ggml-cpu/unary-ops.cpp
        ggml-cpu/simd-mappings.h
        ggml-cpu/vec.h
        ggml-cpu/vec.cpp
        ggml-cpu/ops.h
        ggml-cpu/ops.cpp
        )

    target_compile_features(${GGML_CPU_NAME} PRIVATE c_std_11 cxx_std_17)
    target_include_directories(${GGML_CPU_NAME} PRIVATE . ggml-cpu)

    if (GGML_OPENMP_ENABLED)
        target_compile_definitions(${GGML_CPU_NAME} PRIVATE GGML_USE_OPENMP)
        target_link_libraries(${GGML_CPU_NAME} PRIVATE ${GGML_OPENMP_TARGET_C} ${GGML_OPENMP_TARGET_CXX})
    endif()

    if (GGML_CPU_HBM)
        find_library(memkind memkind REQUIRED)
        message(STATUS "Using memkind for CPU HBM")
        target_compile_definitions(${GGML_CPU_NAME} PRIVATE GGML_USE_CPU_HBM)
        target_link_libraries(${GGML_CPU_NAME} PUBLIC memkind)
    endif()

    if (GGML_SYSTEM_ARCH STREQUAL "ARM")
        message(STATUS "ARM detected")
        list(APPEND GGML_CPU_SOURCES
            ggml-cpu/arch/arm/quants.c
            ggml-cpu/arch/arm/repack.cpp
            )

        check_cxx_compiler_flag(-mfp16-format=ieee GGML_COMPILER_SUPPORTS_FP16_FORMAT_I3E)
        if (NOT "${GGML_COMPILER_SUPPORTS_FP16_FORMAT_I3E}" STREQUAL "")
            list(APPEND ARCH_FLAGS -mfp16-format=ieee)
        endif()

        if (GGML_CPU_ARM_ARCH)
            list(APPEND ARCH_FLAGS -march=${GGML_CPU_ARM_ARCH})
        elseif(GGML_CPU_ALL_VARIANTS)
            set(ARM_MCPU "armv8-a")
            set(ARCH_TAGS "")
            set(ARCH_DEFINITIONS "")

            if (GGML_INTERNAL_DOTPROD)
                set(ARM_MCPU "armv8.2-a")
                set(ARCH_TAGS "${ARCH_TAGS}+dotprod")
                list(APPEND ARCH_DEFINITIONS GGML_USE_DOTPROD)
            endif()
            if (GGML_INTERNAL_FP16_VECTOR_ARITHMETIC)
                set(ARM_MCPU "armv8.2-a")
                set(ARCH_TAGS "${ARCH_TAGS}+fp16")
                list(APPEND ARCH_DEFINITIONS GGML_USE_FP16_VECTOR_ARITHMETIC)
            endif()
            if (GGML_INTERNAL_SVE)
                set(ARM_MCPU "armv8.2-a")
                set(ARCH_TAGS "${ARCH_TAGS}+sve")
                list(APPEND ARCH_DEFINITIONS GGML_USE_SVE)
            endif()
            if (GGML_INTERNAL_MATMUL_INT8)
                set(ARM_MCPU "armv8.6-a")
                set(ARCH_TAGS "${ARCH_TAGS}+i8mm")
                list(APPEND ARCH_DEFINITIONS GGML_USE_MATMUL_INT8)
            endif()
            if (GGML_INTERNAL_SVE2)
                set(ARM_MCPU "armv8.6-a")
                set(ARCH_TAGS "${ARCH_TAGS}+sve2")
                list(APPEND ARCH_DEFINITIONS GGML_USE_SVE2)
            endif()
            if (GGML_INTERNAL_NOSVE)
                set(ARCH_TAGS "${ARCH_TAGS}+nosve")
            endif()
            if (GGML_INTERNAL_SME)
                set(ARM_MCPU "armv9.2-a")
                set(ARCH_TAGS "${ARCH_TAGS}+sme")
                list(APPEND ARCH_DEFINITIONS GGML_USE_SME)
            endif()
            list(APPEND ARCH_FLAGS "-march=${ARM_MCPU}${ARCH_TAGS}")
            ggml_add_cpu_backend_features(${GGML_CPU_NAME} arm ${ARCH_DEFINITIONS})
        endif()

        message(STATUS "Checking for ARM features using flags:")
        foreach(flag IN LISTS ARCH_FLAGS)
            message(STATUS "  ${flag}")
        endforeach()

        include(CheckCXXSourceCompiles)
        set(CMAKE_REQUIRED_FLAGS_SAVE ${CMAKE_REQUIRED_FLAGS})
        string(REPLACE ";" " " ARCH_FLAGS_STR "${ARCH_FLAGS}")
        set(CMAKE_REQUIRED_FLAGS "${ARCH_FLAGS_STR}")
        foreach(feature DOTPROD SVE MATMUL_INT8 FMA FP16_VECTOR_ARITHMETIC SME)
            set(ARM_FEATURE "HAVE_${feature}")
            check_cxx_source_compiles(
                "
                #if !defined(__ARM_FEATURE_${feature})
                #  error \"Feature ${feature} is not defined\"
                #endif
                int main() { return 0; }
                "
                ${ARM_FEATURE}
            )
        endforeach()
        set(CMAKE_REQUIRED_FLAGS ${CMAKE_REQUIRED_FLAGS_SAVE})
    else()
        message(WARNING "Unknown CPU architecture. Falling back to generic implementations.")
        list(APPEND ARCH_FLAGS -DGGML_CPU_GENERIC)
    endif()

    if (GGML_CPU_REPACK)
        target_compile_definitions(${GGML_CPU_NAME} PRIVATE GGML_USE_CPU_REPACK)
    endif()

    if (GGML_CPU_KLEIDIAI)
        if (CMAKE_VERSION VERSION_LESS 3.16)
            message(FATAL_ERROR "GGML_CPU_KLEIDIAI requires CMake >= 3.16")
        endif()

        set(GGML_CPU_KLEIDIAI_AARCH64 OFF)
        if (GGML_SYSTEM_ARCH STREQUAL "ARM" AND
            CMAKE_SYSTEM_NAME STREQUAL "Android" AND
            (CMAKE_SYSTEM_PROCESSOR MATCHES "^(aarch64|arm64|ARM64|arm64-v8a)$" OR
             CMAKE_ANDROID_ARCH_ABI STREQUAL "arm64-v8a"))
            set(GGML_CPU_KLEIDIAI_AARCH64 ON)
        endif()
        if (NOT GGML_CPU_KLEIDIAI_AARCH64)
            message(FATAL_ERROR "GGML_CPU_KLEIDIAI requires an Android AArch64/arm64-v8a target")
        endif()

        message(STATUS "Using KleidiAI optimized kernels if applicable")

        include(FetchContent)
        set(KLEIDIAI_COMMIT_TAG "v1.32.0")
        set(KLEIDIAI_DOWNLOAD_URL "https://github.com/ARM-software/kleidiai/releases/download/${KLEIDIAI_COMMIT_TAG}/kleidiai-${KLEIDIAI_COMMIT_TAG}-src.tar.gz")
        set(KLEIDIAI_RELEASE_ARCHIVE_MD5  "05f1953428ffe8e8c7f585dc6d508887")

        set(KLEIDIAI_FETCH_ARGS
            URL ${KLEIDIAI_DOWNLOAD_URL}
            URL_HASH MD5=${KLEIDIAI_RELEASE_ARCHIVE_MD5}
        )
        if (CMAKE_VERSION VERSION_GREATER_EQUAL "3.24")
            list(APPEND KLEIDIAI_FETCH_ARGS DOWNLOAD_EXTRACT_TIMESTAMP NEW)
        endif()

        FetchContent_Declare(kleidiai
            ${KLEIDIAI_FETCH_ARGS}
        )

        set(KLEIDIAI_BUILD_TESTS OFF CACHE BOOL "" FORCE)
        set(KLEIDIAI_BUILD_BENCHMARK OFF CACHE BOOL "" FORCE)

        FetchContent_GetProperties(kleidiai
            SOURCE_DIR KLEIDIAI_SRC
            BINARY_DIR KLEIDIAI_BIN
            POPULATED KLEIDIAI_POPULATED
        )
        if (NOT KLEIDIAI_POPULATED)
            FetchContent_Populate(kleidiai)
            FetchContent_GetProperties(kleidiai
                SOURCE_DIR KLEIDIAI_SRC
                BINARY_DIR KLEIDIAI_BIN
            )
        endif()

        if (NOT TARGET kleidiai)
            add_subdirectory(
                "${CMAKE_CURRENT_SOURCE_DIR}/ggml-cpu/kleidiai"
                "${CMAKE_CURRENT_BINARY_DIR}/kleidiai-wrapper"
                EXCLUDE_FROM_ALL
            )
            if (NOT CMAKE_SKIP_INSTALL_RULES AND
                (NOT DEFINED BUILD_SHARED_LIBS OR NOT BUILD_SHARED_LIBS))
                install(TARGETS kleidiai ARCHIVE)
            endif()
        endif()

        if (NOT TARGET kleidiai)
            message(FATAL_ERROR "KleidiAI target was not created")
        endif()

        set_target_properties(kleidiai PROPERTIES POSITION_INDEPENDENT_CODE ON)

        target_link_libraries(${GGML_CPU_NAME} PRIVATE kleidiai)

        target_compile_definitions(${GGML_CPU_NAME} PRIVATE GGML_USE_CPU_KLEIDIAI)

        list(APPEND GGML_CPU_SOURCES
            ggml-cpu/kleidiai/kleidiai.cpp
            ggml-cpu/kleidiai/kernels.cpp
            ggml-cpu/kleidiai/kleidiai.h
            ggml-cpu/kleidiai/kernels.h
            )
    endif()

    message(STATUS "Adding CPU backend variant ${GGML_CPU_NAME}: ${ARCH_FLAGS} ${ARCH_DEFINITIONS}")
    target_sources(${GGML_CPU_NAME} PRIVATE ${GGML_CPU_SOURCES})
    target_compile_options(${GGML_CPU_NAME} PRIVATE ${ARCH_FLAGS})
    target_compile_definitions(${GGML_CPU_NAME} PRIVATE ${ARCH_DEFINITIONS})

endfunction()

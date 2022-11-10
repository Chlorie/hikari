add_library(hikari)
add_library(hikari::hikari ALIAS hikari)

set_target_properties(hikari PROPERTIES LINKER_LANGUAGE CXX)
target_set_output_dirs(hikari)

function (add_headers TGT)
    set(headers ${ARGN})
    list(TRANSFORM headers PREPEND "include/hikari/")
    set(${TGT} ${headers} PARENT_SCOPE)
endfunction ()
function (add_sources TGT)
    set(sources ${ARGN})
    list(TRANSFORM sources PREPEND "src/")
    set(${TGT} ${sources} PARENT_SCOPE)
endfunction ()

add_headers(HEADERS
    # Header files here (relative to ./include/hikari/)
    "export.h"
    "api.h"
    "types.h"
)
add_sources(SOURCES
    # Source files here (relative to ./src/)
    "lilypond_converter.h"
    "lilypond_converter.cpp"
    "measurifier.h"
    "measurifier.cpp"
    "parser.h"
    "parser.cpp"
    "parser_types.h"
    "parser_types.cpp"
    "preprocessor.h"
    "preprocessor.cpp"
    "tie_processor.h"
    "tie_processor.cpp"
    "types.cpp"
)

find_package(clu CONFIG REQUIRED)
find_package(fmt CONFIG REQUIRED)

target_sources(hikari PRIVATE ${HEADERS} ${SOURCES})
target_set_options(hikari PRIVATE)
target_set_warnings(hikari PRIVATE)
target_set_cxx_std(hikari)

if (BUILD_SHARED_LIBS)
    target_compile_definitions(hikari
        PRIVATE HIKARI_EXPORT_SHARED
        PUBLIC HIKARI_BUILD_SHARED)
endif ()

target_include_directories(hikari
PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
target_link_libraries(hikari
PUBLIC
    clu::clu
PRIVATE
    fmt::fmt
)

include(CMakePackageConfigHelpers)
write_basic_package_version_file(
    "${PROJECT_BINARY_DIR}/hikariConfigVersion.cmake"
    VERSION ${PACKAGE_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(TARGETS hikari
    EXPORT hikariTargets
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
    PUBLIC_HEADER DESTINATION include
)

include(CMakePackageConfigHelpers)
configure_package_config_file(
    "${PROJECT_SOURCE_DIR}/cmake/hikariConfig.cmake.in"
    "${PROJECT_BINARY_DIR}/hikariConfig.cmake"
    INSTALL_DESTINATION lib/cmake/hikari
)

export(TARGETS hikari NAMESPACE hikari:: FILE hikariConfig.cmake)
install(EXPORT hikariTargets NAMESPACE hikari:: DESTINATION lib/cmake/hikari)
install(FILES "${PROJECT_BINARY_DIR}/hikariConfigVersion.cmake"
                "${PROJECT_BINARY_DIR}/hikariConfig.cmake"
        DESTINATION lib/cmake/hikari)
install(DIRECTORY ${PROJECT_SOURCE_DIR}/lib/include/ DESTINATION include)

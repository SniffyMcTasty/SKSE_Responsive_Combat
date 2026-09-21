cmake_minimum_required(VERSION 3.21)
project(ResponsiveCombat VERSION 0.0.2 LANGUAGES CXX)

find_package(CommonLibSSE CONFIG REQUIRED)
find_package(unofficial-inih CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)

add_library(ResponsiveCombatSupport STATIC src/Config.cpp src/Logging.cpp src/ObservationTrace.cpp)
target_compile_features(ResponsiveCombatSupport PUBLIC cxx_std_23)
target_include_directories(ResponsiveCombatSupport PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(ResponsiveCombatSupport PUBLIC spdlog::spdlog PRIVATE unofficial::inih::inih)

add_commonlibsse_plugin(${PROJECT_NAME} SOURCES plugin.cpp src/CombatObserver.cpp)
target_link_libraries(${PROJECT_NAME} PRIVATE ResponsiveCombatSupport)
target_compile_definitions(${PROJECT_NAME} PRIVATE RESPONSIVE_COMBAT_VERSION="${PROJECT_VERSION}")
target_precompile_headers(${PROJECT_NAME} PRIVATE PCH.h)

include(CTest)
if(BUILD_TESTING)
    add_executable(ResponsiveCombatTests tests/SupportTests.cpp)
    target_link_libraries(ResponsiveCombatTests PRIVATE ResponsiveCombatSupport)
    foreach(case IN ITEMS defaults valid levels invalid-values schema syntax bounds unknown files logging-directory logging logging-fallback logging-rotation diagnostic-config)
        add_test(NAME ${case} COMMAND ResponsiveCombatTests ${case})
    endforeach()
    add_executable(ObservationTests tests/ObservationTests.cpp)
    target_link_libraries(ObservationTests PRIVATE ResponsiveCombatSupport)
    foreach(case IN ITEMS lifecycle budget channels timestamps text states concurrent)
        add_test(NAME observation-${case} COMMAND ObservationTests ${case})
    endforeach()
    add_test(NAME example-config COMMAND ResponsiveCombatTests example "${CMAKE_CURRENT_SOURCE_DIR}/config/ResponsiveCombat.ini")
    add_test(NAME deployment-preserves-config COMMAND "${CMAKE_COMMAND}"
        "-DSOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}" "-DTEST_DIR=${CMAKE_CURRENT_BINARY_DIR}/deployment-test"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/DeploymentTest.cmake")
endif()

# Deployment is explicit and is invoked by build.ps1 only after tests pass.
if(DEFINED ENV{SKYRIM_FOLDER} AND IS_DIRECTORY "$ENV{SKYRIM_FOLDER}/Data")
    set(OUTPUT_FOLDER "$ENV{SKYRIM_FOLDER}/Data")
endif()
if(DEFINED ENV{SKYRIM_MODS_FOLDER} AND IS_DIRECTORY "$ENV{SKYRIM_MODS_FOLDER}")
    set(OUTPUT_FOLDER "$ENV{SKYRIM_MODS_FOLDER}/${PROJECT_NAME}")
endif()
if(DEFINED OUTPUT_FOLDER)
    set(DLL_FOLDER "${OUTPUT_FOLDER}/SKSE/Plugins")
    message(STATUS "SKSE plugin output folder: ${DLL_FOLDER}")
    add_custom_target(deploy
        COMMAND "${CMAKE_COMMAND}"
            "-DDLL=$<TARGET_FILE:${PROJECT_NAME}>"
            "-DPDB=$<$<CONFIG:Debug>:$<TARGET_PDB_FILE:${PROJECT_NAME}>>"
            "-DCONFIG=${CMAKE_CURRENT_SOURCE_DIR}/config/ResponsiveCombat.ini"
            "-DDESTINATION=${DLL_FOLDER}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/Deploy.cmake"
        DEPENDS ${PROJECT_NAME}
        VERBATIM
    )
endif()

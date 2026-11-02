cmake_minimum_required(VERSION 3.24)
project(tamapoke CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
enable_testing()

if (MSVC)
  add_compile_options(/utf-8 /Zc:__cplusplus /permissive- /W3)
  add_compile_definitions(NOMINMAX WIN32_LEAN_AND_MEAN _CRT_SECURE_NO_WARNINGS)
endif()

include(FetchContent)
FetchContent_Declare(SDL3
  GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
  GIT_TAG release-3.2.10
  GIT_SHALLOW TRUE)
set(SDL_SHARED ON CACHE BOOL "" FORCE)
set(SDL_STATIC OFF CACHE BOOL "" FORCE)
set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(SDL3)

# nlohmann/json v3.11.3 declara cmake_minimum_required < 3.5, rejeitado pelo CMake 4.x.
# Politica escopada so a esta dependencia (nao vaza para o SDL3 acima).
FetchContent_Declare(nlohmann_json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG v3.11.3
  GIT_SHALLOW TRUE)
set(JSON_BuildTests OFF CACHE INTERNAL "")
set(JSON_Install OFF CACHE INTERNAL "")
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
FetchContent_MakeAvailable(nlohmann_json)
unset(CMAKE_POLICY_VERSION_MINIMUM)

add_executable(tamapoke WIN32 src/main.cpp)
target_link_libraries(tamapoke PRIVATE SDL3::SDL3)

add_custom_command(TARGET tamapoke POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:SDL3::SDL3> $<TARGET_FILE_DIR:tamapoke>)

# Suite de testes do upstream (logica pura, sem SDL). Fontes espelham upstream/test/Makefile.
add_executable(upstream_tests
  upstream/test/main.cpp
  upstream/test/shim/shim.cpp
  upstream/test/test_pet.cpp
  upstream/test/test_dex.cpp
  upstream/test/test_i18n.cpp
  upstream/test/test_blit.cpp
  upstream/test/test_batcurve.cpp
  upstream/test/test_power.cpp
  upstream/pet.cpp
  upstream/i18n.cpp)
target_include_directories(upstream_tests PRIVATE upstream/test/shim upstream)
add_test(NAME upstream COMMAND upstream_tests)

# Testes contra a nossa HAL (nunca ve o shim do upstream; upstream/test fora do include path).
file(GLOB HAL_SRC CONFIGURE_DEPENDS hal/*.cpp)
file(GLOB TEST_SRC CONFIGURE_DEPENDS tests/*.cpp)
add_executable(tamapoke_tests ${TEST_SRC} ${HAL_SRC} src/present.cpp upstream/sdmon.cpp upstream/rtcbat.cpp upstream/audio.cpp)
target_include_directories(tamapoke_tests PRIVATE hal upstream src third_party/Arduino_GFX/src)
target_link_libraries(tamapoke_tests PRIVATE SDL3::SDL3 nlohmann_json::nlohmann_json gfx_core)
add_test(NAME tamapoke COMMAND tamapoke_tests)
set_tests_properties(tamapoke PROPERTIES WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

# SDL3 e DLL compartilhada: sem isto o exe de teste nao abre ("Erro do sistema").
add_custom_command(TARGET tamapoke_tests POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
          $<TARGET_FILE:SDL3::SDL3> $<TARGET_FILE_DIR:tamapoke_tests>)

# Nucleo real do Arduino_GFX (v1.6.9, submodulo) contra a nossa HAL. So compila;
# a Task 11A liga ao resto. Decisao: docs/decisions/0001-canvas.md.
add_library(gfx_core STATIC
  third_party/Arduino_GFX/src/Arduino_GFX.cpp
  third_party/Arduino_GFX/src/Arduino_G.cpp
  third_party/Arduino_GFX/src/Arduino_DataBus.cpp
  third_party/Arduino_GFX/src/canvas/Arduino_Canvas.cpp)
target_include_directories(gfx_core PRIVATE hal third_party/Arduino_GFX/src)

# Gerador .ino -> .cpp (prototipos como o builder do Arduino). Teste stdlib unittest.
find_package(Python3 REQUIRED COMPONENTS Interpreter)
add_test(NAME gen_protos COMMAND ${Python3_EXECUTABLE} -I ${CMAKE_SOURCE_DIR}/tests/test_gen_protos.py)

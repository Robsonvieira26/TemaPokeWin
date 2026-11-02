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
add_executable(tamapoke_tests ${TEST_SRC} ${HAL_SRC})
target_include_directories(tamapoke_tests PRIVATE hal upstream src)
target_link_libraries(tamapoke_tests PRIVATE SDL3::SDL3)
add_test(NAME tamapoke COMMAND tamapoke_tests)

# SDL3 e DLL compartilhada: sem isto o exe de teste nao abre ("Erro do sistema").
add_custom_command(TARGET tamapoke_tests POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
          $<TARGET_FILE:SDL3::SDL3> $<TARGET_FILE_DIR:tamapoke_tests>)

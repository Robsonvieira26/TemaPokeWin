cmake_minimum_required(VERSION 3.26)  # 3.26: copy_directory_if_different
project(tamapoke VERSION 0.1.0.0 LANGUAGES CXX RC)  # versao unica: assets/version.h.in -> app.rc

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
enable_testing()

# CRT estatico: o exe nao depende do redistribuivel do VC++. Definido uma vez, antes de
# qualquer alvo/dependencia, para que todos (gfx_core, nlohmann, testes, app, SDL3.dll) concordem.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

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
set(SDL_INSTALL OFF CACHE BOOL "" FORCE)  # so instalamos a DLL, via nossas regras abaixo
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

# Gerador .ino -> .cpp (prototipos como o builder do Arduino).
find_package(Python3 REQUIRED COMPONENTS Interpreter)
set(INO_GEN ${CMAKE_BINARY_DIR}/gen/TamaPoke.gen.cpp)
add_custom_command(
  OUTPUT ${INO_GEN}
  COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/gen
  COMMAND ${Python3_EXECUTABLE} -I ${CMAKE_SOURCE_DIR}/tools/gen_protos.py
          ${CMAKE_SOURCE_DIR}/upstream/TamaPoke.ino -o ${INO_GEN}
  DEPENDS ${CMAKE_SOURCE_DIR}/upstream/TamaPoke.ino ${CMAKE_SOURCE_DIR}/tools/gen_protos.py
  COMMENT "Gerando TamaPoke.gen.cpp a partir do .ino")
add_custom_target(ino_gen DEPENDS ${INO_GEN})
set_source_files_properties(${INO_GEN} PROPERTIES GENERATED TRUE)

# Fontes .cpp do jogo (reusadas pelo alvo do app em tarefa futura).
set(UPSTREAM_GAME_SRC
  upstream/pet.cpp
  upstream/i18n.cpp
  upstream/sdmon.cpp
  upstream/rtcbat.cpp
  upstream/audio.cpp)

# Testes contra a nossa HAL (nunca ve o shim do upstream; upstream/test fora do include path).
file(GLOB HAL_SRC CONFIGURE_DEPENDS hal/*.cpp)
file(GLOB TEST_SRC CONFIGURE_DEPENDS tests/*.cpp)
# Fontes do shell/jogo compartilhadas entre o app e os testes (sem main.cpp/app_shell.cpp).
set(CORE_SRC
  src/present.cpp src/game_host.cpp src/settings.cpp src/shell_logic.cpp
  src/win/tray.cpp src/win/tray_menu.cpp src/win/autostart.cpp src/win/single_instance.cpp src/win/window_mode.cpp src/win/console.cpp src/win/console_line.cpp
  ${HAL_SRC} ${UPSTREAM_GAME_SRC} ${INO_GEN})
add_executable(tamapoke_tests ${TEST_SRC} ${CORE_SRC})
target_include_directories(tamapoke_tests PRIVATE hal upstream src third_party/Arduino_GFX/src)
add_dependencies(tamapoke_tests ino_gen)
target_link_libraries(tamapoke_tests PRIVATE SDL3::SDL3 nlohmann_json::nlohmann_json gfx_core Shell32)
# char sem sinal como no Xtensa do ESP32: o Arduino_GFX compara bytes UTF-8 (c >= 0xE0)
# em getTextBounds(); com char com sinal (padrao do MSVC) o texto CJK mede errado.
if (MSVC)
  target_compile_options(tamapoke_tests PRIVATE /J)
endif()
add_test(NAME tamapoke COMMAND tamapoke_tests)
set_tests_properties(tamapoke PROPERTIES WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

# SDL3 e DLL compartilhada: sem isto o exe de teste nao abre ("Erro do sistema").
add_custom_command(TARGET tamapoke_tests POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
          $<TARGET_FILE:SDL3::SDL3> $<TARGET_FILE_DIR:tamapoke_tests>)

# O app: desktop pet (janela redonda, sem borda, sempre no topo). /J: o firmware compila aqui.
configure_file(assets/version.h.in ${CMAKE_BINARY_DIR}/gen/version.h @ONLY)
add_executable(tamapoke WIN32 src/main.cpp src/app_shell.cpp ${CORE_SRC} assets/app.rc)
target_include_directories(tamapoke PRIVATE ${CMAKE_BINARY_DIR}/gen)  # version.h (tambem para o RC)
set_source_files_properties(assets/app.rc PROPERTIES OBJECT_DEPENDS ${CMAKE_SOURCE_DIR}/assets/tamapoke.ico)
target_include_directories(tamapoke PRIVATE hal upstream src third_party/Arduino_GFX/src)
add_dependencies(tamapoke ino_gen)
target_link_libraries(tamapoke PRIVATE SDL3::SDL3 nlohmann_json::nlohmann_json gfx_core Shell32 Ole32)
if (MSVC)
  target_compile_options(tamapoke PRIVATE /J)
endif()
add_custom_command(TARGET tamapoke POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:SDL3::SDL3> $<TARGET_FILE_DIR:tamapoke>
  COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different
          ${CMAKE_SOURCE_DIR}/upstream/tools/sdcard/mons $<TARGET_FILE_DIR:tamapoke>/sdcard/mons
  COMMENT "Copiando SDL3.dll e sprites para a pasta de saida")

# Empacotamento (tools/package.ps1). Dois componentes: "app" (exe, SDL3.dll, docs) e "sprites"
# (sdcard/mons, CC BY-NC: so uso pessoal; o pacote publico nao os inclui, ver CREDITS.md).
# Prefixo padrao fora de "C:\Program Files": <build>/install.
if (CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
  set(CMAKE_INSTALL_PREFIX ${CMAKE_BINARY_DIR}/install CACHE PATH "Install prefix" FORCE)
endif()
install(TARGETS tamapoke RUNTIME DESTINATION . COMPONENT app)
install(FILES $<TARGET_FILE:SDL3::SDL3> DESTINATION . COMPONENT app)  # so a DLL (sem headers/.lib)
install(FILES LICENSE CREDITS.md README.md DESTINATION . COMPONENT app)
install(DIRECTORY upstream/tools/sdcard/mons DESTINATION sdcard COMPONENT sprites)
# Versao unica para os scripts de empacotamento (major.minor.patch).
file(WRITE ${CMAKE_BINARY_DIR}/gen/version.txt "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH}")

# Nucleo real do Arduino_GFX (v1.6.9, submodulo) contra a nossa HAL. So compila;
# a Task 11A liga ao resto. Decisao: docs/decisions/0001-canvas.md.
add_library(gfx_core STATIC
  third_party/Arduino_GFX/src/Arduino_GFX.cpp
  third_party/Arduino_GFX/src/Arduino_G.cpp
  third_party/Arduino_GFX/src/Arduino_DataBus.cpp
  third_party/Arduino_GFX/src/canvas/Arduino_Canvas.cpp)
target_include_directories(gfx_core PRIVATE hal third_party/Arduino_GFX/src)
if (MSVC)
  target_compile_options(gfx_core PRIVATE /J)  # ver comentario em tamapoke_tests
endif()

# Testes stdlib unittest dos scripts Python.
add_test(NAME gen_protos COMMAND ${Python3_EXECUTABLE} -I ${CMAKE_SOURCE_DIR}/tests/test_gen_protos.py)
add_test(NAME extract_u8g2_fonts COMMAND ${Python3_EXECUTABLE} -I ${CMAKE_SOURCE_DIR}/tests/test_extract_u8g2_fonts.py)
add_test(NAME make_icon COMMAND ${Python3_EXECUTABLE} -I ${CMAKE_SOURCE_DIR}/tests/test_make_icon.py)

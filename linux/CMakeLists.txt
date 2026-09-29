cmake_minimum_required(VERSION 3.16)
project(Kirikiroid2Linux
  VERSION 1.3.9
  DESCRIPTION "Native Linux host for the Kirikiroid2 1.3.9 engine"
  LANGUAGES CXX)

option(KRKR2_WITH_X11 "Enable the X11 window backend" ON)
option(KRKR2_WITH_SDL2 "Enable SDL2 fallback backend" ON)
option(KRKR2_WITH_TJS "Build the original TJS2 interpreter" ON)
option(KRKR2_WITH_TJS_REGEXP "Enable TJS2 regular expressions when Oniguruma is available" ON)
option(KRKR2_WITH_GTK3 "Build the GTK3 game-library launcher when available" ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Threads REQUIRED)
find_package(ZLIB REQUIRED)
set(KRKR2_DEFS)
set(KRKR2_LIBS Threads::Threads ZLIB::ZLIB ${CMAKE_DL_LIBS})
set(KRKR2_INCLUDES)

if(KRKR2_WITH_GTK3)
  find_package(PkgConfig QUIET)
  if(PkgConfig_FOUND)
    pkg_check_modules(GTK3 QUIET IMPORTED_TARGET gtk+-3.0)
  endif()
  if(GTK3_FOUND)
    list(APPEND KRKR2_DEFS KRKR2_HAVE_GTK3=1)
    list(APPEND KRKR2_LIBS PkgConfig::GTK3)
    message(STATUS "Kirikiroid2 Linux: GTK3 game library enabled")
  else()
    message(STATUS "Kirikiroid2 Linux: GTK3 not found; graphical launcher disabled")
  endif()
endif()

if(KRKR2_WITH_X11)
  find_package(X11 QUIET)
  if(X11_FOUND)
    list(APPEND KRKR2_DEFS KRKR2_HAVE_X11=1)
    if(TARGET X11::X11)
      list(APPEND KRKR2_LIBS X11::X11)
    else()
      list(APPEND KRKR2_LIBS ${X11_LIBRARIES})
      list(APPEND KRKR2_INCLUDES ${X11_INCLUDE_DIR})
    endif()
    message(STATUS "Kirikiroid2 Linux: X11 backend enabled")
  else()
    message(STATUS "Kirikiroid2 Linux: X11 not found; backend disabled")
  endif()
endif()

if(KRKR2_WITH_SDL2)
  find_package(SDL2 QUIET)
  if(SDL2_FOUND)
    list(APPEND KRKR2_DEFS KRKR2_HAVE_SDL2=1)
    if(TARGET SDL2::SDL2)
      list(APPEND KRKR2_LIBS SDL2::SDL2)
    elseif(TARGET SDL2::SDL2-static)
      list(APPEND KRKR2_LIBS SDL2::SDL2-static)
    else()
      list(APPEND KRKR2_LIBS ${SDL2_LIBRARIES})
      list(APPEND KRKR2_INCLUDES ${SDL2_INCLUDE_DIRS})
    endif()
    message(STATUS "Kirikiroid2 Linux: SDL2 fallback enabled")
  else()
    message(STATUS "Kirikiroid2 Linux: SDL2 not found; fallback disabled")
  endif()
endif()

set(KRKR2_HOST_SOURCES main.cpp host.cpp storage_root.cpp xp3_archive.cpp
  game_library.cpp launcher.cpp)

if(KRKR2_WITH_TJS)
  file(GLOB KRKR2_TJS_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_LIST_DIR}/../src/core/tjs2/*.cpp")
  list(FILTER KRKR2_TJS_SOURCES EXCLUDE REGEX "/tjsRegExp\\.cpp$")

  set(KRKR2_TJS_DEFS __STDC_CONSTANT_MACROS USE_UNICODE_FSTRING)
  set(KRKR2_TJS_LIBS Threads::Threads)
  set(KRKR2_TJS_PRIVATE_INCLUDES
    "${CMAKE_CURRENT_LIST_DIR}/../src/core/utils"
    "${CMAKE_CURRENT_LIST_DIR}/../src/core/environ")
  if(KRKR2_WITH_TJS_REGEXP)
    find_path(KRKR2_ONIG_INCLUDE_DIR NAMES oniguruma.h)
    find_library(KRKR2_ONIG_LIBRARY NAMES onig)
  endif()
  if(KRKR2_ONIG_INCLUDE_DIR AND KRKR2_ONIG_LIBRARY)
    list(APPEND KRKR2_TJS_SOURCES
      "${CMAKE_CURRENT_LIST_DIR}/../src/core/tjs2/tjsRegExp.cpp")
    list(APPEND KRKR2_TJS_LIBS "${KRKR2_ONIG_LIBRARY}")
    list(APPEND KRKR2_TJS_PRIVATE_INCLUDES "${KRKR2_ONIG_INCLUDE_DIR}")
    message(STATUS "Kirikiroid2 Linux: TJS2 regular expressions enabled")
  else()
    list(APPEND KRKR2_TJS_DEFS TJS_NO_REGEXP=1)
    message(STATUS "Kirikiroid2 Linux: Oniguruma not found; TJS2 regular expressions disabled")
  endif()

  add_library(krkr2-tjs STATIC ${KRKR2_TJS_SOURCES} tjs_platform.cpp)
  target_include_directories(krkr2-tjs
    PUBLIC "${CMAKE_CURRENT_LIST_DIR}/../src/core/tjs2"
    PRIVATE ${KRKR2_TJS_PRIVATE_INCLUDES})
  target_compile_definitions(krkr2-tjs PRIVATE ${KRKR2_TJS_DEFS} TJS_TEXT_OUT_CRLF=1)
  target_compile_options(krkr2-tjs PRIVATE -Wall -Wextra -Wno-unused-parameter)
  target_link_libraries(krkr2-tjs PUBLIC ${KRKR2_TJS_LIBS})

  list(APPEND KRKR2_HOST_SOURCES tjs_runtime.cpp)
  list(APPEND KRKR2_DEFS KRKR2_HAVE_TJS=1)
  list(APPEND KRKR2_LIBS krkr2-tjs)
  message(STATUS "Kirikiroid2 Linux: original TJS2 interpreter enabled")
endif()

add_executable(kirikiroid2-linux ${KRKR2_HOST_SOURCES})
target_compile_definitions(kirikiroid2-linux PRIVATE ${KRKR2_DEFS} KRKR2_VERSION=\"${PROJECT_VERSION}\")
target_include_directories(kirikiroid2-linux PRIVATE ${KRKR2_INCLUDES} "${CMAKE_CURRENT_LIST_DIR}")
target_link_libraries(kirikiroid2-linux PRIVATE ${KRKR2_LIBS})
target_compile_options(kirikiroid2-linux PRIVATE -Wall -Wextra -Wpedantic)

include(CTest)
if(BUILD_TESTING)
  find_package(Python3 COMPONENTS Interpreter QUIET)
  add_test(NAME linux-help COMMAND kirikiroid2-linux --help)
  add_test(NAME linux-headless COMMAND kirikiroid2-linux --no-window)
  add_executable(game-library-test tests/game_library_test.cpp game_library.cpp)
  target_include_directories(game-library-test PRIVATE "${CMAKE_CURRENT_LIST_DIR}")
  target_compile_options(game-library-test PRIVATE -Wall -Wextra -Wpedantic)
  add_test(NAME game-library COMMAND game-library-test)
  if(Python3_Interpreter_FOUND)
    set(KRKR2_TEST_XP3 "${CMAKE_CURRENT_BINARY_DIR}/smoke.xp3")
    set(KRKR2_TEST_RAW_XP3 "${CMAKE_CURRENT_BINARY_DIR}/smoke-raw.xp3")
    add_test(NAME xp3-create-fixture COMMAND "${Python3_EXECUTABLE}"
      "${CMAKE_CURRENT_LIST_DIR}/tests/make_xp3.py" "${KRKR2_TEST_XP3}")
    set_tests_properties(xp3-create-fixture PROPERTIES FIXTURES_SETUP xp3_fixture)
    add_test(NAME xp3-create-raw-fixture COMMAND "${Python3_EXECUTABLE}"
      "${CMAKE_CURRENT_LIST_DIR}/tests/make_xp3.py" "${KRKR2_TEST_RAW_XP3}" --raw)
    set_tests_properties(xp3-create-raw-fixture PROPERTIES FIXTURES_SETUP xp3_raw_fixture)
    add_test(NAME xp3-list COMMAND kirikiroid2-linux --list "${KRKR2_TEST_XP3}")
    set_tests_properties(xp3-list PROPERTIES
      FIXTURES_REQUIRED xp3_fixture PASS_REGULAR_EXPRESSION "startup.tjs")
    add_test(NAME xp3-list-raw COMMAND kirikiroid2-linux --list "${KRKR2_TEST_RAW_XP3}")
    set_tests_properties(xp3-list-raw PROPERTIES
      FIXTURES_REQUIRED xp3_raw_fixture PASS_REGULAR_EXPRESSION "startup.tjs")
  endif()
  if(KRKR2_WITH_TJS)
    add_test(NAME tjs-expression COMMAND kirikiroid2-linux --eval "1 + 2 * 3")
    set_tests_properties(tjs-expression PROPERTIES PASS_REGULAR_EXPRESSION "7")
    add_test(NAME tjs-formatting COMMAND kirikiroid2-linux --eval
      "\"%03d %.2f\".sprintf(7, 1.25)")
    set_tests_properties(tjs-formatting PROPERTIES PASS_REGULAR_EXPRESSION "007 1.25")
    add_test(NAME tjs-invalid-expression COMMAND kirikiroid2-linux --eval "1 +")
    set_tests_properties(tjs-invalid-expression PROPERTIES WILL_FAIL TRUE)
    add_test(NAME tjs-script COMMAND kirikiroid2-linux --script
      "${CMAKE_CURRENT_LIST_DIR}/tests/smoke.tjs")
    add_test(NAME tjs-directory-startup COMMAND kirikiroid2-linux --run
      "${CMAKE_CURRENT_LIST_DIR}/tests/game")
    set_tests_properties(tjs-directory-startup PROPERTIES
      PASS_REGULAR_EXPRESSION "startup script completed")
    add_test(NAME tjs-directory-fallback COMMAND kirikiroid2-linux --run
      "${CMAKE_CURRENT_LIST_DIR}/tests/fallback")
    set_tests_properties(tjs-directory-fallback PROPERTIES
      PASS_REGULAR_EXPRESSION "startup script completed")
    if(KRKR2_ONIG_INCLUDE_DIR AND KRKR2_ONIG_LIBRARY)
      add_test(NAME tjs-regexp COMMAND kirikiroid2-linux --eval
        "/linux/i.test(\"Linux\")")
      set_tests_properties(tjs-regexp PROPERTIES PASS_REGULAR_EXPRESSION "1")
    endif()
    if(Python3_Interpreter_FOUND)
      add_test(NAME tjs-xp3-script COMMAND kirikiroid2-linux --script
        "${KRKR2_TEST_XP3}>startup.tjs")
      set_tests_properties(tjs-xp3-script PROPERTIES
        FIXTURES_REQUIRED xp3_fixture)
      add_test(NAME tjs-xp3-script-raw COMMAND kirikiroid2-linux --script
        "${KRKR2_TEST_RAW_XP3}>startup.tjs")
      set_tests_properties(tjs-xp3-script-raw PROPERTIES
        FIXTURES_REQUIRED xp3_raw_fixture)
      add_test(NAME tjs-xp3-startup COMMAND kirikiroid2-linux --run
        "${KRKR2_TEST_XP3}")
      set_tests_properties(tjs-xp3-startup PROPERTIES
        FIXTURES_REQUIRED xp3_fixture PASS_REGULAR_EXPRESSION "startup script completed")
      add_test(NAME tjs-xp3-startup-raw COMMAND kirikiroid2-linux --run
        "${KRKR2_TEST_RAW_XP3}")
      set_tests_properties(tjs-xp3-startup-raw PROPERTIES
        FIXTURES_REQUIRED xp3_raw_fixture PASS_REGULAR_EXPRESSION "startup script completed")
    endif()
  endif()
endif()

include(GNUInstallDirs)
install(TARGETS kirikiroid2-linux RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES kirkr_host.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/kirikiroid2)
install(FILES kirikiroid2-linux.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
install(FILES "${CMAKE_CURRENT_LIST_DIR}/../icons/kirikiroid2-linux.png"
  DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/256x256/apps)
install(FILES README.md DESTINATION ${CMAKE_INSTALL_DATADIR}/doc/kirikiroid2)

set(CPACK_PACKAGE_NAME "kirikiroid2-linux")
set(CPACK_PACKAGE_VENDOR "Kirikiroid2 contributors")
set(CPACK_PACKAGE_CONTACT "https://github.com/zeas2/Kirikiroid2")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Native Linux X11/SDL2 host for Kirikiroid2")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_LIST_DIR}/../LICENSE")
set(CPACK_GENERATOR "DEB")
set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
set(CPACK_DEBIAN_PACKAGE_SECTION "games")
include(CPack)

cmake_minimum_required(VERSION 3.16)

# Vita build:    cmake -B build/vita -DCMAKE_TOOLCHAIN_FILE=$VITASDK/share/vita.toolchain.cmake
# Desktop build: cmake -B build/desktop
# (see ../build.sh, which also generates the audio clips first)
project(diamondrush C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(VITA)
  include("$ENV{VITASDK}/share/vita.cmake" REQUIRED)
  set(VITA_APP_NAME "Diamond Rush")
  set(VITA_TITLEID  "DRSH00001")   # 4 uppercase letters + 5 digits
  set(VITA_VERSION  "01.00")
  set(VITA_MKSFOEX_FLAGS "${VITA_MKSFOEX_FLAGS} -d ATTRIBUTE2=12")  # extended RAM
endif()

find_package(SDL2 REQUIRED)
find_package(SDL2_mixer REQUIRED)

file(GLOB GAME_SOURCES CONFIGURE_DEPENDS src/game/gen/*.cpp)
add_executable(diamondrush
  src/main.cpp
  src/platform/platform.cpp
  src/jrt/jrt.cpp
  src/midp/graphics.cpp
  src/midp/runner.cpp
  src/midp/rms.cpp
  src/midp/media.cpp
  ${GAME_SOURCES}
)
target_include_directories(diamondrush PRIVATE src)
# Java integer semantics: signed overflow wraps.
target_compile_options(diamondrush PRIVATE -fwrapv -Wall -Wno-unused-variable -Wno-unused-label
  -Wno-unused-but-set-variable -Wno-parentheses-equality -Wno-parentheses)

if(TARGET SDL2_mixer::SDL2_mixer-static)
  set(MIXER SDL2_mixer::SDL2_mixer-static)
else()
  set(MIXER SDL2_mixer::SDL2_mixer)
endif()
if(TARGET SDL2::SDL2-static AND VITA)
  set(SDL SDL2::SDL2-static)
else()
  set(SDL SDL2::SDL2)
endif()
target_link_libraries(diamondrush PRIVATE ${MIXER} ${SDL})

# Resources: everything in the jar except code, generated audio (build/gen-res), and port assets
# (port/res: button glyphs, widescreen title; made by tools/make_glyphs.py, tools/make_title_art.py,
# tools/make_banners_wide.py).
set(RES_DIRS "${CMAKE_SOURCE_DIR}/../jar" "${CMAKE_SOURCE_DIR}/../build/gen-res" "${CMAKE_SOURCE_DIR}/res")
# Staged into the build dir: also keeps vita-pack-vpk away from the project path's spaces.
set(RES_FILES)
foreach(dir ${RES_DIRS})
  file(GLOB_RECURSE files RELATIVE "${dir}" CONFIGURE_DEPENDS "${dir}/*")
  list(FILTER files EXCLUDE REGEX "(\\.class$|^META-INF/|^com/|\\.mid$)")
  foreach(f ${files})
    configure_file("${dir}/${f}" "${CMAKE_BINARY_DIR}/res/${f}" COPYONLY)
    list(APPEND RES_FILES "${f}")
  endforeach()
endforeach()

if(VITA)
  target_compile_options(diamondrush PRIVATE -O2)
  target_link_libraries(diamondrush PRIVATE pthread -Wl,-q)  # std::thread (jrt Thread)
  vita_create_self(eboot.bin diamondrush UNSAFE)

  set(VPK_FILES)
  foreach(f ${RES_FILES})
    list(APPEND VPK_FILES FILE "${CMAKE_BINARY_DIR}/res/${f}" "res/${f}")
  endforeach()
  foreach(f icon0.png livearea/contents/bg.png livearea/contents/startup.png livearea/contents/template.xml)
    if(EXISTS "${CMAKE_SOURCE_DIR}/sce_sys/${f}")
      configure_file("${CMAKE_SOURCE_DIR}/sce_sys/${f}" "${CMAKE_BINARY_DIR}/sce_sys/${f}" COPYONLY)
      list(APPEND VPK_FILES FILE "${CMAKE_BINARY_DIR}/sce_sys/${f}" "sce_sys/${f}")
    endif()
  endforeach()

  vita_create_vpk(diamondrush.vpk ${VITA_TITLEID} eboot.bin
    VERSION ${VITA_VERSION}
    NAME ${VITA_APP_NAME}
    ${VPK_FILES}
  )
else()
  target_compile_options(diamondrush PRIVATE -O1 -g -fsanitize=address,undefined -fno-sanitize=vptr)
  target_link_options(diamondrush PRIVATE -fsanitize=address,undefined)
endif()

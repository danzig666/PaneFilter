project(PaneFilter CXX)

set(SRC_CPP
    PaneFilter.cpp
    FarApi.cpp
    FarApi.hpp
    FarLang.h
    Guids.hpp
    Matching.cpp
    Matching.hpp
    Updater.cpp
    Updater.hpp
    UpdaterConfig.hpp
    core/Json.cpp
    core/Json.hpp
    core/Text.cpp
    core/Text.hpp
    version.hpp
    PaneFilter.rc)

set(SRC_DEF PaneFilter.def)

if (CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
  if (CMAKE_BUILD_TYPE MATCHES Debug)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreadedDebug")
  else ()
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded")
  endif ()

  set(FLAGS_CXX_DEFAULT /Zi)
  set(FLAGS_CXX_RELEASE /W3 /Oi /Ot /GL /GS-)
  set(FLAGS_CXX_DEBUG /W4 /GS)

  set(LINK_FLAGS "/MAP /MANIFEST:NO")

  if (CMAKE_BUILD_TYPE MATCHES "Release")
    set(LINK_FLAGS "${LINK_FLAGS} /incremental:no /OPT:REF /OPT:ICF /debug /ltcg")
  endif ()

  add_definitions(-DUNICODE -D_UNICODE -DNOMINMAX)
  add_compile_options(/utf-8)

  if (IS_x64_PLATFORM)
    set(CMAKE_RC_FLAGS -D_WIN64)
  else ()
    set(FLAGS_CXX_DEFAULT ${FLAGS_CXX_DEFAULT} /arch:IA32)
    set(FLAGS_C_DEFAULT ${FLAGS_C_DEFAULT} /arch:IA32)
  endif ()

  set(FLAGS_CXX_DEFAULT ${FLAGS_CXX_DEFAULT} /Zc:threadSafeInit-)
  set(FLAGS_C_DEFAULT ${FLAGS_C_DEFAULT} /Zc:threadSafeInit-)
endif ()

set(MAIN_FLAGS_CXX_DEBUG ${FLAGS_CXX_DEBUG} ${FLAGS_CXX_DEFAULT})
set(MAIN_FLAGS_CXX_RELEASE ${FLAGS_CXX_RELEASE} ${FLAGS_CXX_DEFAULT})
set(MAIN_FLAGS_C_DEBUG ${FLAGS_C_DEBUG} ${FLAGS_C_DEFAULT})
set(MAIN_FLAGS_C_RELEASE ${FLAGS_C_RELEASE} ${FLAGS_C_DEFAULT})

if (MSVC)
  set(ASM_SOURCE ${PROJECT_SOURCE_DIR}/../farsdk/vc_crt_fix.asm)
  set(ASM_OBJECT ${CMAKE_CURRENT_BINARY_DIR}/vc_crt_fix.obj)
  set(ASM_OBJECTS ${ASM_OBJECTS} ${ASM_OBJECT})
  set(MASM_DEFINES "")
  if (${CMAKE_CL_64} STREQUAL "0")
    find_program(MASM_EXECUTABLE ml)
  else ()
    find_program(MASM_EXECUTABLE ml64)
    set(MASM_DEFINES "/DX64")
  endif ()

  add_custom_command(
      OUTPUT ${ASM_OBJECT}
      COMMAND ${MASM_EXECUTABLE}
      ARGS /c ${MASM_DEFINES} /Fo ${ASM_OBJECT} ${ASM_SOURCE}
      DEPENDS ${ASM_SOURCE}
  )

  set(SRC_CPP ${SRC_CPP}
      ${PROJECT_SOURCE_DIR}/../farsdk/vc_crt_fix_impl.cpp
      ${ASM_OBJECTS}
      )
endif ()

set(SRC_FILES ${SRC_CPP} ${SRC_DEF})
add_library(PaneFilter SHARED ${SRC_FILES})
target_include_directories(PaneFilter
    PUBLIC ${PROJECT_SOURCE_DIR}/../farsdk/unicode
    )
target_link_libraries(PaneFilter PRIVATE winhttp bcrypt psapi normaliz)

set_target_properties(PaneFilter PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
    )

if (${CMAKE_BUILD_TYPE} MATCHES Debug)
  target_compile_options(PaneFilter
      PUBLIC $<$<COMPILE_LANGUAGE:C>:${MAIN_FLAGS_C_DEBUG}>
      PUBLIC $<$<COMPILE_LANGUAGE:CXX>:${MAIN_FLAGS_CXX_DEBUG}>
      )
else ()
  target_compile_options(PaneFilter
      PUBLIC $<$<COMPILE_LANGUAGE:C>:${MAIN_FLAGS_C_RELEASE}>
      PUBLIC $<$<COMPILE_LANGUAGE:CXX>:${MAIN_FLAGS_CXX_RELEASE}>
      )
endif ()

set_target_properties(PaneFilter
    PROPERTIES
    LINK_FLAGS "${LINK_FLAGS}"
    )

configure_file(File_ID.diz File_ID.diz)
configure_file(PaneFilter_Eng.hlf PaneFilter_Eng.hlf)

install(TARGETS PaneFilter RUNTIME DESTINATION .)
install(FILES $<TARGET_PDB_FILE:${PROJECT_NAME}> ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.map DESTINATION .)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/PaneFilter_Eng.hlf PaneFilter_Eng.lng DESTINATION .)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/File_ID.diz DESTINATION .)

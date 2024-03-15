add_library(Avendish)
add_library(Avendish::Avendish ALIAS Avendish)

if(MSVC)
  target_compile_options(Avendish
      PUBLIC
       -std:c++latest
       -Zi
       "-permissive-"
       "-Zc:__cplusplus"
       "-Zc:enumTypes"
       "-Zc:inline"
       "-Zc:preprocessor"
       "-Zc:templateScope"
       -wd4244)
  target_compile_definitions(Avendish PUBLIC -DNOMINMAX=1 -DWIN32_LEAN_AND_MEAN=1)
elseif(APPLE)
  target_compile_options(Avendish
    PUBLIC
      -Wno-c99-extensions
      -Wno-global-constructors
      -Wno-extra-semi
      -Wno-exit-time-destructors
      -Wno-cast-function-type
      -Wno-unused-template
      -Wno-unused-local-typedef
  )
endif()

target_sources(Avendish
  PUBLIC
    ${AVENDISH_SOURCES}
    $<TARGET_OBJECTS:avnd_dummy_lib>
)

if(AVENDISH_INCLUDE_SOURCE_ONLY)
  target_include_directories(
      Avendish
      PUBLIC
        $<BUILD_INTERFACE:${AVND_SOURCE_DIR}/include>
  )
  return()
endif()

function(avnd_target_setup AVND_FX_TARGET)
  target_compile_definitions(${AVND_FX_TARGET} PUBLIC -DFMT_HEADER_ONLY=1)
  target_compile_features(
      ${AVND_FX_TARGET}
      PUBLIC
        cxx_std_20
  )

  if(UNIX AND NOT APPLE)
    target_compile_options(
      ${AVND_FX_TARGET}
      PUBLIC
        -fno-semantic-interposition
        -fPIC
    )
  endif()

  if("${CMAKE_CXX_COMPILER_ID}" MATCHES "GNU")
    target_compile_options(
        ${AVND_FX_TARGET}
        PUBLIC
          -fcoroutines
          # -flto
          -ffunction-sections
          -fdata-sections
    )
  elseif("${CMAKE_CXX_COMPILER_ID}" MATCHES ".*Clang")
    target_compile_options(
        ${AVND_FX_TARGET}
        PUBLIC
          # -stdlib=libc++
          # -flto
          -fno-stack-protector
          -fno-ident
          -fno-plt
          -ffunction-sections
          -fdata-sections
    )

    if("${CMAKE_CXX_COMPILER_VERSION}" VERSION_GREATER_EQUAL 13.0)
      # target_compile_options(
      #   ${AVND_FX_TARGET}
      #   PUBLIC
      #     -fvisibility-inlines-hidden-static-local-var
      #     -fvirtual-function-elimination
      # )
    endif()

    if("${CMAKE_CXX_COMPILER_VERSION}" VERSION_LESS_EQUAL 15.0)
      target_compile_options(
        ${AVND_FX_TARGET}
        PUBLIC
          -fcoroutines-ts
      )
      endif()
  endif()

  if(TARGET fmt::fmt)
    target_link_libraries(
        ${AVND_FX_TARGET}
        PUBLIC fmt::fmt
    )
  elseif(TARGET fmt::fmt_header_only)
    target_link_libraries(
        ${AVND_FX_TARGET}
        PUBLIC fmt::fmt_header_only
    )
  endif()

  target_include_directories(
      ${AVND_FX_TARGET}
      PUBLIC
        $<BUILD_INTERFACE:${AVND_SOURCE_DIR}/include>
  )

  if(UNIX AND NOT APPLE)
    target_link_libraries(${AVND_FX_TARGET} PRIVATE
   #   -static-libgcc
   #   -static-libstdc++
      -Wl,--gc-sections
    )
  endif()

  if ("${CMAKE_CXX_COMPILER_ID}" MATCHES "GNU")
    target_link_libraries(${AVND_FX_TARGET}
      PRIVATE
        -Bsymbolic
        # -flto
    )
  elseif("${CMAKE_CXX_COMPILER_ID}" MATCHES "Clang")
    target_link_libraries(${AVND_FX_TARGET} PRIVATE
      # -lc++
      -Bsymbolic
      # -flto
    )
  endif()

  set_target_properties(
    ${AVND_FX_TARGET}
    PROPERTIES
      PREFIX ""
      POSITION_INDEPENDENT_CODE 1
      VISIBILITY_INLINES_HIDDEN 1
      CXX_VISIBILITY_PRESET internal
  )

  target_link_libraries(${AVND_FX_TARGET} PUBLIC Boost::boost)
endfunction()

function(avnd_common_setup AVND_TARGET AVND_FX_TARGET)
  avnd_target_setup("${AVND_FX_TARGET}")

  if(TARGET "${AVND_TARGET}")
    avnd_target_setup("${AVND_TARGET}")

    target_link_libraries(
      ${AVND_FX_TARGET}
      PUBLIC
        ${AVND_TARGET}
    )
  endif()
endfunction()

avnd_common_setup("" "Avendish")

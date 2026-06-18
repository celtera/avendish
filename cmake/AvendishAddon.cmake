# Defines avnd_addon_init / avnd_addon_object / avnd_addon_finalize. Loaded once
# Avendish is available (find_package(Avendish), which pulls this in via AvendishConfig).
#
# One CMakeLists then builds an addon both ways, with no host knowledge in the addon:
#  - as / against ossia score (avnd_score_plugin_add exists) -> ossia back-end only
#  - standalone (pure Avendish)                              -> avnd_make_* back-ends

if(TARGET score_plugin_avnd OR COMMAND avnd_score_plugin_add)
  set(AVND_ADDON_SCORE 1)
else()
  set(AVND_ADDON_SCORE 0)
endif()

# avnd_addon_init(NAME <base_target>)
#
# Creates the addon "base target" -- the name addons attach shared sources / link
# deps / include dirs to (target_sources(<base> PRIVATE shared.cpp),
# target_link_libraries(<base> ...), etc.). In a score build that is the plugin
# library. Standalone there is no single shipped artifact, but addons still write
# the same calls, so we create a real compiled STATIC target that behaves the same
# in both modes -- each object then links it (see avnd_addon_object) so the shared
# code / usage requirements reach every object and, transitively, every back-end.
macro(avnd_addon_init)
  cmake_parse_arguments(AA "" "NAME" "" ${ARGN})
  if(AVND_ADDON_SCORE)
    avnd_score_plugin_init(BASE_TARGET ${AA_NAME})
  elseif(NOT TARGET ${AA_NAME})
    # A generated empty TU guarantees a valid STATIC lib even when the addon never
    # adds sources to it (CMake needs at least one source / a linker language).
    set(_avnd_base_stub "${CMAKE_CURRENT_BINARY_DIR}/${AA_NAME}_avnd_base_stub.cpp")
    if(NOT EXISTS "${_avnd_base_stub}")
      file(WRITE "${_avnd_base_stub}"
        "// Avendish standalone addon base target (intentionally empty).\n")
    endif()
    add_library(${AA_NAME} STATIC "${_avnd_base_stub}")
    # Back-ends are typically shared objects (pd/max/... externals); the base's
    # objects get linked into them, so they must be position-independent.
    set_target_properties(${AA_NAME} PROPERTIES POSITION_INDEPENDENT_CODE ON)
    # Carry the addon source root so the addon's own headers resolve by their
    # in-repo angle path (<Addon/Foo.hpp>) -- in the base's shared sources, in
    # every object that links it, and (transitively) in every back-end, including
    # the generated prototype's #include <MAIN_FILE>. Subsumes the per-addon
    # "deps INTERFACE lib + CMAKE_CURRENT_SOURCE_DIR" shim.
    target_include_directories(${AA_NAME}
      PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>)
  endif()
endmacro()

# avnd_addon_object(
#   BASE <base_target> C_NAME <obj> CLASS <Class> [NAMESPACE <ns>]
#   SOURCES <a.hpp> [b.cpp ...] [MAIN_FILE <a.hpp>]
#   [DEPENDENCIES <target> ...]
#   # standalone back-ends -- pick one of:
#   [CATEGORY object|audioplug|texture|geometry|buffer|all]   (preset, default: object)
#   [BACKENDS <backend>[:<PROCESSOR_TYPE>] ...])              (explicit, e.g. touchdesigner:POP)
#
# In a score build only the object is registered (back-end args are ignored: score
# introspects the object). Standalone, CATEGORY runs an avnd_make_* preset, while
# BACKENDS emits one avnd_make_<backend> per entry with its optional PROCESSOR_TYPE --
# so e.g. BACKENDS godot:GEOMETRY touchdesigner:POP builds a POP but not a SOP.
#
# DEPENDENCIES are extra targets (typically a small INTERFACE library carrying an addon's
# include dirs / libs). They are linked to whatever compiles the object -- the score BASE
# target, or the standalone object library, which propagates them to every back-end (each
# back-end links the object library PUBLIC). So a single declaration works in both modes.
macro(avnd_addon_object)
  cmake_parse_arguments(AA "" "BASE;C_NAME;CLASS;NAMESPACE;CATEGORY;MAIN_FILE" "SOURCES;BACKENDS;DEPENDENCIES" ${ARGN})
  if(NOT AA_CATEGORY)
    set(AA_CATEGORY object)
  endif()
  if(NOT AA_MAIN_FILE)
    list(GET AA_SOURCES 0 AA_MAIN_FILE)
  endif()
  set(_ns_arg "")
  if(AA_NAMESPACE)
    set(_ns_arg NAMESPACE ${AA_NAMESPACE})
  endif()
  if(AVND_ADDON_SCORE)
    avnd_score_plugin_add(
      BASE_TARGET ${AA_BASE}
      TARGET ${AA_C_NAME}
      SOURCES ${AA_SOURCES}
      MAIN_CLASS ${AA_CLASS}
      ${_ns_arg})
    if(AA_DEPENDENCIES)
      target_link_libraries(${AA_BASE} PRIVATE ${AA_DEPENDENCIES})
    endif()
  else()
    if(NOT TARGET ${AA_C_NAME})
      add_library(${AA_C_NAME} STATIC ${AA_SOURCES})
    endif()
    # Link the addon base so its shared sources / deps / include dirs reach this
    # object and, since each back-end links the object PUBLIC, every back-end too.
    if(TARGET ${AA_BASE} AND NOT "${AA_BASE}" STREQUAL "${AA_C_NAME}")
      target_link_libraries(${AA_C_NAME} PUBLIC ${AA_BASE})
      # In a score build the objects are compiled *into* the base target, so the
      # base's own (incl. PRIVATE) include dirs / definitions / options apply to
      # them. Standalone each object is a separate target linking the base, so
      # PRIVATE settings would not reach it -- replicate score's behaviour by
      # inheriting the base's full build interface. PUBLIC so each back-end (which
      # links the object) compiles its generated prototype + MAIN_FILE the same way.
      target_include_directories(${AA_C_NAME} PUBLIC
        $<TARGET_PROPERTY:${AA_BASE},INCLUDE_DIRECTORIES>)
      target_compile_definitions(${AA_C_NAME} PUBLIC
        $<TARGET_PROPERTY:${AA_BASE},COMPILE_DEFINITIONS>)
      target_compile_options(${AA_C_NAME} PUBLIC
        $<TARGET_PROPERTY:${AA_BASE},COMPILE_OPTIONS>)
    endif()
    if(AA_DEPENDENCIES)
      target_link_libraries(${AA_C_NAME} PUBLIC ${AA_DEPENDENCIES})
    endif()
    set(_cls "${AA_CLASS}")
    if(AA_NAMESPACE)
      set(_cls "${AA_NAMESPACE}::${AA_CLASS}")
    endif()
    if(AA_BACKENDS)
      foreach(_spec ${AA_BACKENDS})
        string(REPLACE ":" ";" _parts "${_spec}")
        list(GET _parts 0 _backend)
        set(_ptype_arg "")
        list(LENGTH _parts _n)
        if(_n GREATER 1)
          list(GET _parts 1 _ptype)
          set(_ptype_arg PROCESSOR_TYPE ${_ptype})
        endif()
        # Respect AVND_ENABLE_<BACKEND> if defined (set by avendish.cmake).
        string(TOUPPER "${_backend}" _backend_upper)
        set(_skip 0)
        if(DEFINED AVND_ENABLE_${_backend_upper} AND NOT AVND_ENABLE_${_backend_upper})
          set(_skip 1)
        endif()
        if(NOT _skip)
          cmake_language(CALL "avnd_make_${_backend}"
            TARGET ${AA_C_NAME}
            MAIN_FILE "${AA_MAIN_FILE}"
            MAIN_CLASS ${_cls}
            C_NAME ${AA_C_NAME}
            ${_ptype_arg})
        endif()
      endforeach()
    else()
      cmake_language(CALL "avnd_make_${AA_CATEGORY}"
        TARGET ${AA_C_NAME}
        MAIN_FILE "${AA_MAIN_FILE}"
        MAIN_CLASS ${_cls}
        C_NAME ${AA_C_NAME})
    endif()
  endif()
endmacro()

# avnd_addon_finalize(NAME <base_target> UUID <uuid> VERSION <ver>)
macro(avnd_addon_finalize)
  cmake_parse_arguments(AA "" "NAME;UUID;VERSION" "" ${ARGN})
  if(AVND_ADDON_SCORE)
    avnd_score_plugin_finalize(
      BASE_TARGET ${AA_NAME}
      PLUGIN_UUID ${AA_UUID}
      PLUGIN_VERSION ${AA_VERSION})
  endif()
endmacro()

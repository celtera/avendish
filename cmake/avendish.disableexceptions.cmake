add_library(DisableExceptions STATIC src/disable_exceptions.cpp)

# Most back-ends (pd, max, vst3, clap, vintage, gstreamer, the example host, ...)
# link DisableExceptions to build the add-on glue without exceptions/RTTI. Some
# add-ons cannot tolerate that: they wrap a library whose API throws -- e.g.
# onnxruntime, whose C++ headers expand to `throw` -- so -fno-exceptions makes them
# fail to even compile. Such an add-on sets AVND_USES_EXCEPTIONS before pulling in
# Avendish. DisableExceptions then stays a valid link target (every back-end that
# links it keeps working) but carries no exception/RTTI-stripping flags -- it becomes
# a no-op -- and its boost::throw_exception stub is dropped so Boost's own (throwing)
# implementation is used instead of one that abort()s.
if(NOT AVND_USES_EXCEPTIONS)
  target_compile_definitions(DisableExceptions PRIVATE AVND_DISABLE_EXCEPTIONS=1)
  target_compile_definitions(DisableExceptions INTERFACE
    BOOST_NO_RTTI=1
    BOOST_NO_TYPEID=1
    BOOST_NO_EXCEPTIONS=1
  )
  if(MSVC)
    target_compile_options(DisableExceptions INTERFACE
      /GR- # no rtti
      /EHs- /EHc- # no exceptions
    )
  elseif("${CMAKE_CXX_COMPILER_ID}" MATCHES "GNU")
    target_compile_options(DisableExceptions INTERFACE
      -fno-exceptions
      -fno-rtti
    )
  elseif("${CMAKE_CXX_COMPILER_ID}" MATCHES ".*Clang")
    target_compile_definitions(DisableExceptions INTERFACE
      _LIBCPP_NO_EXCEPTIONS=1
    )
    target_compile_options(DisableExceptions INTERFACE
      -fno-exceptions
      -fno-rtti
      -fno-unwind-tables
      -fno-asynchronous-unwind-tables
    )
  endif()
endif()

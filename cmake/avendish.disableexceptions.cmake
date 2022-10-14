add_library(DisableExceptions STATIC src/disable_exceptions.cpp)

target_compile_definitions(DisableExceptions INTERFACE
  BOOST_NO_RTTI=1
  BOOST_NO_TYPEID=1
  BOOST_NO_EXCEPTIONS=1
)
if("${CMAKE_CXX_COMPILER_ID}" MATCHES "GNU")
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

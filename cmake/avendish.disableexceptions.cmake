add_library(DisableExceptions INTERFACE)

if("${CMAKE_CXX_COMPILER_ID}" MATCHES "GNU")
  target_compile_options(DisableExceptions INTERFACE
    -fno-exceptions
    -fno-rtti
  )
elseif("${CMAKE_CXX_COMPILER_ID}" MATCHES ".*Clang")
  target_compile_options(DisableExceptions INTERFACE
    -fno-exceptions
    -fno-rtti
    -fno-unwind-tables
    -fno-asynchronous-unwind-tables
  )
endif()

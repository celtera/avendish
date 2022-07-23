add_library(nuklear_impl STATIC
  src/nuklear.cpp
)
target_include_directories(nuklear_impl
  PRIVATE
    include)
avnd_target_setup(nuklear_impl)

if(TARGET GLEW::GLEW)
  target_link_libraries(
    nuklear_impl
    PUBLIC
      GLEW::GLEW
      glfw
      OpenGL::GL
  )
endif()

target_link_libraries(
  nuklear_impl
  PUBLIC
    X11
)

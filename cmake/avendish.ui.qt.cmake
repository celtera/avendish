find_package(Qt5 5.15 COMPONENTS Core Gui Qml Quick)
if(NOT TARGET Qt5::Core)
  return()
endif()

target_sources(Avendish PRIVATE
  include/avnd/binding/ui/qml_ui.hpp
  include/avnd/binding/ui/qml/enum_control.hpp
  include/avnd/binding/ui/qml/enum_ui.hpp
  include/avnd/binding/ui/qml/toggle_control.hpp
  include/avnd/binding/ui/qml/toggle_ui.hpp
  include/avnd/binding/ui/qml/float_control.hpp
  include/avnd/binding/ui/qml/float_knob.hpp
  include/avnd/binding/ui/qml/float_slider.hpp
  include/avnd/binding/ui/qml/int_control.hpp
  include/avnd/binding/ui/qml/int_knob.hpp
  include/avnd/binding/ui/qml/int_slider.hpp
)

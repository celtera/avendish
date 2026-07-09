# Point AVND_DIR at the avendish checkout; nuklear/canvas_ity headers can be
# vendored or taken from a desktop build's _deps directory.
set(AVND_DIR "$ENV{AVND_DIR}" CACHE PATH "avendish source dir")

idf_component_register(
  SRCS "avnd_ui_main.cpp"
  INCLUDE_DIRS
    "${AVND_DIR}/include"
    "${AVND_DIR}/thirdparty/nuklear"      # nuklear.h
    "${AVND_DIR}/thirdparty/canvas_ity"   # canvas_ity.hpp
  EMBED_FILES "font.ttf"                  # any TTF; registered at startup
  REQUIRES esp_lcd driver
)

target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_23)

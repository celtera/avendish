cmake_minimum_required(VERSION 3.13 FATAL_ERROR)

set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

project(avendish)

include_directories(
  /opt/libossia/include
  /usr/include/qt
  /usr/include/qt/QtCore
  /usr/include/qt/QtGui
  /usr/include/qt/QtQuick
  /usr/include/qt/QtQml
  /usr/include/python3.9
)

include(avendish)
include(avendish.examples)
include(avendish.tests)

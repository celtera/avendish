#pragma once

#define $(name, value) \
  static consteval auto name() { return value; }

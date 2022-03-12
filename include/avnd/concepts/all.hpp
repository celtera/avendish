#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <avnd/concepts/audio_port.hpp>
#include <avnd/concepts/audio_processor.hpp>
#include <avnd/concepts/callback.hpp>
#include <avnd/concepts/channels.hpp>
#include <avnd/concepts/generic.hpp>
#include <avnd/concepts/gfx.hpp>
#include <avnd/concepts/message.hpp>
#include <avnd/concepts/midi.hpp>
#include <avnd/concepts/midi_port.hpp>
#include <avnd/concepts/parameter.hpp>
#include <avnd/concepts/port.hpp>
#include <avnd/concepts/processor.hpp>

namespace avnd
{
#define STATIC_TODO(T) static_assert(std::is_void_v<T>, "TODO !");
#define AVND_ERROR(T, Message) static_assert(std::is_void_v<T>, Message);
#define TODO      \
  do              \
  {               \
    std::abort(); \
  } while (0)
}

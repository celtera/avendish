#pragma once

/*
 * Copyright (C) 2006-2021  Music Technology Group - Universitat Pompeu Fabra
 *
 * This file is ported from Essentia
 * Original version:
 *
 *  https://github.com/MTG/essentia/blob/master/src/algorithms/stats/entropy.h
 *
 * Essentia is free software: you can redistribute it and/or modify it under
 * the terms of the GNU Affero General Public License as published by the Free
 * Software Foundation (FSF), either version 3 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the Affero GNU General Public License
 * version 3 along with this program.  If not, see http://www.gnu.org/licenses/
 */


#include <halp/audio.hpp>
#include <halp/controls.hpp>
#include <halp/meta.hpp>

#include "../essentia.hpp"

#include <algorithm>
#include <stdexcept>
#include <cmath>

#include <optional>

namespace essentia_ports
{
using error = std::optional<const char*>;
constexpr auto success = std::nullopt;

template <typename T> void normalizeSum(std::span<T>& array) {
  if (array.empty()) return;

  T sumElements = (T) 0.;
  for (size_t i=0; i<array.size(); ++i) {
    if (array[i] < 0) return;
    sumElements += array[i];
  }

  if (sumElements != (T) 0.0) {
    for (size_t i=0; i<array.size(); ++i) {
      array[i] /= sumElements;
    }
  }
}

struct Entropy
{
  halp_meta(name, "Entropy")
  halp_meta(c_name, "avnd_essentia_entropy")
  halp_meta(category, "Statistics")
  halp_meta(version, "1.0")
  halp_meta(author, "Gerard Roma")
  halp_meta(vendor, "Music Technology Group - Universitat Pompeu Fabra")
  halp_meta(description, DOC("This algorithm computes the Shannon entropy of an array. Entropy can be used to quantify the peakiness of a distribution. This has been used for voiced/unvoiced decision in automatic speech recognition. \n"
                             "\n"
                             "Entropy cannot be computed neither on empty arrays nor arrays which contain negative values. In such cases, exceptions will be thrown.\n"
                             "\n"
                             "References:\n"
                             "  [1] H. Misra, S. Ikbal, H. Bourlard and H. Hermansky, \"Spectral entropy\n"
                             "  based feature for robust ASR,\" in IEEE International Conference on\n"
                             "  Acoustics, Speech, and Signal Processing (ICASSP'04)."))
  halp_meta(uuid, "f728cdd1-702f-4aa3-8e17-bb5149358bf7")

  struct {
    array_port<"array", "the input array (cannot contain negative values, and must be non-empty)"> array;
  } inputs;

  struct {
    real_port<"entropy", "the entropy of the input array"> entropy;
  } outputs;

  error operator()(std::size_t frames)
  {
    std::span<Real> array{inputs.array.channel, frames};
    Real& entropy = outputs.entropy;

    if (array.size() == 0) {
      return error{"Entropy: array does not contain any values"};
    }

    if (std::ranges::any_of(array, [](Real value) { return value < 0; })) {
      return error{"Entropy: array must not contain negative values"};
    }

    normalizeSum(array);
    entropy = 0.0;

    for (size_t i=0; i<array.size(); ++i) {
        if (array[i]==0)array[i] = 1;
        entropy -= std::log2(array[i]) * array[i];
    }

    return success;
  }
};

}



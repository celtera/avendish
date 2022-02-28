#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <avnd/binding/vintage/vintage.hpp>
#include <avnd/wrappers/avnd.hpp>
#include <math.h>

#include <atomic>
#include <set>

namespace vintage
{
using namespace avnd;

using program_name = limited_string_view<vintage::Constants::ProgNameLen>;
using param_display = limited_string<vintage::Constants::ParamStrLen>;
using vendor_name = limited_string_view<vintage::Constants::VendorStrLen>;
using product_name = limited_string_view<vintage::Constants::ProductStrLen>;
using effect_name = limited_string_view<vintage::Constants::EffectNameLen>;
using name = limited_string_view<vintage::Constants::NameLen>;
using label = limited_string_view<vintage::Constants::LabelLen>;
using short_label = limited_string_view<vintage::Constants::ShortLabelLen>;
using category_label = limited_string_view<vintage::Constants::CategLabelLen>;
using file_name = limited_string_view<vintage::Constants::FileNameLen>;

}

#pragma once
#include "MapTool.hpp"

#include <halp/callback.hpp>
namespace ao
{
/**
 * Things we want:
 * 
 * - A LED combination object to lay them out, reverse, etc
 * -> dynamic
 * 
 * - Basic LED pattern generation
 * - Combine r, g, b channels from separate input arrays
 * 
 * - W support
 * 
 * - Some object to generate a pattern from characters ? 
 *
 * - Some object to generate a pattern by drawing an array, like wavecycle?
 *
 * - Rotate a pattern ?
 * 
 * - Array tool ?
 *  * rotate
 *  * repeat
 *  * reverse
 *  * window
 *  * stride
 *    * stride mode: 0, midpoint, keep value
 *  * gain
 *  * contrast
 *  
 *  
 * - Array crush
 *  * + noise
 *  * + rand
 * 
 * - HSV ?
 */

enum class ArrayToolStrideMode
{
  Zero,
  Keep
};
struct ArrayTool
{
  halp_meta(name, "Array tool")
  halp_meta(c_name, "array_tool")
  halp_meta(category, "Control/Mappings")
  halp_meta(author, "Jean-Michaël Celerier")
  halp_meta(description, "Tool for processing arrays")
  halp_meta(
      manual_url, "https://ossia.io/score-docs/processes/array-utilities.html#tool")
  halp_meta(uuid, "dc51200e-87d3-483e-8cb5-81491cad7384")

  struct ins
  {
    struct : halp::spinbox_f32<"Min", halp::free_range_min<>>
    {
      halp_flag(class_attribute);
      std::function<void(float)> update_controller;
    } in_min;
    struct : halp::spinbox_f32<"Max", halp::free_range_max<>>
    {
      halp_flag(class_attribute);
      std::function<void(float)> update_controller;
    } in_max;

    halp::toggle<"Learn min"> min_learn;
    halp::toggle<"Learn max"> max_learn;

    halp::knob_f32<"Gain", halp::range{-10., 10., 1.}> gain;
    halp::knob_f32<"Brightness", halp::range{-10., 10., 0.}> bright;
    // halp::knob_f32<"Contrast", halp::range{-1., 1., 0.}> contrast;
    halp::enum_t<ao::MapToolWrapMode, "Range behaviour"> range_behaviour;
    halp::enum_t<ao::MapToolShapeMode, "Shape behaviour"> shape_behaviour;
    halp::toggle<"Invert"> invert;
    halp::toggle<"Absolute value"> absolute;
    // halp::hslider_f32<"Window"> window;
    // halp::hslider_f32<"Window Strength"> window_str;

    halp::spinbox_i32<"Pre-padding L", halp::range{-256, 256, 0}> in_pad_l;
    halp::spinbox_i32<"Pre-padding R", halp::range{-256, 256, 0}> in_pad_r;
    halp::toggle<"Reverse"> reverse;
    halp::hslider_f32<"Rotate", halp::range{0., 1., 0.}> rotate;
    halp::spinbox_i32<"Repeat", halp::range{0, 256, 1}> repeat;
    halp::spinbox_i32<"Stride", halp::range{0, 256, 1}> stride;
    halp::enum_t<ArrayToolStrideMode, "Stride"> stridemode;
    halp::spinbox_i32<"Post-padding L", halp::range{-256, 256, 0}> out_pad_l;
    halp::spinbox_i32<"Post-padding R", halp::range{-256, 256, 0}> out_pad_r;

    halp::toggle<"Normalize"> normalize;
  } inputs;

  struct messages
  {
    struct msg
    {
      halp_meta(name, "Array")
      void operator()(ArrayTool& self, std::vector<float> v)
      {
        self.process(v);
        self.outputs.v(std::move(v));
      }
    } m;
  };

  struct outs
  {
    halp::callback<"Array", std::vector<float>> v;
  } outputs;

  void process(std::vector<float>& v)
  {
    if(v.empty())
      return;
    /// Learn min / max
    // TODO think of a better way to have host feature detection?
    if(inputs.in_min.update_controller)
    {
      // FIXME if (inputs.min_learn && v < inputs.in_min)
      // FIXME {
      // FIXME   inputs.in_min.value = v;
      // FIXME   inputs.in_min.update_controller(v);
      // FIXME }
      // FIXME if (inputs.max_learn && v > inputs.in_max)
      // FIXME {
      // FIXME   inputs.in_max.value = v;
      // FIXME   inputs.in_max.update_controller(v);
      // FIXME }
    }

    for(float& val : v)
    {
      val = inputs.absolute ? std::abs(val) : val;
      val = ao::rescale(val, inputs.in_min, inputs.in_max) * inputs.gain + inputs.bright;
      val = ao::wrap(inputs.range_behaviour.value, val);
      val = ao::shape(inputs.shape_behaviour.value, val);
      val = inputs.invert ? 1.f - val : val;
      val = inputs.normalize ? val
                             : val * (inputs.in_max - inputs.in_min) + inputs.in_min;
    }
    if(inputs.in_pad_l > 0)
    {
      v.insert(v.begin(), inputs.in_pad_l, 0.);
    }
    else if(inputs.in_pad_l < 0)
    {
      const int N = std::min(int(v.size()), -inputs.in_pad_l);
      v.erase(v.begin(), v.begin() + N);
    }

    if(inputs.in_pad_r > 0)
    {
      v.resize(v.size() + inputs.in_pad_r, 0.);
    }
    else if(inputs.in_pad_r < 0)
    {
      const int N = std::min(int(v.size()), -inputs.in_pad_r);
      v.erase((v.rbegin() + N).base(), v.rbegin().base());
    }

    if(inputs.reverse)
    {
      std::reverse(v.begin(), v.end());
    }

    if(std::size_t rotate_n = inputs.rotate * v.size();
       rotate_n > 0 && rotate_n < v.size())
    {
      std::rotate(v.begin(), v.begin() + rotate_n, v.end());
    }

    if(int stride = inputs.stride; stride > 1)
    {
      int orig_sz = v.size();
      auto copy = v;
      v.clear();
      v.resize(orig_sz * stride);
      switch(this->inputs.stridemode)
      {
        case ArrayToolStrideMode::Zero: {
          for(int i = 0; i < orig_sz; i++)
            v[i * stride] = copy[i];
          break;
        }
        case ArrayToolStrideMode::Keep: {
          for(int i = 0; i < orig_sz; i++)
            for(int s = 0; s < stride; s++)
              v[i * stride + s] = copy[i];
          break;
        }
      }
    }

    if(int rpt = std::abs(inputs.repeat); rpt > 1)
    {
      int orig_sz = v.size();
      v.resize(rpt * orig_sz);
      for(int i = 1; i < rpt; i++)
      {
        std::copy_n(v.begin(), orig_sz, v.begin() + i * orig_sz);
      }
    }

    if(inputs.out_pad_l > 0)
      v.insert(v.begin(), inputs.out_pad_l, 0.);
    else if(inputs.out_pad_l < 0)
      v.erase(v.begin(), v.begin() + std::min(int(v.size()), -inputs.out_pad_l));

    if(inputs.out_pad_r > 0)
    {
      v.resize(v.size() + inputs.out_pad_r, 0.);
    }
    else if(inputs.out_pad_r < 0)
    {
      int to_remove = std::min(int(v.size()), -inputs.out_pad_r);
      v.erase(v.begin() + v.size() - to_remove, v.end());
    }
  }
};
}

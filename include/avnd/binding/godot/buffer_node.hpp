#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <avnd/binding/godot/node.hpp>
#include <avnd/introspection/port.hpp>

#include <godot_cpp/variant/packed_byte_array.hpp>
#include <godot_cpp/variant/packed_float32_array.hpp>

namespace godot_binding
{

/// Upload a CPU buffer output port to a PackedByteArray.
/// Handles both raw buffers (unsigned char*) and typed buffers (T*).
template <typename Port>
void upload_cpu_buffer(Port& port, godot::PackedByteArray& pba)
{
  auto& buf = port.buffer;

  if constexpr(avnd::cpu_raw_buffer_port<Port>)
  {
    if(!buf.changed || !buf.raw_data || buf.byte_size <= 0)
      return;

    const auto sz = buf.byte_size - buf.byte_offset;
    if(sz <= 0)
      return;

    pba.resize(sz);
    memcpy(pba.ptrw(), buf.raw_data + buf.byte_offset, sz);
    buf.changed = false;
  }
  else if constexpr(avnd::cpu_typed_buffer_port<Port>)
  {
    if(!buf.changed || !buf.elements || buf.element_count <= 0)
      return;

    using elem_t = std::decay_t<decltype(buf.elements[0])>;
    const auto byte_sz = buf.element_count * sizeof(elem_t);

    pba.resize(byte_sz);
    memcpy(pba.ptrw(), buf.elements, byte_sz);
    buf.changed = false;
  }
}

/**
 * Godot Node wrapping an Avendish buffer generator/processor.
 *
 * Each frame, runs the processor's operator() and uploads output buffers
 * to Godot PackedByteArray properties.
 */
template <typename T>
struct godot_buffer_node : public godot::Node
{
  mutable avnd::effect_container<T> effect;

  static constexpr int buf_out_count
      = avnd::cpu_buffer_output_introspection<T>::size;

  godot::PackedByteArray buf_outputs[buf_out_count > 0 ? buf_out_count : 1];

  godot_buffer_node()
  {
    if constexpr(avnd::has_inputs<T>)
    {
      for(auto state : effect.full_state())
      {
        avnd::for_each_field_ref(state.inputs, [&]<typename Ctl>(Ctl& ctl) {
          if constexpr(avnd::has_range<Ctl>)
          {
            static_constexpr auto c = avnd::get_range<Ctl>();
            if_possible(ctl.value = c.values[c.init].second)
                else if_possible(ctl.value = c.values[c.init])
                else if_possible(ctl.value = c.init);
          }
        });
      }
    }
  }

  ~godot_buffer_node() override = default;

  void do_ready()
  {
    if constexpr(avnd::has_inputs<T>)
    {
      for(auto state : effect.full_state())
      {
        avnd::for_each_field_ref(state.inputs, [&]<typename Ctl>(Ctl& ctl) {
          if_possible(ctl.update(state.effect));
        });
      }
    }
  }

  void do_process(double delta)
  {
    if constexpr(requires { effect.effect({delta}); })
      effect.effect(delta);
    else if constexpr(requires { effect.effect(); })
      effect.effect();

    upload_buffers();
  }

  void do_get_property_list(godot::List<godot::PropertyInfo>* p_list) const
  {
    godot_binding::get_property_list<T>(p_list);

    int idx = 0;
    avnd::cpu_buffer_output_introspection<T>::for_all(
        [&]<auto Idx, typename C>(avnd::field_reflection<Idx, C>) {
      using outputs_t = typename avnd::outputs_type<T>::type;
      auto buf_name
          = godot::StringName(field_name<Idx, C, outputs_t>().data());
      p_list->push_back(godot::PropertyInfo(
          godot::Variant::PACKED_BYTE_ARRAY, buf_name));
      ++idx;
    });
  }

  bool do_set(const godot::StringName& p_name, const godot::Variant& p_value)
  {
    return godot_binding::set_property<T>(effect, p_name, p_value);
  }

  bool do_get(const godot::StringName& p_name, godot::Variant& r_ret) const
  {
    int idx = 0;
    bool found = false;
    avnd::cpu_buffer_output_introspection<T>::for_all(
        [&]<auto Idx, typename C>(avnd::field_reflection<Idx, C>) {
      if(found)
        return;
      using outputs_t = typename avnd::outputs_type<T>::type;
      static const godot::StringName buf_name{
          field_name<Idx, C, outputs_t>().data()};
      if(buf_name == p_name)
      {
        r_ret = buf_outputs[idx];
        found = true;
      }
      ++idx;
    });
    if(found)
      return true;

    return godot_binding::get_property<T>(effect, p_name, r_ret);
  }

  bool do_property_can_revert(const godot::StringName& p_name) const
  {
    return godot_binding::property_can_revert<T>(p_name);
  }

  bool do_property_get_revert(
      const godot::StringName& p_name, godot::Variant& r_property) const
  {
    return godot_binding::property_get_revert<T>(p_name, r_property);
  }

private:
  void upload_buffers()
  {
    int idx = 0;
    avnd::cpu_buffer_output_introspection<T>::for_all(
        [&]<auto Idx, typename C>(avnd::field_reflection<Idx, C>) {
      auto& port = avnd::pfr::get<Idx>(effect.outputs());
      upload_cpu_buffer(port, buf_outputs[idx]);
      ++idx;
    });
  }
};

}

// clang-format off
#define AVND_GODOT_BUFFER_NODE_OVERRIDES                                                 \
public:                                                                                  \
  void _ready() override { this->do_ready(); }                                           \
  void _process(double delta) override { this->do_process(delta); }                      \
  void _get_property_list(godot::List<godot::PropertyInfo>* p) const                     \
  { this->do_get_property_list(p); }                                                     \
  bool _set(const godot::StringName& n, const godot::Variant& v)                        \
  { return this->do_set(n, v); }                                                         \
  bool _get(const godot::StringName& n, godot::Variant& r) const                        \
  { return this->do_get(n, r); }                                                         \
  bool _property_can_revert(const godot::StringName& n) const                            \
  { return this->do_property_can_revert(n); }                                            \
  bool _property_get_revert(const godot::StringName& n, godot::Variant& r) const        \
  { return this->do_property_get_revert(n, r); }
// clang-format on

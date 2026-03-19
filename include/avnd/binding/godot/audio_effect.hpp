#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <avnd/binding/godot/node.hpp>
#include <avnd/wrappers/audio_channel_manager.hpp>
#include <avnd/wrappers/process_adapter.hpp>

#include <godot_cpp/classes/audio_effect.hpp>
#include <godot_cpp/classes/audio_effect_instance.hpp>

#include <vector>

namespace godot_binding
{

/// Godot audio is always stereo AudioFrame { float left, right; }.
/// Avendish expects separate per-channel float* buffers.
/// This adapter deinterleaves input, runs the processor, re-interleaves output.
template <typename T>
struct audio_process_adapter
{
  static constexpr int godot_channels = 2; // Godot is always stereo

  avnd::effect_container<T> effect;
  avnd::process_adapter<T> processor;
  AVND_NO_UNIQUE_ADDRESS avnd::audio_channel_manager<T> channels{effect};

  std::vector<float> in_buf_l, in_buf_r;
  std::vector<float> out_buf_l, out_buf_r;

  bool prepared{false};

  void prepare(int frames, double rate)
  {
    in_buf_l.resize(frames);
    in_buf_r.resize(frames);
    out_buf_l.resize(frames);
    out_buf_r.resize(frames);

    channels.set_input_channels(effect, 0, godot_channels);
    channels.set_output_channels(effect, 0, godot_channels);

    avnd::process_setup setup{
        .input_channels = godot_channels,
        .output_channels = godot_channels,
        .frames_per_buffer = frames,
        .rate = rate};

    processor.allocate_buffers(setup, float{});
    effect.init_channels(godot_channels, godot_channels);
    avnd::prepare(effect, setup);

    if constexpr(avnd::has_inputs<T>)
      avnd::init_controls(effect);

    prepared = true;
  }

  /// p_src_buffer: const AudioFrame* (interleaved stereo: L R L R ...)
  /// p_dst_buffer: AudioFrame* (interleaved stereo)
  /// AudioFrame is { float left; float right; }
  void process(const void* p_src_buffer, void* p_dst_buffer, int32_t frame_count)
  {
    if(!prepared || frame_count <= 0)
      return;

    // Resize scratch buffers if needed
    if(std::ssize(in_buf_l) < frame_count)
    {
      in_buf_l.resize(frame_count);
      in_buf_r.resize(frame_count);
      out_buf_l.resize(frame_count);
      out_buf_r.resize(frame_count);
    }

    // Deinterleave stereo AudioFrame input → separate channel buffers
    const float* src = static_cast<const float*>(p_src_buffer);
    for(int32_t i = 0; i < frame_count; ++i)
    {
      in_buf_l[i] = src[i * 2 + 0];
      in_buf_r[i] = src[i * 2 + 1];
    }

    // Clear output
    std::fill_n(out_buf_l.data(), frame_count, 0.f);
    std::fill_n(out_buf_r.data(), frame_count, 0.f);

    // Run Avendish processor with separate channel buffers
    float* in_ptrs[2] = {in_buf_l.data(), in_buf_r.data()};
    float* out_ptrs[2] = {out_buf_l.data(), out_buf_r.data()};

    processor.process(
        effect, std::span<float*>(in_ptrs, godot_channels),
        std::span<float*>(out_ptrs, godot_channels), frame_count);

    // Interleave separate channel buffers → stereo AudioFrame output
    float* dst = static_cast<float*>(p_dst_buffer);
    for(int32_t i = 0; i < frame_count; ++i)
    {
      dst[i * 2 + 0] = out_buf_l[i];
      dst[i * 2 + 1] = out_buf_r[i];
    }
  }
};

/**
 * Godot AudioEffectInstance wrapping an Avendish audio processor.
 *
 * Provides do_* implementation methods. The concrete GDCLASS wrapper
 * must use AVND_GODOT_AUDIO_EFFECT_INSTANCE_OVERRIDES for virtual dispatch.
 */
template <typename T>
struct godot_audio_effect_instance : public godot::AudioEffectInstance
{
  audio_process_adapter<T> adapter;

  // Points to the parent AudioEffect's params; kept alive by base_ref.
  avnd::effect_container<T>* params_src{nullptr};
  godot::Ref<godot::AudioEffect> base_ref;

  godot_audio_effect_instance() = default;

  void init(avnd::effect_container<T>& params, const godot::Ref<godot::AudioEffect>& ref)
  {
    params_src = &params;
    base_ref = ref;
    adapter.prepare(1024, 44100.0); // FIXME get proper samplerate / buffer size
  }

  void do_process(
      const void* p_src_buffer, godot::AudioFrame* p_dst_buffer,
      int32_t p_frame_count)
  {
    if(p_frame_count <= 0)
      return;

    sync_parameters();
    adapter.process(p_src_buffer, p_dst_buffer, p_frame_count);
  }

private:
  void sync_parameters()
  {
    if(!params_src)
      return;

    if constexpr(avnd::has_inputs<T>)
    {
      auto& src = params_src->inputs();
      auto& dst = adapter.effect.inputs();
      avnd::parameter_input_introspection<T>::for_all(
          [&]<auto Idx, typename C>(avnd::field_reflection<Idx, C>) {
        avnd::pfr::get<Idx>(dst).value = avnd::pfr::get<Idx>(src).value;
      });
    }
  }
};

/**
 * Godot AudioEffect (Resource) wrapping an Avendish processor's parameters.
 *
 * Provides do_* implementation methods. The concrete GDCLASS wrapper
 * must use AVND_GODOT_AUDIO_EFFECT_OVERRIDES for virtual dispatch.
 */
template <typename T, typename InstanceClass>
struct godot_audio_effect : public godot::AudioEffect
{
  mutable avnd::effect_container<T> params;

  godot_audio_effect()
  {
    if constexpr(avnd::has_inputs<T>)
      avnd::init_controls(params);
  }

  ~godot_audio_effect() override = default;

  godot::Ref<godot::AudioEffectInstance> do_instantiate()
  {
    godot::Ref<InstanceClass> inst;
    inst.instantiate();
    inst->init(params, godot::Ref<godot::AudioEffect>(this));
    return inst;
  }

  void do_get_property_list(godot::List<godot::PropertyInfo>* p_list) const
  {
    godot_binding::get_property_list<T>(p_list);
  }

  bool do_set(const godot::StringName& p_name, const godot::Variant& p_value)
  {
    return godot_binding::set_property<T>(params, p_name, p_value);
  }

  bool do_get(const godot::StringName& p_name, godot::Variant& r_ret) const
  {
    return godot_binding::get_property<T>(params, p_name, r_ret);
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
};

}

// clang-format off
#define AVND_GODOT_AUDIO_EFFECT_INSTANCE_OVERRIDES                                      \
public:                                                                                 \
  void _process(const void* s, godot::AudioFrame* d, int32_t n) override                \
  { this->do_process(s, d, n); }

#define AVND_GODOT_AUDIO_EFFECT_OVERRIDES                                               \
public:                                                                                 \
  godot::Ref<godot::AudioEffectInstance> _instantiate() override                        \
  { return this->do_instantiate(); }                                                    \
  void _get_property_list(godot::List<godot::PropertyInfo>* p) const                    \
  { this->do_get_property_list(p); }                                                    \
  bool _set(const godot::StringName& n, const godot::Variant& v)                       \
  { return this->do_set(n, v); }                                                        \
  bool _get(const godot::StringName& n, godot::Variant& r) const                       \
  { return this->do_get(n, r); }                                                        \
  bool _property_can_revert(const godot::StringName& n) const                           \
  { return this->do_property_can_revert(n); }                                           \
  bool _property_get_revert(const godot::StringName& n, godot::Variant& r) const       \
  { return this->do_property_get_revert(n, r); }
// clang-format on

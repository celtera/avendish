#pragma once

#include <avnd/binding/gstreamer/utils.hpp>
#include <avnd/wrappers/process_adapter.hpp>
#include <cmath>

namespace gst
{

// Audio source element for objects that generate audio data
template <typename T>
  requires(is_audio_generator<T>())
struct element<T> : property_handler
{
  GstPushSrc the_object; // MUST be first for GObject
  avnd::effect_container<T> impl;
  avnd::process_adapter<T> processor;

  void init() { }
  // Using deducing this for property handling
  using effect_type = T;

  void set_property(guint property_id, const GValue* value, GParamSpec* pspec)
  {
    property_handler::set_property(property_id, value, pspec);
  }

  void get_property(guint property_id, GValue* value, GParamSpec* pspec)
  {
    property_handler::get_property(property_id, value, pspec);
  }

  gboolean start()
  {
    GST_DEBUG_OBJECT(this, "start");
    // Set up fixed caps for stereo F32LE audio at 48kHz
    GstCaps* caps = gst_caps_new_simple(
        "audio/x-raw", "format", G_TYPE_STRING, "F32LE", "channels", G_TYPE_INT, 2,
        "rate", G_TYPE_INT, 48000, "layout", G_TYPE_STRING, "interleaved", NULL);
    gst_base_src_set_caps(GST_BASE_SRC(this), caps);
    gst_caps_unref(caps);

    // Set buffer size for 1024 samples stereo F32LE (1024 * 2 * 4 bytes)
    gst_base_src_set_blocksize(GST_BASE_SRC(this), 1024 * 2 * 4);

    // Initialize process adapter buffers
    avnd::process_setup setup_info{
        .input_channels = 0,  // Generator has no inputs
        .output_channels = 2, // Stereo output
        .frames_per_buffer = 1024,
        .rate = 48000.0};
    processor.allocate_buffers(setup_info, float{});

    // Initialize Avendish effect
    avnd::prepare(impl, setup_info);

    return TRUE;
  }

  GstFlowReturn fill(GstBuffer* buf)
  {
    GST_DEBUG_OBJECT(
        this, "fill called with buffer size: %lu", gst_buffer_get_size(buf));

    GstMapInfo info;
    if(!gst_buffer_map(buf, &info, GST_MAP_WRITE))
    {
      GST_ERROR_OBJECT(this, "Failed to map buffer");
      return GST_FLOW_ERROR;
    }

    // Audio parameters (hardcoded for now, should match caps)
    int channels = 2;
    int sample_rate = 48000;
    gsize n_frames = info.size / (channels * sizeof(float));

    GST_DEBUG_OBJECT(this, "Generating %lu frames of audio", n_frames);

    if constexpr(avnd::audio_bus_output_introspection<T>::size > 0)
    {
      // Use process_adapter for Avendish effects with audio outputs
      std::vector<float*> output_ptrs(channels);
      std::vector<std::vector<float>> output_channels(channels);

      // Allocate output channel arrays
      for(int ch = 0; ch < channels; ch++)
      {
        output_channels[ch].resize(n_frames);
        output_ptrs[ch] = output_channels[ch].data();
      }

      // Process through process_adapter (no inputs for generators)
      processor.process(
          impl, avnd::span<float*>{}, // Empty input span for generator
          avnd::span<float*>{output_ptrs.data(), (size_t)channels}, n_frames);

      // Convert from planar to interleaved format
      float* samples = (float*)info.data;
      for(int ch = 0; ch < channels; ch++)
      {
        for(gsize i = 0; i < n_frames; i++)
        {
          samples[i * channels + ch] = output_channels[ch][i];
        }
      }
    }
    else
    {
      GST_DEBUG_OBJECT(this, "No Avendish audio output!");
      std::fill_n((float*)info.data, channels * n_frames, 0);
    }

    gst_buffer_unmap(buf, &info);
    GST_DEBUG_OBJECT(this, "fill completed successfully");
    return GST_FLOW_OK;
  }
};

template <typename T>
  requires(is_audio_generator<T>())
struct metaclass<T>
{
  using type = T;
  static inline GstDebugCategory* debug_category = nullptr;

  static inline GstStaticPadTemplate src_pad_template = GST_STATIC_PAD_TEMPLATE(
      "src", GST_PAD_SRC, GST_PAD_ALWAYS,
      GST_STATIC_CAPS(
          "audio/x-raw,format={F32LE,F64LE,S16LE},channels=[1,8],rate=[8000,192000]"));

  GstPushSrcClass the_class;

  void init()
  {
    auto klass = this;
    GObjectClass* gobject_class = G_OBJECT_CLASS(klass);
    GstPushSrcClass* push_src_class = GST_PUSH_SRC_CLASS(klass);
    GstBaseSrcClass* base_src_class = GST_BASE_SRC_CLASS(klass);

    gst_element_class_add_static_pad_template(
        GST_ELEMENT_CLASS(klass), &src_pad_template);

    init_properties(*this, "Source/Audio");

    // GStreamer API
    base_src_class->start = GST_DEBUG_FUNCPTR(+[](GstBaseSrc* gobject) -> gboolean {
      return ((element<T>*)gobject)->start();
    });

    push_src_class->fill
        = GST_DEBUG_FUNCPTR(+[](GstPushSrc* gobject, GstBuffer* buf) -> GstFlowReturn {
      return ((element<T>*)gobject)->fill(buf);
    });
  }

  static GType get_type()
  {
    static const GType tid = get_type_once<T>(gst_push_src_get_type());
    return tid;
  }
};
}

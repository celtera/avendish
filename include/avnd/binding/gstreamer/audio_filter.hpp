#pragma once

#include <avnd/binding/gstreamer/utils.hpp>
#include <avnd/wrappers/process_adapter.hpp>

#include <vector>

namespace gst
{
// Audio filter element for objects with audio input and output
template <typename T>
  requires(is_audio_filter<T>())
struct element<T> : property_handler
{
  GstBaseTransform the_object; // MUST be first for GObject
  avnd::effect_container<T> impl;
  avnd::process_adapter<T> processor;

  // Using deducing this for property handling
  using effect_type = T;

  void init() { }

  void set_property(guint property_id, const GValue* value, GParamSpec* pspec)
  {
    property_handler::set_property(property_id, value, pspec);
  }

  void get_property(guint property_id, GValue* value, GParamSpec* pspec)
  {
    property_handler::get_property(property_id, value, pspec);
  }

  GstFlowReturn transform(GstBuffer* inbuf, GstBuffer* outbuf)
  {
    GST_DEBUG_OBJECT(this, "transform");

    // Get audio format info from caps
    GstCaps* caps = gst_pad_get_current_caps(GST_BASE_TRANSFORM_SINK_PAD(this));
    if(!caps)
    {
      GST_ERROR_OBJECT(this, "No caps available");
      return GST_FLOW_ERROR;
    }

    GstAudioInfo audio_info;
    if(!gst_audio_info_from_caps(&audio_info, caps))
    {
      GST_ERROR_OBJECT(this, "Failed to get audio info from caps");
      gst_caps_unref(caps);
      return GST_FLOW_ERROR;
    }
    gst_caps_unref(caps);

    GstMapInfo in_info, out_info;
    if(!gst_buffer_map(inbuf, &in_info, GST_MAP_READ))
    {
      GST_ERROR_OBJECT(this, "Failed to map input buffer");
      return GST_FLOW_ERROR;
    }

    if(!gst_buffer_map(outbuf, &out_info, GST_MAP_WRITE))
    {
      GST_ERROR_OBJECT(this, "Failed to map output buffer");
      gst_buffer_unmap(inbuf, &in_info);
      return GST_FLOW_ERROR;
    }

    // Ensure buffer sizes match
    if(in_info.size != out_info.size)
    {
      GST_ERROR_OBJECT(this, "Input and output buffer sizes don't match");
      gst_buffer_unmap(inbuf, &in_info);
      gst_buffer_unmap(outbuf, &out_info);
      return GST_FLOW_ERROR;
    }

    // Get audio parameters
    int channels = GST_AUDIO_INFO_CHANNELS(&audio_info);
    int sample_rate = GST_AUDIO_INFO_RATE(&audio_info);
    GstAudioFormat format = GST_AUDIO_INFO_FORMAT(&audio_info);
    int bps = GST_AUDIO_INFO_BPF(&audio_info); // bytes per frame
    gsize n_frames = in_info.size / bps;

    GST_DEBUG_OBJECT(
        this, "Processing %lu frames: %d channels, %d Hz, format=%s, bps=%d", n_frames,
        channels, sample_rate, gst_audio_format_to_string(format), bps);

    // Convert interleaved GStreamer audio to planar format for process_adapter
    std::vector<float*> input_ptrs(channels);
    std::vector<float*> output_ptrs(channels);
    std::vector<std::vector<float>> input_channels(channels);
    std::vector<std::vector<float>> output_channels(channels);

    // Allocate and prepare channel arrays
    for(int ch = 0; ch < channels; ch++)
    {
      input_channels[ch].resize(n_frames);
      output_channels[ch].resize(n_frames);
      input_ptrs[ch] = input_channels[ch].data();
      output_ptrs[ch] = output_channels[ch].data();
    }

    // Convert from interleaved to planar format
    float* samples = (float*)in_info.data;
    for(int ch = 0; ch < channels; ch++)
    {
      for(gsize i = 0; i < n_frames; i++)
      {
        input_channels[ch][i] = samples[i * channels + ch];
      }
    }

    // Process through process_adapter (handles all Avendish audio APIs)
    processor.process(
        impl, avnd::span<float*>{input_ptrs.data(), (size_t)channels},
        avnd::span<float*>{output_ptrs.data(), (size_t)channels}, n_frames);

    // Convert back from planar to interleaved format
    float* out_samples = (float*)out_info.data;
    for(int ch = 0; ch < channels; ch++)
    {
      for(gsize i = 0; i < n_frames; i++)
      {
        out_samples[i * channels + ch] = output_channels[ch][i];
      }
    }

    GST_DEBUG_OBJECT(this, "Audio processing completed via process_adapter");

    gst_buffer_unmap(inbuf, &in_info);
    gst_buffer_unmap(outbuf, &out_info);
    return GST_FLOW_OK;
  }

  // Initialize processor buffers when caps are set
  gboolean set_caps(GstCaps* incaps, GstCaps* outcaps)
  {
    GstAudioInfo audio_info;
    if(!gst_audio_info_from_caps(&audio_info, incaps))
      return FALSE;

    int channels = GST_AUDIO_INFO_CHANNELS(&audio_info);
    int sample_rate = GST_AUDIO_INFO_RATE(&audio_info);
    int frames_per_buffer = 1024; // Default, will be updated in transform()

    // Initialize process adapter buffers
    avnd::process_setup setup_info{
        .input_channels = channels,
        .output_channels = channels,
        .frames_per_buffer = frames_per_buffer,
        .rate = (double)sample_rate};
    processor.allocate_buffers(setup_info, float{});

    // Initialize Avendish effect
    avnd::prepare(impl, setup_info);

    return TRUE;
  }

  // Stub caps negotiation methods (audio doesn't change format)
  GstCaps* transform_caps(GstPadDirection direction, GstCaps* caps, GstCaps* filter)
  {
    return gst_caps_ref(caps);
  }

  GstCaps* fixate_caps(GstPadDirection direction, GstCaps* caps, GstCaps* othercaps)
  {
    return gst_caps_fixate(gst_caps_copy(othercaps));
  }
};

template <typename T>
  requires(is_audio_filter<T>())
struct metaclass<T>
{
  using type = T;
  static inline GstDebugCategory* debug_category = nullptr;

  static inline GstStaticPadTemplate sink_pad_template = GST_STATIC_PAD_TEMPLATE(
      "sink", GST_PAD_SINK, GST_PAD_ALWAYS,
      GST_STATIC_CAPS("audio/x-raw,format=F32LE,channels=[1,8],rate=[8000,192000]"));

  static inline GstStaticPadTemplate src_pad_template = GST_STATIC_PAD_TEMPLATE(
      "src", GST_PAD_SRC, GST_PAD_ALWAYS,
      GST_STATIC_CAPS("audio/x-raw,format=F32LE,channels=[1,8],rate=[8000,192000]"));

  GstBaseTransformClass the_class;

  void init()
  {
    auto klass = this;
    GObjectClass* gobject_class = G_OBJECT_CLASS(klass);
    GstBaseTransformClass* base_transform_class = GST_BASE_TRANSFORM_CLASS(klass);

    gst_element_class_add_static_pad_template(
        GST_ELEMENT_CLASS(klass), &sink_pad_template);
    gst_element_class_add_static_pad_template(
        GST_ELEMENT_CLASS(klass), &src_pad_template);

    init_properties(*this, "Filter/Effect/Audio");

    // Transform API
    base_transform_class->transform = GST_DEBUG_FUNCPTR(
        +[](GstBaseTransform* gobject, GstBuffer* inbuf,
            GstBuffer* outbuf) -> GstFlowReturn {
      return ((element<T>*)gobject)->transform(inbuf, outbuf);
    });

    // Caps negotiation for texture processing
    base_transform_class->transform_caps = GST_DEBUG_FUNCPTR(
        +[](GstBaseTransform* gobject, GstPadDirection direction, GstCaps* caps,
            GstCaps* filter) -> GstCaps* {
      return ((element<T>*)gobject)->transform_caps(direction, caps, filter);
    });

    base_transform_class->fixate_caps = GST_DEBUG_FUNCPTR(
        +[](GstBaseTransform* gobject, GstPadDirection direction, GstCaps* caps,
            GstCaps* othercaps) -> GstCaps* {
      return ((element<T>*)gobject)->fixate_caps(direction, caps, othercaps);
    });

    base_transform_class->set_caps = GST_DEBUG_FUNCPTR(
        +[](GstBaseTransform* gobject, GstCaps* incaps, GstCaps* outcaps) -> gboolean {
      return ((element<T>*)gobject)->set_caps(incaps, outcaps);
    });
  }

  static GType get_type()
  {
    static const GType tid = get_type_once<T>(gst_base_transform_get_type());
    return tid;
  }
};
}

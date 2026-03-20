#pragma once

#include <avnd/binding/gstreamer/utils.hpp>
#include <avnd/wrappers/process_adapter.hpp>

namespace gst
{

// Audio sink element for objects that consume audio data
template <typename T>
  requires(is_audio_sink<T>())
struct element<T> : property_handler
{
  GstBaseSink the_object; // MUST be first for GObject
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
    return TRUE;
  }

  gboolean set_caps(GstCaps* caps)
  {
    GST_DEBUG_OBJECT(this, "set_caps");

    GstAudioInfo audio_info;
    if(!gst_audio_info_from_caps(&audio_info, caps))
      return FALSE;

    int channels = GST_AUDIO_INFO_CHANNELS(&audio_info);
    int sample_rate = GST_AUDIO_INFO_RATE(&audio_info);
    int frames_per_buffer = 1024; // Default, will be updated in render()

    // Initialize process adapter buffers
    avnd::process_setup setup_info{
        .input_channels = channels,
        .output_channels = 0, // Sink has no outputs
        .frames_per_buffer = frames_per_buffer,
        .rate = (double)sample_rate};
    processor.allocate_buffers(setup_info, float{});

    // Initialize Avendish effect
    avnd::prepare(impl, setup_info);

    return TRUE;
  }

  GstFlowReturn render(GstBuffer* buffer)
  {
    GST_DEBUG_OBJECT(this, "render");

    // Get audio format info from caps
    GstCaps* caps = gst_pad_get_current_caps(GST_BASE_SINK_PAD(this));
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

    GstMapInfo info;
    if(!gst_buffer_map(buffer, &info, GST_MAP_READ))
    {
      GST_ERROR_OBJECT(this, "Failed to map buffer");
      return GST_FLOW_ERROR;
    }

    // Get audio parameters
    int channels = GST_AUDIO_INFO_CHANNELS(&audio_info);
    int sample_rate = GST_AUDIO_INFO_RATE(&audio_info);
    GstAudioFormat format = GST_AUDIO_INFO_FORMAT(&audio_info);
    int bps = GST_AUDIO_INFO_BPF(&audio_info); // bytes per frame
    gsize n_frames = info.size / bps;

    GST_DEBUG_OBJECT(
        this, "Processing %lu frames: %d channels, %d Hz, format=%s, bps=%d", n_frames,
        channels, sample_rate, gst_audio_format_to_string(format), bps);

    if constexpr(avnd::audio_bus_input_introspection<T>::size > 0)
    {
      // Use process_adapter for Avendish effects with audio inputs
      std::vector<float*> input_ptrs(channels);
      std::vector<std::vector<float>> input_channels(channels);

      // Allocate input channel arrays and convert from interleaved to planar
      for(int ch = 0; ch < channels; ch++)
      {
        input_channels[ch].resize(n_frames);
        input_ptrs[ch] = input_channels[ch].data();
      }

      // Convert from interleaved to planar format
      if(format == GST_AUDIO_FORMAT_F32LE)
      {
        float* samples = (float*)info.data;
        for(int ch = 0; ch < channels; ch++)
        {
          for(gsize i = 0; i < n_frames; i++)
          {
            input_channels[ch][i] = samples[i * channels + ch];
          }
        }
      }
      else if(format == GST_AUDIO_FORMAT_F64LE)
      {
        double* samples = (double*)info.data;
        for(int ch = 0; ch < channels; ch++)
        {
          for(gsize i = 0; i < n_frames; i++)
          {
            input_channels[ch][i] = samples[i * channels + ch];
          }
        }
      }
      else if(format == GST_AUDIO_FORMAT_S16LE)
      {
        int16_t* samples = (int16_t*)info.data;
        for(int ch = 0; ch < channels; ch++)
        {
          for(gsize i = 0; i < n_frames; i++)
          {
            input_channels[ch][i] = samples[i * channels + ch] / 32768.0;
          }
        }
      }
      else
      {
        GST_WARNING_OBJECT(
            this, "Unsupported audio format: %s", gst_audio_format_to_string(format));
        // Fill with silence for unsupported formats
        for(int ch = 0; ch < channels; ch++)
        {
          std::fill(input_channels[ch].begin(), input_channels[ch].end(), 0.0f);
        }
      }

      // Process through process_adapter (no outputs for sinks)
      processor.process(
          impl, avnd::span<float*>{input_ptrs.data(), (size_t)channels},
          avnd::span<float*>{}, // Empty output span for sink
          n_frames);
    }

    gst_buffer_unmap(buffer, &info);
    return GST_FLOW_OK;
  }
};

// Audio sink metaclass for audio sink elements
template <typename T>
  requires(is_audio_sink<T>())
struct metaclass<T>
{
  using type = T;
  static inline GstDebugCategory* debug_category = nullptr;

  static inline GstStaticPadTemplate sink_pad_template = GST_STATIC_PAD_TEMPLATE(
      "sink", GST_PAD_SINK, GST_PAD_ALWAYS,
      GST_STATIC_CAPS(
          "audio/x-raw,format={F32LE,F64LE,S16LE},channels=[1,8],rate=[8000,192000]"));

  GstBaseSinkClass the_class;

  void init()
  {
    auto klass = this;
    GObjectClass* gobject_class = G_OBJECT_CLASS(klass);
    GstBaseSinkClass* base_sink_class = GST_BASE_SINK_CLASS(klass);

    gst_element_class_add_static_pad_template(
        GST_ELEMENT_CLASS(klass), &sink_pad_template);

    init_properties(*this, "Sink/Audio");

    // Sink API
    base_sink_class->start = GST_DEBUG_FUNCPTR(+[](GstBaseSink* gobject) -> gboolean {
      return ((element<T>*)gobject)->start();
    });

    base_sink_class->set_caps
        = GST_DEBUG_FUNCPTR(+[](GstBaseSink* gobject, GstCaps* caps) -> gboolean {
      return ((element<T>*)gobject)->set_caps(caps);
    });

    base_sink_class->render = GST_DEBUG_FUNCPTR(
        +[](GstBaseSink* gobject, GstBuffer* buffer) -> GstFlowReturn {
      return ((element<T>*)gobject)->render(buffer);
    });
  }

  static GType get_type()
  {
    static const GType tid = get_type_once<T>(gst_base_sink_get_type());
    return tid;
  }
};

}

#pragma once

#include <avnd/binding/gstreamer/utils.hpp>
#include <vector>

namespace gst
{

// Audio filter element for objects with audio input and output
template <typename T>
  requires(is_audio_filter<T>())
struct element<T>
{
  GstBaseTransform the_object; // MUST be first for GObject
  avnd::effect_container<T> impl;

  element() { }
  ~element() { }

  void set_property(guint property_id, const GValue* value, GParamSpec* pspec)
  {
    GST_DEBUG_OBJECT(this, "set_property");

    if(property_id == 0)
    {
      G_OBJECT_WARN_INVALID_PROPERTY_ID(this, property_id, pspec);
      return;
    }

    avnd::parameter_input_introspection<T>::for_nth_mapped(
        avnd::get_inputs(impl), property_id - 1, gst::set_property{value, pspec});
  }

  void get_property(guint property_id, GValue* value, GParamSpec* pspec)
  {
    GST_DEBUG_OBJECT(this, "get_property");

    if(property_id == 0)
    {
      G_OBJECT_WARN_INVALID_PROPERTY_ID(this, property_id, pspec);
      return;
    }

    avnd::parameter_input_introspection<T>::for_nth_mapped(
        avnd::get_inputs(impl), property_id - 1, gst::get_property{value, pspec});
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
        this, "Processing %lu frames: %d channels, %d Hz, format=%s, bps=%d",
        n_frames, channels, sample_rate, gst_audio_format_to_string(format), bps);

    // Set up audio input and output data for Avendish
    auto& inputs = avnd::get_inputs(impl);
    auto& outputs = avnd::get_outputs(impl);

    // Handle different Avendish audio processing patterns
    if constexpr(avnd::polyphonic_arg_audio_effect<double, T>)
    {
      // Raw double** API processing
      GST_DEBUG_OBJECT(this, "Processing raw double** audio API");
      
      // Allocate temporary channel arrays for double processing
      std::vector<std::vector<double>> input_channels(channels);
      std::vector<std::vector<double>> output_channels(channels);
      std::vector<double*> input_ptrs(channels);
      std::vector<double*> output_ptrs(channels);
      
      for(int ch = 0; ch < channels; ch++)
      {
        input_channels[ch].resize(n_frames);
        output_channels[ch].resize(n_frames);
        input_ptrs[ch] = input_channels[ch].data();
        output_ptrs[ch] = output_channels[ch].data();
      }
      
      // Convert from interleaved float to planar double
      float* samples = (float*)in_info.data;
      for(int ch = 0; ch < channels; ch++)
      {
        for(gsize i = 0; i < n_frames; i++)
        {
          input_channels[ch][i] = samples[i * channels + ch];
        }
      }
      
      // Process through Avendish effect
      impl.effect(input_ptrs.data(), output_ptrs.data(), n_frames);
      
      // Convert back from planar double to interleaved float
      float* out_samples = (float*)out_info.data;
      for(int ch = 0; ch < channels; ch++)
      {
        for(gsize i = 0; i < n_frames; i++)
        {
          out_samples[i * channels + ch] = output_channels[ch][i];
        }
      }
      
      GST_DEBUG_OBJECT(this, "Raw double** audio processing completed");
    }
    else if constexpr(avnd::polyphonic_arg_audio_effect<float, T>)
    {
      // Raw float** API processing
      GST_DEBUG_OBJECT(this, "Processing raw float** audio API");
      
      // Allocate temporary channel arrays for float processing
      std::vector<std::vector<float>> input_channels(channels);
      std::vector<std::vector<float>> output_channels(channels);
      std::vector<float*> input_ptrs(channels);
      std::vector<float*> output_ptrs(channels);
      
      for(int ch = 0; ch < channels; ch++)
      {
        input_channels[ch].resize(n_frames);
        output_channels[ch].resize(n_frames);
        input_ptrs[ch] = input_channels[ch].data();
        output_ptrs[ch] = output_channels[ch].data();
      }
      
      // Convert from interleaved to planar float
      float* samples = (float*)in_info.data;
      for(int ch = 0; ch < channels; ch++)
      {
        for(gsize i = 0; i < n_frames; i++)
        {
          input_channels[ch][i] = samples[i * channels + ch];
        }
      }
      
      // Process through Avendish effect
      impl.effect(input_ptrs.data(), output_ptrs.data(), n_frames);
      
      // Convert back from planar to interleaved float
      float* out_samples = (float*)out_info.data;
      for(int ch = 0; ch < channels; ch++)
      {
        for(gsize i = 0; i < n_frames; i++)
        {
          out_samples[i * channels + ch] = output_channels[ch][i];
        }
      }
      
      GST_DEBUG_OBJECT(this, "Raw float** audio processing completed");
    }
    else if constexpr(avnd::audio_bus_input_introspection<T>::size > 0 && avnd::audio_bus_output_introspection<T>::size > 0)
    {
      // Audio bus API processing - only handle halp::dynamic_audio_bus with .channel() method
      GST_DEBUG_OBJECT(this, "Processing audio bus API");
      
      // Validate format (currently only F32LE supported)
      if(format != GST_AUDIO_FORMAT_F32LE)
      {
        GST_ERROR_OBJECT(
            this, "Unsupported audio format: %s (only F32LE supported)",
            gst_audio_format_to_string(format));
        gst_buffer_unmap(inbuf, &in_info);
        gst_buffer_unmap(outbuf, &out_info);
        return GST_FLOW_ERROR;
      }

      // Check if all input buses support the .channel() method (halp::dynamic_audio_bus)
      bool all_inputs_supported = true;
      avnd::audio_bus_input_introspection<T>::for_all(
          inputs, [&all_inputs_supported]<typename Field>(Field& field) {
        if constexpr(!requires { field.channel(0, 0); })
        {
          all_inputs_supported = false;
        }
      });

      bool all_outputs_supported = true;
      avnd::audio_bus_output_introspection<T>::for_all(
          outputs, [&all_outputs_supported]<typename Field>(Field& field) {
        if constexpr(!requires { field.channel(0, 0); })
        {
          all_outputs_supported = false;
        }
      });

      if(!all_inputs_supported || !all_outputs_supported)
      {
        GST_ERROR_OBJECT(
            this, "Effect uses raw audio bus API which is not supported by GStreamer binding. Use halp::dynamic_audio_bus or polyphonic_arg_audio_effect instead.");
        gst_buffer_unmap(inbuf, &in_info);
        gst_buffer_unmap(outbuf, &out_info);
        return GST_FLOW_ERROR;
      }

      // Set up input audio buses (halp::dynamic_audio_bus only)
      avnd::audio_bus_input_introspection<T>::for_all(
          inputs, [&]<typename Field>(Field& field) {
        float* samples = (float*)in_info.data;
        int max_channels = std::min(channels, field.channels);
        for(int ch = 0; ch < max_channels; ch++)
        {
          auto channel_span = field.channel(ch, n_frames);
          // Interleaved to planar conversion
          for(gsize i = 0; i < n_frames && i < channel_span.size(); i++)
          {
            channel_span[i] = samples[i * channels + ch];
          }
        }
      });

      // Process audio through Avendish effect
      if constexpr(avnd::tag_single_exec<T>)
        impl.effect();
      else
        impl.effect.operator()();

      // Convert output audio data back to interleaved format (halp::dynamic_audio_bus only)
      avnd::audio_bus_output_introspection<T>::for_all(
          outputs, [&]<typename Field>(Field& field) {
        float* samples = (float*)out_info.data;
        int max_channels = std::min(channels, field.channels);
        for(int ch = 0; ch < max_channels; ch++)
        {
          auto channel_span = field.channel(ch, n_frames);
          // Planar to interleaved conversion
          for(gsize i = 0; i < std::min(n_frames, channel_span.size()); i++)
          {
            samples[i * channels + ch] = channel_span[i];
          }
        }
      });

      GST_DEBUG_OBJECT(this, "Audio bus processing completed");
    }
    else
    {
      // No recognized audio processing API, fallback to simple passthrough
      GST_DEBUG_OBJECT(this, "No recognized audio processing API, using passthrough");
      memcpy(out_info.data, in_info.data, in_info.size);
    }

    gst_buffer_unmap(inbuf, &in_info);
    gst_buffer_unmap(outbuf, &out_info);
    return GST_FLOW_OK;
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

  gboolean set_caps(GstCaps* incaps, GstCaps* outcaps) { return TRUE; }
};

template <typename T>
  requires(is_audio_filter<T>())
struct metaclass<T>
{
  static inline GstDebugCategory* debug_category = nullptr;

  static inline GstStaticPadTemplate sink_pad_template = GST_STATIC_PAD_TEMPLATE(
      "sink", GST_PAD_SINK, GST_PAD_ALWAYS,
      GST_STATIC_CAPS("audio/x-raw,format=F32LE,channels=[1,8],rate=[8000,192000]"));

  static inline GstStaticPadTemplate src_pad_template = GST_STATIC_PAD_TEMPLATE(
      "src", GST_PAD_SRC, GST_PAD_ALWAYS,
      GST_STATIC_CAPS("audio/x-raw,format=F32LE,channels=[1,8],rate=[8000,192000]"));

  GstBaseTransformClass the_class;

  metaclass()
  {
    auto klass = this;
    GObjectClass* gobject_class = G_OBJECT_CLASS(klass);
    GstBaseTransformClass* base_transform_class = GST_BASE_TRANSFORM_CLASS(klass);

    gst_element_class_add_static_pad_template(
        GST_ELEMENT_CLASS(klass), &sink_pad_template);
    gst_element_class_add_static_pad_template(
        GST_ELEMENT_CLASS(klass), &src_pad_template);

    static constexpr auto c_name = avnd::get_static_symbol<T>();
    static std::string author = avnd::get_author<T>();
    if constexpr(!avnd::has_author<T>)
      author = "John Doe";
    static std::string_view desc = avnd::get_description<T>();
    if constexpr(!avnd::has_description<T>)
      desc = "Audio Filter";

    gst_element_class_set_static_metadata(
        GST_ELEMENT_CLASS(klass), std::string_view{c_name}.data(), "Filter/Effect/Audio",
        desc.data(), author.c_str());

    // GObject API
    gobject_class->set_property = +[](GObject* object, guint property_id,
                                      const GValue* value, GParamSpec* pspec) -> void {
      ((element<T>*)object)->set_property(property_id, value, pspec);
    };
    gobject_class->get_property = +[](GObject* object, guint property_id, GValue* value,
                                      GParamSpec* pspec) -> void {
      ((element<T>*)object)->get_property(property_id, value, pspec);
    };
    gobject_class->dispose = GST_DEBUG_FUNCPTR(+[](GObject* gobject) -> void {
      std::destroy_at((element<T>*)GST_BASE_TRANSFORM(gobject));
      G_OBJECT_CLASS(parent_class)->dispose(gobject);
    });

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

    if constexpr(avnd::parameter_input_introspection<T>::size > 0)
    {
      avnd::parameter_input_introspection<T>::for_all(
          [gobject_class]<std::size_t I, typename F>(avnd::field_reflection<I, F> f) {
        static constexpr auto idx
            = avnd::parameter_input_introspection<T>::template unmap<I>();

        auto param = param_spec<F>();
        if(!param)
        {
          static constexpr auto symname = avnd::get_static_symbol<F>();
          static constexpr std::string_view name = avnd::get_name<F>();
          static constexpr std::string_view desc = avnd::get_description<F>();

          param = g_param_spec_string(
              symname.data(), name.data(), desc.data(), "",
              static_cast<GParamFlags>(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
        }
        g_object_class_install_property(gobject_class, idx + 1, param);
      });
    }
  }

  static GType get_type_once()
  {
    static constexpr GClassInitFunc class_intern_init_ptr
        = +[](gpointer klass, gpointer class_data) {
      parent_class = g_type_class_peek_parent(klass);
      std::construct_at((metaclass*)klass);
    };

    static constexpr GInstanceInitFunc instance_init_ptr
        = +[](GTypeInstance* ptr, void* cls) { std::construct_at((element<T>*)ptr); };

    static constexpr auto c_name = avnd::get_static_symbol<T>();

    const GType tid = g_type_register_static_simple(
        gst_base_transform_get_type(),
        g_intern_static_string(std::string_view{c_name}.data()), sizeof(metaclass),
        class_intern_init_ptr, sizeof(element<T>), (GInstanceInitFunc)instance_init_ptr,
        (GTypeFlags)0);

    debug_category = _gst_debug_category_new(
        c_name.data(), 0, "debug category for audio filter element");

    return tid;
  };

  static GType get_type()
  {
    static const GType tid = get_type_once();
    return tid;
  }
};
}

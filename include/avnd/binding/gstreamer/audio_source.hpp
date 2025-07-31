#pragma once

#include <avnd/binding/gstreamer/utils.hpp>
#include <avnd/wrappers/process_adapter.hpp>
#include <cmath>

namespace gst
{

// Audio source element for objects that generate audio data
template <typename T>
  requires(is_audio_generator<T>())
struct element<T>
{
  GstPushSrc the_object; // MUST be first for GObject
  avnd::effect_container<T> impl;
  avnd::process_adapter<T> processor;

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
        .rate = 48000.0
    };
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
          impl,
          avnd::span<float*>{}, // Empty input span for generator
          avnd::span<float*>{output_ptrs.data(), (size_t)channels},
          n_frames
      );
      
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
      // No Avendish audio output, generate a simple test tone
      float* samples = (float*)info.data;
      static double phase = 0.0;
      double freq = 440.0; // A4 note
      double phase_increment = 2.0 * M_PI * freq / sample_rate;
      
      for(gsize i = 0; i < n_frames; i++)
      {
        float sample = 0.1 * sin(phase);
        samples[i * channels] = sample;     // Left
        samples[i * channels + 1] = sample; // Right
        phase += phase_increment;
        if(phase >= 2.0 * M_PI) phase -= 2.0 * M_PI;
      }
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
  static inline GstDebugCategory* debug_category = nullptr;

  static inline GstStaticPadTemplate src_pad_template = GST_STATIC_PAD_TEMPLATE(
      "src", GST_PAD_SRC, GST_PAD_ALWAYS,
      GST_STATIC_CAPS(
          "audio/x-raw,format={F32LE,F64LE,S16LE},channels=[1,8],rate=[8000,192000]"));

  GstPushSrcClass the_class;

  metaclass()
  {
    auto klass = this;
    GObjectClass* gobject_class = G_OBJECT_CLASS(klass);
    GstPushSrcClass* push_src_class = GST_PUSH_SRC_CLASS(klass);
    GstBaseSrcClass* base_src_class = GST_BASE_SRC_CLASS(klass);

    gst_element_class_add_static_pad_template(GST_ELEMENT_CLASS(klass), &src_pad_template);

    static constexpr auto c_name = avnd::get_static_symbol<T>();
    static std::string author = avnd::get_author<T>();
    if constexpr(!avnd::has_author<T>)
      author = "John Doe";
    static std::string_view desc = avnd::get_description<T>();
    if constexpr(!avnd::has_description<T>)
      desc = "Audio Generator";

    g_print("instantiate");
    gst_element_class_set_static_metadata(
        GST_ELEMENT_CLASS(klass), std::string_view{c_name}.data(), "Source/Audio",
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
      std::destroy_at((element<T>*)GST_PUSH_SRC(gobject));

      G_OBJECT_CLASS(parent_class)->dispose(gobject);
    });

    // GStreamer API
    base_src_class->start = GST_DEBUG_FUNCPTR(+[](GstBaseSrc* gobject) -> gboolean {
      return ((element<T>*)gobject)->start();
    });

    push_src_class->fill
        = GST_DEBUG_FUNCPTR(+[](GstPushSrc* gobject, GstBuffer* buf) -> GstFlowReturn {
      return ((element<T>*)gobject)->fill(buf);
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
        gst_push_src_get_type(), g_intern_static_string(std::string_view{c_name}.data()),
        sizeof(metaclass), class_intern_init_ptr, sizeof(element<T>),
        (GInstanceInitFunc)instance_init_ptr, (GTypeFlags)0);

    debug_category
        = _gst_debug_category_new(c_name.data(), 0, "debug category for element");

    return tid;
  };

  static GType get_type()
  {
    static const GType tid = get_type_once();
    return tid;
  }
};
}

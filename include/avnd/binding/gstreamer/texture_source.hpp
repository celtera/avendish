#pragma once

#include <avnd/binding/gstreamer/utils.hpp>

namespace gst
{

// Texture generator element for objects that generate textures/video
template <typename T>
  requires(is_texture_generator<T>())
struct element<T> : property_handler
{
  GstPushSrc the_object; // MUST be first for GObject
  avnd::effect_container<T> impl;
  
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
    // Set up fixed caps for RGBA video
    GstCaps* caps = gst_caps_new_simple(
        "video/x-raw", "format", G_TYPE_STRING, "RGBA", "width", G_TYPE_INT, 480,
        "height", G_TYPE_INT, 270, "framerate", GST_TYPE_FRACTION, 30, 1, NULL);
    gst_base_src_set_caps(GST_BASE_SRC(this), caps);
    gst_caps_unref(caps);

    // Set buffer size for RGBA 480x270
    gst_base_src_set_blocksize(GST_BASE_SRC(this), 480 * 270 * 4);
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

    // Fixed dimensions (should match caps from start())
    int width = 480;
    int height = 270;
    gsize expected_size = width * height * 4; // RGBA

    GST_DEBUG_OBJECT(this, "Generating %dx%d texture, buffer size: %lu", width, height, info.size);

    // Set up texture output data for Avendish
    auto& outputs = avnd::get_outputs(impl);

    if constexpr(avnd::texture_output_introspection<T>::size > 0)
    {
      avnd::texture_output_introspection<T>::for_all(
          outputs, [&]<typename Field>(Field& field) {
        // Set up output texture with proper dimensions
        field.texture.width = width;
        field.texture.height = height;
        field.texture.changed = true;

        // Allocate buffer if needed (let Avendish manage it)
        if(!field.texture.bytes || field.texture.width * field.texture.height * 4 != expected_size)
        {
          // Avendish will handle allocation via halp::texture
        }
      });

      // Generate texture through Avendish effect
      if constexpr(avnd::tag_single_exec<T>)
        impl.effect();
      else
        impl.effect.operator()();

      // Copy generated texture data to GStreamer buffer
      avnd::texture_output_introspection<T>::for_all(
          outputs, [&]<typename Field>(Field& field) {
        if(field.texture.bytes && field.texture.width * field.texture.height * 4 <= info.size)
        {
          memcpy(info.data, field.texture.bytes, field.texture.width * field.texture.height * 4);
          GST_DEBUG_OBJECT(
              this, "Copied %dx%d texture from Avendish effect", 
              field.texture.width, field.texture.height);
        }
        else
        {
          GST_WARNING_OBJECT(this, "Avendish texture output invalid, using fallback pattern");
          // Fallback to simple pattern if Avendish fails
          uint8_t* data = info.data;
          for(gsize i = 0; i < std::min(expected_size, info.size) && i + 3 < info.size; i += 4)
          {
            data[i] = (i / 4) % 256;      // R
            data[i + 1] = (i / 8) % 256;  // G
            data[i + 2] = (i / 16) % 256; // B
            data[i + 3] = 255;            // A
          }
        }
      });
    }
    else
    {
      // No Avendish texture output, generate a simple gradient pattern
      uint8_t* data = info.data;
      GST_DEBUG_OBJECT(this, "No Avendish texture output, generating fallback pattern");

      for(int y = 0; y < height; y++)
      {
        for(int x = 0; x < width; x++)
        {
          gsize idx = (y * width + x) * 4;
          if(idx + 3 < info.size)
          {
            data[idx] = (x * 255) / width;     // R gradient
            data[idx + 1] = (y * 255) / height; // G gradient
            data[idx + 2] = 128;                // B constant
            data[idx + 3] = 255;                // A opaque
          }
        }
      }
    }

    gst_buffer_unmap(buf, &info);
    GST_DEBUG_OBJECT(this, "fill completed successfully");
    return GST_FLOW_OK;
  }
};

template <typename T>
  requires(is_texture_generator<T>())
struct metaclass<T>
{
  static inline GstDebugCategory* debug_category = nullptr;

  static inline GstStaticPadTemplate src_pad_template = GST_STATIC_PAD_TEMPLATE(
      "src", GST_PAD_SRC, GST_PAD_ALWAYS,
      GST_STATIC_CAPS(
          "video/x-raw,format=RGBA,width=[1,4096],height=[1,4096],framerate=[1/1,120/"
          "1]"));

  GstPushSrcClass the_class;

  metaclass()
  {
    auto klass = this;
    GObjectClass* gobject_class = G_OBJECT_CLASS(klass);
    GstPushSrcClass* push_src_class = GST_PUSH_SRC_CLASS(klass);
    GstBaseSrcClass* base_src_class = GST_BASE_SRC_CLASS(klass);

    gst_element_class_add_static_pad_template(
        GST_ELEMENT_CLASS(klass), &src_pad_template);

    static constexpr auto c_name = avnd::get_static_symbol<T>();
    static std::string author = avnd::get_author<T>();
    if constexpr(!avnd::has_author<T>)
      author = "John Doe";
    static std::string_view desc = avnd::get_description<T>();
    if constexpr(!avnd::has_description<T>)
      desc = "Texture Generator";

    gst_element_class_set_static_metadata(
        GST_ELEMENT_CLASS(klass), std::string_view{c_name}.data(), "Source/Video",
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

    debug_category = _gst_debug_category_new(
        c_name.data(), 0, "debug category for texture generator element");

    return tid;
  };

  static GType get_type()
  {
    static const GType tid = get_type_once();
    return tid;
  }
};

}

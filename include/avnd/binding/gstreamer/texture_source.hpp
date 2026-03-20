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
    // Don't set fixed caps here - let negotiation happen
    return TRUE;
  }

  gboolean negotiate()
  {
    GST_DEBUG_OBJECT(this, "negotiate");

    // First, check if the Avendish effect has an initialized output
    auto& outputs = avnd::get_outputs(impl);
    int desired_width = 480;  // Default
    int desired_height = 270; // Default

    avnd::texture_output_introspection<T>::for_all(
        outputs, [&]<typename Field>(Field& field) {
      if(field.texture.width > 0 && field.texture.height > 0)
      {
        desired_width = field.texture.width;
        desired_height = field.texture.height;
        GST_DEBUG_OBJECT(
            this, "Using Avendish effect size: %dx%d", desired_width, desired_height);
      }
    });

    // Create caps for the determined size
    GstCaps* caps = gst_caps_new_simple(
        "video/x-raw", "format", G_TYPE_STRING, "RGBA", "width", G_TYPE_INT,
        desired_width, "height", G_TYPE_INT, desired_height, "framerate",
        GST_TYPE_FRACTION, 30, 1, NULL);

    // Set the caps
    gboolean result = gst_base_src_set_caps(GST_BASE_SRC(this), caps);
    if(result)
    {
      // Update block size for the new dimensions
      gst_base_src_set_blocksize(GST_BASE_SRC(this), desired_width * desired_height * 4);
    }

    gst_caps_unref(caps);
    return result;
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

    // Get current caps to determine size
    GstCaps* caps = gst_pad_get_current_caps(GST_BASE_SRC_PAD(this));
    if(!caps)
    {
      // Negotiate if we don't have caps yet
      if(!negotiate())
      {
        GST_ERROR_OBJECT(this, "Failed to negotiate caps");
        gst_buffer_unmap(buf, &info);
        return GST_FLOW_NOT_NEGOTIATED;
      }
      caps = gst_pad_get_current_caps(GST_BASE_SRC_PAD(this));
    }

    if(!caps)
    {
      GST_ERROR_OBJECT(this, "No caps available after negotiation");
      gst_buffer_unmap(buf, &info);
      return GST_FLOW_ERROR;
    }

    // Extract dimensions from caps
    GstVideoInfo video_info;
    if(!gst_video_info_from_caps(&video_info, caps))
    {
      GST_ERROR_OBJECT(this, "Failed to get video info from caps");
      gst_caps_unref(caps);
      gst_buffer_unmap(buf, &info);
      return GST_FLOW_ERROR;
    }
    gst_caps_unref(caps);

    int current_width = GST_VIDEO_INFO_WIDTH(&video_info);
    int current_height = GST_VIDEO_INFO_HEIGHT(&video_info);

    GST_DEBUG_OBJECT(
        this, "Generating %dx%d texture, buffer size: %lu", current_width,
        current_height, info.size);

    // Set up texture output data for Avendish
    auto& outputs = avnd::get_outputs(impl);
    gsize expected_size = current_width * current_height * 4; // RGBA

    if constexpr(avnd::texture_output_introspection<T>::size > 0)
    {
      // Generate texture through Avendish effect
      if constexpr(avnd::tag_single_exec<T>)
        impl.effect();
      else
        impl.effect.operator()();

      // Check if the effect changed its output size
      bool size_changed = false;
      int new_width = current_width;
      int new_height = current_height;

      avnd::texture_output_introspection<T>::for_all(
          outputs, [&]<typename Field>(Field& field) {
        if(field.texture.width != current_width
           || field.texture.height != current_height)
        {
          new_width = field.texture.width;
          new_height = field.texture.height;
          size_changed = true;
        }
      });

      // Handle dynamic size changes
      if(size_changed && new_width > 0 && new_height > 0)
      {
        GST_DEBUG_OBJECT(
            this, "Effect changed output size: %dx%d -> %dx%d", current_width,
            current_height, new_width, new_height);

        // Renegotiate with new size
        gst_buffer_unmap(buf, &info);
        if(!negotiate())
        {
          GST_ERROR_OBJECT(this, "Failed to renegotiate for new size");
          return GST_FLOW_NOT_NEGOTIATED;
        }

        // Return and let GStreamer allocate a new buffer with correct size
        return GST_FLOW_OK;
      }

      // Copy generated texture data to GStreamer buffer
      avnd::texture_output_introspection<T>::for_all(
          outputs, [&]<typename Field>(Field& field) {
        if(field.texture.bytes
           && field.texture.width * field.texture.height * 4 <= info.size)
        {
          memcpy(
              info.data, field.texture.bytes,
              field.texture.width * field.texture.height * 4);
          GST_DEBUG_OBJECT(
              this, "Copied %dx%d texture from Avendish effect", field.texture.width,
              field.texture.height);
        }
        else
        {
          GST_WARNING_OBJECT(
              this, "Avendish texture output invalid, using fallback pattern");
          // Fallback to simple pattern if Avendish fails
          uint8_t* data = info.data;
          for(gsize i = 0; i < std::min(expected_size, info.size) && i + 3 < info.size;
              i += 4)
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
      GST_DEBUG_OBJECT(this, "No Avendish texture output!");
      uint8_t* data = info.data;
      std::fill_n(data, info.size, 0);
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
  using type = T;
  static inline GstDebugCategory* debug_category = nullptr;

  static inline GstStaticPadTemplate src_pad_template = GST_STATIC_PAD_TEMPLATE(
      "src", GST_PAD_SRC, GST_PAD_ALWAYS,
      GST_STATIC_CAPS(
          "video/x-raw,format=RGBA,width=[1,MAX],height=[1,MAX],framerate=(fraction)[0/"
          "1,MAX]"));

  GstPushSrcClass the_class;

  void init()
  {
    auto klass = this;
    GObjectClass* gobject_class = G_OBJECT_CLASS(klass);
    GstPushSrcClass* push_src_class = GST_PUSH_SRC_CLASS(klass);
    GstBaseSrcClass* base_src_class = GST_BASE_SRC_CLASS(klass);

    gst_element_class_add_static_pad_template(
        GST_ELEMENT_CLASS(klass), &src_pad_template);

    init_properties(*this, "Source/Video");

    // GStreamer API
    base_src_class->start = GST_DEBUG_FUNCPTR(+[](GstBaseSrc* gobject) -> gboolean {
      return ((element<T>*)gobject)->start();
    });

    push_src_class->fill
        = GST_DEBUG_FUNCPTR(+[](GstPushSrc* gobject, GstBuffer* buf) -> GstFlowReturn {
      return ((element<T>*)gobject)->fill(buf);
    });

    base_src_class->negotiate = GST_DEBUG_FUNCPTR(+[](GstBaseSrc* gobject) -> gboolean {
      return ((element<T>*)gobject)->negotiate();
    });
  }

  static GType get_type()
  {
    static const GType tid = get_type_once<T>(gst_push_src_get_type());
    return tid;
  }
};

}

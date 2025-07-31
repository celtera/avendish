#pragma once

#include <avnd/binding/gstreamer/utils.hpp>

namespace gst
{

// Texture sink element for objects that consume textures

template <typename T>
  requires(is_texture_sink<T>())
struct element<T> : property_handler
{
  GstBaseSink the_object; // MUST be first for GObject
  avnd::effect_container<T> impl;

  void init() { }
  using effect_type = T;

  void set_property(guint property_id, const GValue* value, GParamSpec* pspec)
  {
    property_handler::set_property(property_id, value, pspec);
  }

  void get_property(guint property_id, GValue* value, GParamSpec* pspec)
  {
    property_handler::get_property(property_id, value, pspec);
  }

  GstFlowReturn render(GstBuffer* buffer)
  {
    GST_DEBUG_OBJECT(this, "render");

    // Get video format info from caps
    GstCaps* caps = gst_pad_get_current_caps(GST_BASE_SINK_PAD(this));
    if(!caps)
    {
      GST_ERROR_OBJECT(this, "No caps available");
      return GST_FLOW_ERROR;
    }

    GstVideoInfo video_info;
    if(!gst_video_info_from_caps(&video_info, caps))
    {
      GST_ERROR_OBJECT(this, "Failed to get video info from caps");
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

    // Get video dimensions and format info
    int width = GST_VIDEO_INFO_WIDTH(&video_info);
    int height = GST_VIDEO_INFO_HEIGHT(&video_info);
    GstVideoFormat format = GST_VIDEO_INFO_FORMAT(&video_info);
    int stride = GST_VIDEO_INFO_PLANE_STRIDE(&video_info, 0);

    GST_DEBUG_OBJECT(
        this, "Consuming %dx%d texture, format=%s, stride=%d, buffer_size=%lu", width,
        height, gst_video_format_to_string(format), stride, info.size);

    // Set up input texture data for Avendish
    auto& inputs = avnd::get_inputs(impl);

    if constexpr(avnd::texture_input_introspection<T>::size > 0)
    {
      // Validate format and buffer size
      if(format != GST_VIDEO_FORMAT_RGBA)
      {
        GST_ERROR_OBJECT(
            this, "Unexpected format: %s (expected RGBA)",
            gst_video_format_to_string(format));
        gst_buffer_unmap(buffer, &info);
        return GST_FLOW_ERROR;
      }

      size_t expected_min_size = stride * height;
      if(info.size < expected_min_size)
      {
        GST_ERROR_OBJECT(
            this, "Input buffer too small: %lu < %lu", info.size, expected_min_size);
        gst_buffer_unmap(buffer, &info);
        return GST_FLOW_ERROR;
      }

      avnd::texture_input_introspection<T>::for_all(
          inputs, [&]<typename Field>(Field& field) {
        // Handle stride: if buffer has padding, we need to create a tightly packed copy
        if(stride == width * 4)
        {
          // No padding, can use buffer directly
          field.texture.bytes = (unsigned char*)info.data;
        }
        else
        {
          // Has padding, need to create tightly packed buffer
          static std::unique_ptr<uint8_t[]> stride_conversion_buffer;
          static size_t stride_conversion_buffer_size = 0;

          if(!stride_conversion_buffer
             || stride_conversion_buffer_size < width * height * 4)
          {
            stride_conversion_buffer = std::make_unique<uint8_t[]>(width * height * 4);
            stride_conversion_buffer_size = width * height * 4;
          }

          // Copy row by row to remove padding
          uint8_t* src = (uint8_t*)info.data;
          uint8_t* dst = stride_conversion_buffer.get();
          for(int y = 0; y < height; y++)
          {
            memcpy(dst + y * width * 4, src + y * stride, width * 4);
          }

          field.texture.bytes = stride_conversion_buffer.get();
          GST_DEBUG_OBJECT(
              this, "Converted strided input (%d) to tightly packed buffer", stride);
        }

        field.texture.width = width;
        field.texture.height = height;
        field.texture.changed = true;

        GST_DEBUG_OBJECT(
            this, "Input texture setup: %dx%d, %lu bytes, ptr=%p", width, height,
            info.size, info.data);
      });

      // Process texture through Avendish effect (for analysis, recording, etc.)
      if constexpr(avnd::tag_single_exec<T>)
        impl.effect();
      else
        impl.effect.operator()();

      GST_DEBUG_OBJECT(this, "Texture consumed by Avendish effect");
    }
    else
    {
      GST_DEBUG_OBJECT(this, "No Avendish texture input - texture consumed silently");
    }

    gst_buffer_unmap(buffer, &info);
    return GST_FLOW_OK;
  }
};

template <typename T>
  requires(is_texture_sink<T>())
struct metaclass<T>
{
  using type = T;
  static inline GstDebugCategory* debug_category = nullptr;

  static inline GstStaticPadTemplate sink_pad_template = GST_STATIC_PAD_TEMPLATE(
      "sink", GST_PAD_SINK, GST_PAD_ALWAYS,
      GST_STATIC_CAPS(
          "video/x-raw,format=RGBA,width=[1,MAX],height=[1,MAX],framerate=(fraction)[0/"
          "1,MAX]"));

  GstBaseSinkClass the_class;

  void init()
  {
    auto klass = this;
    GObjectClass* gobject_class = G_OBJECT_CLASS(klass);
    GstBaseSinkClass* base_sink_class = GST_BASE_SINK_CLASS(klass);

    gst_element_class_add_static_pad_template(
        GST_ELEMENT_CLASS(klass), &sink_pad_template);

    init_properties(*this, "Sink/Video");

    // Sink API
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

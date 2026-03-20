#pragma once

#include <avnd/binding/gstreamer/utils.hpp>

namespace gst
{

// Texture filter element for objects that process textures
template <typename T>
  requires(is_texture_filter<T>())
struct element<T> : property_handler
{
  GstBaseTransform the_object; // MUST be first for GObject
  avnd::effect_container<T> impl;

  // Buffer for handling stride conversion when input has padding
  std::unique_ptr<uint8_t[]> stride_conversion_buffer;
  size_t stride_conversion_buffer_size = 0;

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

  GstFlowReturn transform(GstBuffer* inbuf, GstBuffer* outbuf)
  {
    GST_DEBUG_OBJECT(this, "transform");

    // Get video format info from caps
    GstCaps* caps = gst_pad_get_current_caps(GST_BASE_TRANSFORM_SINK_PAD(this));
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

    // Get video dimensions and format info
    int width = GST_VIDEO_INFO_WIDTH(&video_info);
    int height = GST_VIDEO_INFO_HEIGHT(&video_info);
    GstVideoFormat format = GST_VIDEO_INFO_FORMAT(&video_info);
    int stride = GST_VIDEO_INFO_PLANE_STRIDE(&video_info, 0);

    GST_DEBUG_OBJECT(
        this, "Processing %dx%d texture, format=%s, stride=%d, buffer_size=%lu", width,
        height, gst_video_format_to_string(format), stride, in_info.size);

    // Set up input texture data for Avendish
    auto& inputs = avnd::get_inputs(impl);
    auto& outputs = avnd::get_outputs(impl);

    // Validate format and buffer size
    if(format != GST_VIDEO_FORMAT_RGBA)
    {
      GST_ERROR_OBJECT(
          this, "Unexpected format: %s (expected RGBA)",
          gst_video_format_to_string(format));
      gst_buffer_unmap(inbuf, &in_info);
      gst_buffer_unmap(outbuf, &out_info);
      return GST_FLOW_ERROR;
    }

    size_t expected_min_size = stride * height;
    if(in_info.size < expected_min_size)
    {
      GST_ERROR_OBJECT(
          this, "Input buffer too small: %lu < %lu", in_info.size, expected_min_size);
      gst_buffer_unmap(inbuf, &in_info);
      gst_buffer_unmap(outbuf, &out_info);
      return GST_FLOW_ERROR;
    }

    // Set up input texture - ensure proper data flow to Avendish get() method
    if constexpr(avnd::texture_input_introspection<T>::size > 0)
    {
      avnd::texture_input_introspection<T>::for_all(
          inputs, [&]<typename Field>(Field& field) {
        // CRITICAL: Ensure texture buffer is valid and accessible
        if(!in_info.data || in_info.size < expected_min_size)
        {
          GST_ERROR_OBJECT(
              this, "Invalid input buffer: ptr=%p, size=%lu, expected=%lu", in_info.data,
              in_info.size, expected_min_size);
          return;
        }

        // Handle stride: if buffer has padding, we need to create a tightly packed copy
        if(stride == width * 4)
        {
          // No padding, can use buffer directly
          field.texture.bytes = (unsigned char*)in_info.data;
        }
        else
        {
          // Has padding, need to create tightly packed buffer
          if(!stride_conversion_buffer
             || stride_conversion_buffer_size < width * height * 4)
          {
            stride_conversion_buffer = std::make_unique<uint8_t[]>(width * height * 4);
            stride_conversion_buffer_size = width * height * 4;
          }

          // Copy row by row to remove padding
          uint8_t* src = (uint8_t*)in_info.data;
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
            in_info.size, in_info.data);

        // ENSURE: Input buffer contains actual data (not all zeros)
        uint8_t* pixels = (uint8_t*)in_info.data;
        int non_zero_count = 0;
        for(size_t i = 0; i < std::min(in_info.size, size_t(100)); i++)
        {
          if(pixels[i] != 0)
            non_zero_count++;
        }
        GST_DEBUG_OBJECT(
            this, "Input validation: %d non-zero bytes in first 100", non_zero_count);

        if(non_zero_count == 0)
        {
          GST_WARNING_OBJECT(
              this,
              "Input buffer appears to be all zeros - this will cause black output");
        }

        // Debug: Sample pixels to verify format
        if(in_info.size >= 16)
        {
          GST_DEBUG_OBJECT(
              this, "Sample pixels - [0]: R=%d G=%d B=%d A=%d, [1]: R=%d G=%d B=%d A=%d",
              pixels[0], pixels[1], pixels[2], pixels[3], pixels[4], pixels[5],
              pixels[6], pixels[7]);
        }
      });
    }

    // Let the Avendish effect create its own output texture
    if constexpr(avnd::texture_output_introspection<T>::size > 0)
    {
      avnd::texture_output_introspection<T>::for_all(
          outputs, [&]<typename Field>(Field& field) {
        // Let the effect create its own output buffer initially
        // We'll copy back to GStreamer buffer after processing
      });
    }

    // ENSURE: Before calling effect, verify input texture is accessible
    if constexpr(avnd::texture_input_introspection<T>::size > 0)
    {
      avnd::texture_input_introspection<T>::for_all(
          inputs, [&]<typename Field>(Field& field) {
        if(field.texture.bytes == nullptr)
        {
          GST_ERROR_OBJECT(
              this,
              "CRITICAL: Input texture bytes is NULL - Avendish effect will return "
              "early!");
        }
        else
        {
          GST_DEBUG_OBJECT(
              this, "Input texture ready: ptr=%p, %dx%d, changed=%d",
              field.texture.bytes, field.texture.width, field.texture.height,
              field.texture.changed);
        }
      });
    }

    // Call the Avendish effect (texture processing doesn't need a tick)
    if constexpr(avnd::tag_single_exec<T>)
      impl.effect();
    else
      impl.effect.operator()();

    // Copy output texture back to GStreamer buffer (handle dynamic size changes)
    if constexpr(avnd::texture_output_introspection<T>::size > 0)
    {
      avnd::texture_output_introspection<T>::for_all(
          outputs, [&]<typename Field>(Field& field) {
        if(field.texture.bytes)
        {
          // Get actual output texture dimensions after effect
          int out_width = field.texture.width;
          int out_height = field.texture.height;

          GST_DEBUG_OBJECT(
              this, "Effect output size: %dx%d (input was %dx%d)", out_width, out_height,
              width, height);

          // Calculate actual output buffer requirements
          size_t required_output_size = out_width * out_height * 4;

          if(required_output_size > out_info.size)
          {
            GST_ERROR_OBJECT(
                this, "Output texture too large for buffer: %lu > %lu",
                required_output_size, out_info.size);
            return;
          }

          // For dynamic resolution changes, we need to create new caps and update the buffer
          if(out_width != width || out_height != height)
          {
            GST_DEBUG_OBJECT(
                this, "Dynamic resolution change detected: %dx%d -> %dx%d", width,
                height, out_width, out_height);

            // Create new caps for the actual output size
            GstCaps* new_caps = gst_caps_new_simple(
                "video/x-raw", "format", G_TYPE_STRING, "RGBA", "width", G_TYPE_INT,
                out_width, "height", G_TYPE_INT, out_height, "framerate",
                GST_TYPE_FRACTION, 30, 1, NULL);

            // Update the source pad caps
            if(!gst_pad_set_caps(GST_BASE_TRANSFORM_SRC_PAD(this), new_caps))
            {
              GST_WARNING_OBJECT(this, "Failed to update source pad caps for new size");
            }
            gst_caps_unref(new_caps);

            // Resize the output buffer to match actual output
            gst_buffer_set_size(outbuf, required_output_size);

            // Re-map the buffer with the new size
            gst_buffer_unmap(outbuf, &out_info);
            if(!gst_buffer_map(outbuf, &out_info, GST_MAP_WRITE))
            {
              GST_ERROR_OBJECT(this, "Failed to re-map resized output buffer");
              return;
            }
          }

          // Copy output texture data (no stride needed for tightly packed Avendish output)
          memcpy(out_info.data, field.texture.bytes, required_output_size);
          GST_DEBUG_OBJECT(
              this, "Copied %lu bytes from %dx%d output texture", required_output_size,
              out_width, out_height);
        }
        else
        {
          GST_ERROR_OBJECT(
              this, "Output texture bytes is NULL - no output will be generated");
        }
      });
    }

    gst_buffer_unmap(inbuf, &in_info);
    gst_buffer_unmap(outbuf, &out_info);
    return GST_FLOW_OK;
  }

  // Caps negotiation methods for handling resolution changes
  GstCaps* transform_caps(GstPadDirection direction, GstCaps* caps, GstCaps* filter)
  {
    GST_DEBUG_OBJECT(this, "transform_caps");

    // For now, allow any resolution transformation - let the effect determine the output size
    // This avoids hardcoding scaling factors and allows dynamic resolution changes
    GstCaps* result = gst_caps_copy(caps);

    // Remove specific width/height constraints to allow any size negotiation
    if(result)
    {
      gint n_structures = gst_caps_get_size(result);
      for(gint i = 0; i < n_structures; i++)
      {
        GstStructure* structure = gst_caps_get_structure(result, i);
        gst_structure_set(
            structure, "width", GST_TYPE_INT_RANGE, 1, G_MAXINT, "height",
            GST_TYPE_INT_RANGE, 1, G_MAXINT, NULL);
      }
    }

    if(filter)
    {
      GstCaps* tmp = gst_caps_intersect(result, filter);
      gst_caps_unref(result);
      result = tmp;
    }

    return result;
  }

  GstCaps* fixate_caps(GstPadDirection direction, GstCaps* caps, GstCaps* othercaps)
  {
    GST_DEBUG_OBJECT(this, "fixate_caps direction=%d", direction);

    // For dynamic resolution effects, we cannot predetermine output size
    // Start with input size as baseline, actual size will be handled in transform
    GstCaps* result = gst_caps_fixate(gst_caps_copy(othercaps));
    return result;
  }

  gboolean set_caps(GstCaps* incaps, GstCaps* outcaps)
  {
    GST_DEBUG_OBJECT(this, "set_caps");
    // Store the caps for use in transform
    return TRUE;
  }

  GstFlowReturn prepare_output_buffer(GstBuffer* inbuf, GstBuffer** outbuf)
  {
    GST_DEBUG_OBJECT(this, "prepare_output_buffer");

    // For dynamic resolution effects, allocate based on input size as maximum
    GstCaps* incaps = gst_pad_get_current_caps(GST_BASE_TRANSFORM_SINK_PAD(this));
    if(!incaps)
    {
      GST_ERROR_OBJECT(this, "No input caps available");
      return GST_FLOW_ERROR;
    }

    GstVideoInfo in_info;
    if(!gst_video_info_from_caps(&in_info, incaps))
    {
      GST_ERROR_OBJECT(this, "Failed to get video info from input caps");
      gst_caps_unref(incaps);
      return GST_FLOW_ERROR;
    }
    gst_caps_unref(incaps);

    int in_width = GST_VIDEO_INFO_WIDTH(&in_info);
    int in_height = GST_VIDEO_INFO_HEIGHT(&in_info);

    // For effects that might increase resolution, allocate a larger buffer
    // We'll use 2x the input dimensions as a reasonable upper bound
    int max_width = std::min(in_width * 2, 8192);       // Cap at 8K width
    int max_height = std::min(in_height * 2, 8192);     // Cap at 8K height
    gsize max_buffer_size = max_width * max_height * 4; // RGBA

    *outbuf = gst_buffer_new_allocate(nullptr, max_buffer_size, nullptr);

    if(!*outbuf)
    {
      GST_ERROR_OBJECT(
          this, "Failed to allocate output buffer of size %lu", max_buffer_size);
      return GST_FLOW_ERROR;
    }

    GST_DEBUG_OBJECT(
        this, "Allocated max output buffer: %dx%d (input: %dx%d), size=%lu", max_width,
        max_height, in_width, in_height, max_buffer_size);
    return GST_FLOW_OK;
  }
};

// Texture filter metaclass for texture transform elements
template <typename T>
  requires(is_texture_filter<T>())
struct metaclass<T>
{
  using type = T;
  static inline GstDebugCategory* debug_category = nullptr;

  static inline GstStaticPadTemplate sink_pad_template = GST_STATIC_PAD_TEMPLATE(
      "sink", GST_PAD_SINK, GST_PAD_ALWAYS,
      GST_STATIC_CAPS(
          "video/x-raw,format=RGBA,width=[1,MAX],height=[1,MAX],framerate=(fraction)[0/"
          "1,MAX]"));

  static inline GstStaticPadTemplate src_pad_template = GST_STATIC_PAD_TEMPLATE(
      "src", GST_PAD_SRC, GST_PAD_ALWAYS,
      GST_STATIC_CAPS(
          "video/x-raw,format=RGBA,width=[1,MAX],height=[1,MAX],framerate=(fraction)[0/"
          "1,MAX]"));

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

    init_properties(*this, "Filter/Effect/Video");

    // Transform API - disable passthrough to ensure transform is always called
    base_transform_class->passthrough_on_same_caps = FALSE;
    base_transform_class->transform_ip = nullptr; // Force out-of-place transformation
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

    // Handle dynamic output buffer allocation
    base_transform_class->prepare_output_buffer = GST_DEBUG_FUNCPTR(
        +[](GstBaseTransform* gobject, GstBuffer* inbuf,
            GstBuffer** outbuf) -> GstFlowReturn {
      return ((element<T>*)gobject)->prepare_output_buffer(inbuf, outbuf);
    });
  }

  static GType get_type()
  {
    static const GType tid = get_type_once<T>(gst_base_transform_get_type());
    return tid;
  }
};

}

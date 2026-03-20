#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <avnd/binding/touchdesigner/configure.hpp>
#include <avnd/binding/touchdesigner/helpers.hpp>
#include <avnd/binding/touchdesigner/parameter_setup.hpp>
#include <avnd/binding/touchdesigner/parameter_update.hpp>
#include <avnd/binding/touchdesigner/info_output.hpp>
#include <avnd/common/export.hpp>
#include <avnd/common/for_nth.hpp>
#include <avnd/concepts/gfx.hpp>
#include <avnd/concepts/gpu_compute.hpp>
#include <avnd/introspection/input.hpp>
#include <avnd/introspection/output.hpp>
#include <avnd/wrappers/avnd.hpp>
#include <avnd/wrappers/controls.hpp>
#include <avnd/common/enums.hpp>

#include <POP_CPlusPlusBase.h>

namespace touchdesigner::POP
{

/**
 * Particle processor binding for TouchDesigner POPs
 *
 * Maps Avendish processors to TD's particle operator API.
 * Three processing modes, selected at compile time by port introspection:
 *
 * 1. Device memory outputs (device_memory_port) — GPU path
 *    - Processor receives device (CUDA) pointers and a stream
 *    - Launches GPU kernels to fill particle attribute buffers directly
 *    - No GPU→CPU→GPU roundtrip
 *    - Processor may also declare a halp::gpu_worker member for context info
 *
 * 2. Geometry outputs (geometry_port) — CPU path
 *    - Position, normal, color, texcoord attributes are mapped to
 *      POP point attributes (P, N, Cd, uv)
 *    - Index buffers are mapped to POP index buffers
 *
 * 3. Raw buffer outputs (cpu_raw_buffer_port / cpu_typed_buffer_port)
 *    - Interpreted as tightly-packed float3 position data
 *    - Creates a point cloud with P attribute
 */
template <typename T>
struct particle_processor : public TD::POP_CPlusPlusBase
{
  avnd::effect_container<T> implementation;
  parameter_setup<T> param_setup;
  TD::POP_Context* context{};

  explicit particle_processor(const TD::OP_NodeInfo* info, TD::POP_Context* ctx)
      : context{ctx}
  {
    init();
  }

  void init()
  {
    avnd::init_controls(implementation);
  }

  void getGeneralInfo(
      TD::POP_GeneralInfo* info, const TD::OP_Inputs* inputs, void* reserved) override
  {
    info->cookEveryFrame = false;
    info->cookEveryFrameIfAsked = true;
  }

  void execute(TD::POP_Output* output, const TD::OP_Inputs* inputs, void* reserved) override
  {
    update_controls(inputs);

    // Read POP inputs into Avendish geometry inputs if applicable
    if constexpr(avnd::geometry_input_introspection<T>::size > 0)
    {
      read_pop_inputs(inputs);
    }

    // Device memory path: allocate CUDA buffers, run under CUDA ops, submit
    if constexpr(avnd::device_memory_port_output_introspection<T>::size > 0)
    {
      execute_gpu(output, inputs);
    }
    else
    {
      // CPU path: run processor, then upload results
      execute_cpu(output, inputs);
    }
  }

  void execute_cpu(TD::POP_Output* output, const TD::OP_Inputs* inputs)
  {
    struct tick { int frames = 0; } t{.frames = 1};
    invoke_effect(implementation, t);

    if constexpr(avnd::has_outputs<T>)
    {
      write_particles(output);
    }
  }

  void execute_gpu(TD::POP_Output* output, const TD::OP_Inputs* inputs)
  {
#ifdef _WIN32
    // 1. Pre-allocate CUDA POP buffers for each device_memory output
    //    (must happen before beginCUDAOperations)
    struct gpu_buf_entry {
      TD::OP_SmartRef<TD::POP_Buffer> buffer;
      uint64_t byte_size{};
    };

    // Count device_memory outputs
    constexpr int num_dm_outputs = avnd::device_memory_port_output_introspection<T>::size;
    gpu_buf_entry gpu_bufs[num_dm_outputs > 0 ? num_dm_outputs : 1];

    int dm_idx = 0;
    avnd::device_memory_port_output_introspection<T>::for_all(
        avnd::get_outputs(implementation),
        [&]<typename Field>(Field& field) {
      auto& mem = field.mem;
      uint64_t sz = mem.byte_size > 0 ? mem.byte_size : 0;
      if(sz > 0)
      {
        TD::POP_BufferInfo buf_info{};
        buf_info.size = sz;
        buf_info.mode = TD::POP_BufferMode::ReadWrite;
        buf_info.usage = TD::POP_BufferUsage::Attribute;
        buf_info.location = TD::POP_BufferLocation::CUDA;

        gpu_bufs[dm_idx].buffer = context->createBuffer(buf_info, nullptr);
        gpu_bufs[dm_idx].byte_size = sz;
      }
      dm_idx++;
    });

    // 2. Begin CUDA operations
    if(!context->beginCUDAOperations(nullptr))
    {
      // Fallback to CPU if CUDA not available
      execute_cpu(output, inputs);
      return;
    }

    // 3. Fill processor's device_memory output ports with device pointers
    dm_idx = 0;
    int device_idx = context->getCUDADeviceIndex(nullptr);
    avnd::device_memory_port_output_introspection<T>::for_all(
        avnd::get_outputs(implementation),
        [&]<typename Field>(Field& field) {
      auto& mem = field.mem;
      if(gpu_bufs[dm_idx].buffer)
      {
        mem.device_ptr = gpu_bufs[dm_idx].buffer->getData(nullptr);
        mem.byte_size = gpu_bufs[dm_idx].byte_size;
        mem.stream = nullptr; // default stream
        mem.device_index = device_idx;
      }
      dm_idx++;
    });

    // 4. Fill GPU context if processor declares one
    if constexpr(avnd::gpu_compute_context<T>)
    {
      auto& gpu = implementation.effect.gpu;
      gpu.device_index = device_idx;
      gpu.stream = nullptr; // default CUDA stream
      if constexpr(requires { gpu.native_context; })
        gpu.native_context = nullptr;
    }

    // 5. Execute processor (launches GPU kernels writing to device pointers)
    struct tick { int frames = 0; } t{.frames = 1};
    invoke_effect(implementation, t);

    // 6. End CUDA operations
    context->endCUDAOperations(nullptr);

    // 7. Submit the CUDA buffers as POP attributes
    dm_idx = 0;
    uint32_t total_points = 0;
    avnd::device_memory_port_output_introspection<T>::for_all(
        avnd::get_outputs(implementation),
        [&]<typename Field>(Field& field) {
      if(gpu_bufs[dm_idx].buffer && field.mem.byte_size > 0)
      {
        // Derive attribute info from port metadata
        const char* attr_name = "P";
        uint32_t components = 3;
        TD::POP_AttributeQualifier qualifier = TD::POP_AttributeQualifier::None;

        if constexpr(requires { Field::attribute_name(); })
        {
          attr_name = Field::attribute_name().data();
        }
        if constexpr(requires { Field::components(); })
        {
          components = Field::components();
        }
        if constexpr(requires { Field::qualifier; })
        {
          qualifier = static_cast<TD::POP_AttributeQualifier>(Field::qualifier);
        }

        uint32_t num_particles = field.mem.byte_size / (components * sizeof(float));

        TD::POP_AttributeInfo attr_info{};
        attr_info.name = attr_name;
        attr_info.numComponents = components;
        attr_info.type = TD::POP_AttributeType::Float;
        attr_info.qualifier = qualifier;
        attr_info.attribClass = TD::POP_AttributeClass::Point;

        TD::POP_SetBufferInfo set_info{};
        output->setAttribute(&gpu_bufs[dm_idx].buffer, attr_info, set_info, nullptr);

        if(total_points == 0)
          total_points = num_particles;
      }
      dm_idx++;
    });

    if(total_points > 0)
    {
      write_info_buffers(output, total_points);
    }
#else
    // CUDA not available on non-Windows — fall back to CPU
    execute_cpu(output, inputs);
#endif
  }

  void setupParameters(TD::OP_ParameterManager* manager, void* reserved) override
  {
    param_setup.setup(implementation, manager);
  }

  void pulsePressed(const char* name, void* reserved) override
  {
    if constexpr(avnd::has_inputs<T>)
      parameter_update<T>{}.pulse(implementation, name);
  }

  void buildDynamicMenu(
      const TD::OP_Inputs* inputs, TD::OP_BuildDynamicMenuInfo* info,
      void* reserved) override
  {
    if constexpr(avnd::has_inputs<T>)
      parameter_update<T>{}.menu(implementation, inputs, info);
  }

  // Info methods — route non-primary outputs to Info CHOP/DAT
  int32_t getNumInfoCHOPChans(void* reserved) override
  {
    return info_output<T>::get_num_info_chop_chans(implementation);
  }

  void getInfoCHOPChan(int32_t index, TD::OP_InfoCHOPChan* chan, void* reserved) override
  {
    info_output<T>::get_info_chop_chan(implementation, index, chan);
  }

  bool getInfoDATSize(TD::OP_InfoDATSize* infoSize, void* reserved) override
  {
    return info_output<T>::get_info_dat_size(implementation, infoSize);
  }

  void getInfoDATEntries(
      int32_t index, int32_t nEntries, TD::OP_InfoDATEntries* entries,
      void* reserved) override
  {
    info_output<T>::get_info_dat_entries(implementation, index, nEntries, entries);
  }

  void getWarningString(TD::OP_String* warning, void* reserved) override {}

  void getErrorString(TD::OP_String* error, void* reserved) override {}

  void getInfoPopupString(TD::OP_String* info, void* reserved) override {}

private:
  // ===== Output: write Avendish data to POP =====

  void write_particles(TD::POP_Output* output)
  {
    uint32_t total_points = 0;

    // Case 1: Geometry outputs
    if constexpr(avnd::geometry_output_introspection<T>::size > 0)
    {
      avnd::geometry_output_introspection<T>::for_all(
          avnd::get_outputs(implementation),
          [&]<typename Field>(Field& field) {
        total_points = write_geometry_particles(field.mesh, output);
      });
    }
    // Case 2: Raw buffer outputs
    else if constexpr(avnd::buffer_output_introspection<T>::size > 0)
    {
      total_points = write_buffer_particles(output);
    }

    if(total_points > 0)
    {
      write_info_buffers(output, total_points);
    }
  }

  // Write particles from Avendish geometry (buffers/bindings/attributes)
  template <typename GeomPort>
    requires(avnd::static_geometry_type<GeomPort> || avnd::dynamic_geometry_type<GeomPort>)
  uint32_t write_geometry_particles(GeomPort& geom, TD::POP_Output* output)
  {
    uint32_t num_vertices = 0;
    if constexpr(requires { geom.vertices; })
    {
      num_vertices = geom.vertices;
    }

    if(num_vertices == 0)
      return 0;

    // Write each attribute as a POP attribute buffer
    write_geometry_attributes(geom, output, num_vertices);

    // Write index buffer if present
    write_index_buffer(geom, output);

    return num_vertices;
  }

  // Extract per-attribute data from geometry and create POP attribute buffers
  template <typename GeomPort>
  void write_geometry_attributes(GeomPort& geom, TD::POP_Output* output, uint32_t num_vertices)
  {
    avnd::for_each_field_ref(avnd::get_attributes(geom), [&]<typename Attr>(Attr& attr) {
      // Resolve binding index and byte offset for this attribute
      int attr_binding = 0;
      int attr_offset = 0;

      if constexpr(requires { attr.binding; })
        attr_binding = attr.binding;
      if constexpr(requires { attr.byte_offset; })
        attr_offset = attr.byte_offset;

      // Get stride from the binding
      int attr_stride = 0;
      int binding_idx = 0;
      avnd::for_each_field_ref(avnd::get_bindings(geom), [&](auto& binding) {
        if(binding_idx == attr_binding)
          attr_stride = binding.stride;
        binding_idx++;
      });

      // Get buffer index and offset from the input descriptor
      int buffer_idx = 0;
      int buffer_offset = 0;
      int input_idx = 0;
      avnd::for_each_field_ref(geom.input, [&](auto& input) {
        if(input_idx == attr_binding)
        {
          if constexpr(requires { input.buffer(); })
            buffer_idx = avnd::index_in_struct_static<input.buffer()>();
          else if constexpr(requires { input.buffer; })
            buffer_idx = input.buffer;

          if constexpr(requires { input.byte_offset(); })
            buffer_offset = input.byte_offset();
          else if constexpr(requires { input.byte_offset; })
            buffer_offset = input.byte_offset;
        }
        input_idx++;
      });

      // Find the actual buffer and create the POP attribute
      int buf_idx = 0;
      avnd::for_each_field_ref(geom.buffers, [&](auto& buf) {
        if(buf_idx == buffer_idx)
        {
          const char* data_ptr = nullptr;

          if constexpr(std::is_same_v<std::decay_t<decltype(buf.elements)>, void*>)
            data_ptr = static_cast<const char*>(buf.elements);
          else
            data_ptr = reinterpret_cast<const char*>(buf.elements);

          if(!data_ptr)
          {
            buf_idx++;
            return;
          }

          const char* attr_data = data_ptr + buffer_offset + attr_offset;

          // Determine component count from attribute semantics
          uint32_t components = 3;
          TD::POP_AttributeQualifier qualifier = TD::POP_AttributeQualifier::None;

          if constexpr(requires { Attr::position; } || requires { Attr::positions; })
          {
            components = 3;
          }
          else if constexpr(requires { Attr::normal; } || requires { Attr::normals; })
          {
            components = 3;
            qualifier = TD::POP_AttributeQualifier::Direction;
          }
          else if constexpr(requires { Attr::color; } || requires { Attr::colors; })
          {
            using datatype = typename Attr::datatype;
            if constexpr(std::is_same_v<datatype, float[4]>)
              components = 4;
            else
              components = 3;
            qualifier = TD::POP_AttributeQualifier::Color;
          }
          else if constexpr(
              requires { Attr::texcoord; } || requires { Attr::tex_coord; }
              || requires { Attr::texcoords; } || requires { Attr::tex_coords; }
              || requires { Attr::uv; } || requires { Attr::uvs; })
          {
            components = 2;
          }

          // Map attribute name following Houdini/TD conventions
          const char* attr_name = "custom";
          if constexpr(requires { Attr::position; } || requires { Attr::positions; })
            attr_name = "P";
          else if constexpr(requires { Attr::normal; } || requires { Attr::normals; })
            attr_name = "N";
          else if constexpr(requires { Attr::color; } || requires { Attr::colors; })
            attr_name = "Cd";
          else if constexpr(
              requires { Attr::texcoord; } || requires { Attr::tex_coord; }
              || requires { Attr::texcoords; } || requires { Attr::tex_coords; }
              || requires { Attr::uv; } || requires { Attr::uvs; })
            attr_name = "uv";

          set_pop_attribute(output, attr_name, attr_data, num_vertices, components, attr_stride, qualifier);
        }
        buf_idx++;
      });
    });
  }

  // Create a POP attribute buffer, fill it, and assign to POP_Output
  void set_pop_attribute(
      TD::POP_Output* output, const char* name,
      const char* data, uint32_t num_particles, uint32_t components, int stride,
      TD::POP_AttributeQualifier qualifier = TD::POP_AttributeQualifier::None)
  {
    if(!context || !output || !data || num_particles == 0)
      return;

    // Allocate a POP buffer for this attribute
    uint64_t byte_size = static_cast<uint64_t>(num_particles) * components * sizeof(float);

    TD::POP_BufferInfo buf_info{};
    buf_info.size = byte_size;
    buf_info.mode = TD::POP_BufferMode::SequentialWrite;
    buf_info.usage = TD::POP_BufferUsage::Attribute;
    buf_info.location = TD::POP_BufferLocation::CPU;

    TD::OP_SmartRef<TD::POP_Buffer> buffer = context->createBuffer(buf_info, nullptr);
    if(!buffer)
      return;

    // Fill the buffer with attribute data
    float* dst = static_cast<float*>(buffer->getData(nullptr));
    if(!dst)
      return;

    if(stride == static_cast<int>(components * sizeof(float)))
    {
      // Tightly packed source - direct copy
      std::memcpy(dst, data, byte_size);
    }
    else
    {
      // Strided source - copy per-particle
      for(uint32_t i = 0; i < num_particles; ++i)
      {
        const float* src = reinterpret_cast<const float*>(data + i * stride);
        for(uint32_t c = 0; c < components; ++c)
          dst[i * components + c] = src[c];
      }
    }

    // Describe and assign the attribute
    TD::POP_AttributeInfo attr_info{};
    attr_info.name = name;
    attr_info.numComponents = components;
    attr_info.type = TD::POP_AttributeType::Float;
    attr_info.qualifier = qualifier;
    attr_info.attribClass = TD::POP_AttributeClass::Point;

    TD::POP_SetBufferInfo set_info{};
    output->setAttribute(&buffer, attr_info, set_info, nullptr);
  }

  // Write index buffer from geometry if present
  template <typename GeomPort>
  void write_index_buffer(GeomPort& geom, TD::POP_Output* output)
  {
    if constexpr(!requires { geom.index; })
      return;
    else
    {
      int buffer_idx = -1;
      int buffer_offset = 0;
      auto& input = geom.index;

      if constexpr(requires { input.buffer(); })
        buffer_idx = avnd::index_in_struct_static<input.buffer()>();
      else if constexpr(requires { input.buffer; })
        buffer_idx = input.buffer;

      if constexpr(requires { input.byte_offset(); })
        buffer_offset = input.byte_offset();
      else if constexpr(requires { input.byte_offset; })
        buffer_offset = input.byte_offset;

      if(buffer_idx < 0)
        return;

      int buf_idx = 0;
      avnd::for_each_field_ref(geom.buffers, [&](auto& buf) {
        if(buf_idx == buffer_idx)
        {
          uint32_t index_count = buf.element_count;
          if(index_count == 0)
          {
            buf_idx++;
            return;
          }

          uint64_t byte_size = static_cast<uint64_t>(index_count) * sizeof(uint32_t);

          TD::POP_BufferInfo buf_info{};
          buf_info.size = byte_size;
          buf_info.mode = TD::POP_BufferMode::SequentialWrite;
          buf_info.usage = TD::POP_BufferUsage::IndexBuffer;
          buf_info.location = TD::POP_BufferLocation::CPU;

          TD::OP_SmartRef<TD::POP_Buffer> idx_buffer = context->createBuffer(buf_info, nullptr);
          if(!idx_buffer)
          {
            buf_idx++;
            return;
          }

          uint32_t* dst = static_cast<uint32_t*>(idx_buffer->getData(nullptr));
          if(!dst)
          {
            buf_idx++;
            return;
          }

          for(uint32_t k = 0; k < index_count; ++k)
            dst[k] = static_cast<uint32_t>(buf.elements[k]);

          TD::POP_IndexBufferInfo idx_info{};
          idx_info.type = TD::POP_IndexType::UInt32;

          TD::POP_SetBufferInfo set_info{};
          output->setIndexBuffer(&idx_buffer, idx_info, set_info, nullptr);
        }
        buf_idx++;
      });
    }
  }

  // Write PointInfo and TopologyInfo buffers
  void write_info_buffers(TD::POP_Output* output, uint32_t num_points)
  {
    // Create PointInfo buffer
    TD::POP_BufferInfo pt_buf_info{};
    pt_buf_info.size = sizeof(TD::POP_PointInfo);
    pt_buf_info.mode = TD::POP_BufferMode::SequentialWrite;
    pt_buf_info.usage = TD::POP_BufferUsage::PointInfoBuffer;
    pt_buf_info.location = TD::POP_BufferLocation::CPU;

    TD::OP_SmartRef<TD::POP_Buffer> pt_buffer = context->createBuffer(pt_buf_info, nullptr);
    if(!pt_buffer)
      return;

    auto* pt_info = static_cast<TD::POP_PointInfo*>(pt_buffer->getData(nullptr));
    if(!pt_info)
      return;
    *pt_info = {};
    pt_info->numPoints = num_points;

    // Create TopologyInfo buffer
    TD::POP_BufferInfo topo_buf_info{};
    topo_buf_info.size = sizeof(TD::POP_TopologyInfo);
    topo_buf_info.mode = TD::POP_BufferMode::SequentialWrite;
    topo_buf_info.usage = TD::POP_BufferUsage::TopologyInfoBuffer;
    topo_buf_info.location = TD::POP_BufferLocation::CPU;

    TD::OP_SmartRef<TD::POP_Buffer> topo_buffer = context->createBuffer(topo_buf_info, nullptr);
    if(!topo_buffer)
      return;

    auto* topo_info = static_cast<TD::POP_TopologyInfo*>(topo_buffer->getData(nullptr));
    if(!topo_info)
      return;
    *topo_info = {};

    // Default: all points are point primitives (particle cloud)
    // Could be extended to support triangles/lines from geometry topology
    topo_info->pointPrimitivesStartIndex = 0;
    topo_info->pointPrimitivesCount = num_points;

    // Assemble POP_InfoBuffers and submit
    TD::POP_InfoBuffers info_buffers{};
    info_buffers.pointInfo = std::move(pt_buffer);
    info_buffers.topoInfo = std::move(topo_buffer);

    TD::POP_SetBufferInfo set_info{};
    output->setInfoBuffers(&info_buffers, set_info, nullptr);
  }

  // Write particles from raw buffer outputs (cpu_raw_buffer_port / cpu_typed_buffer_port)
  uint32_t write_buffer_particles(TD::POP_Output* output)
  {
    uint32_t total_points = 0;
    int output_index = 0;

    avnd::buffer_output_introspection<T>::for_all(
        avnd::get_outputs(implementation),
        [&]<typename Field>(Field& field) {
      total_points += write_single_buffer(field, output, output_index);
      output_index++;
    });

    return total_points;
  }

  template <avnd::cpu_raw_buffer_port Field>
  uint32_t write_single_buffer(Field& field, TD::POP_Output* output, int index)
  {
    auto& buf = field.buffer;
    if(!buf.raw_data || buf.byte_size <= 0)
      return 0;

    // Interpret raw buffer as tightly packed float3 xyz positions
    constexpr uint32_t components = 3;
    uint32_t num_particles = buf.byte_size / (components * sizeof(float));
    if(num_particles == 0)
      return 0;

    set_pop_attribute(
        output, "P",
        static_cast<const char*>(buf.raw_data),
        num_particles, components, components * sizeof(float));

    return num_particles;
  }

  template <avnd::cpu_typed_buffer_port Field>
  uint32_t write_single_buffer(Field& field, TD::POP_Output* output, int index)
  {
    auto& buf = field.buffer;
    if(!buf.elements || buf.element_count <= 0)
      return 0;

    using elem_type = std::decay_t<decltype(buf.elements[0])>;

    if constexpr(std::is_same_v<elem_type, float>)
    {
      constexpr uint32_t components = 3;
      uint32_t num_particles = buf.element_count / 3;
      if(num_particles == 0)
        return 0;

      set_pop_attribute(
          output, "P",
          reinterpret_cast<const char*>(buf.elements),
          num_particles, components, components * sizeof(float));

      return num_particles;
    }
    else if constexpr(requires { elem_type{}.x; elem_type{}.y; elem_type{}.z; })
    {
      constexpr uint32_t components = 3;
      set_pop_attribute(
          output, "P",
          reinterpret_cast<const char*>(buf.elements),
          buf.element_count, components, sizeof(elem_type));

      return buf.element_count;
    }
    else
    {
      return 0;
    }
  }

  template <typename Field>
  uint32_t write_single_buffer(Field& field, TD::POP_Output* output, int index)
  {
    // Fallback for unsupported buffer types
    return 0;
  }

  // ===== Input: read POP inputs into Avendish geometry =====

  void read_pop_inputs(const TD::OP_Inputs* inputs)
  {
    int input_index = 0;
    avnd::geometry_input_introspection<T>::for_all(
        avnd::get_inputs(implementation),
        [&]<typename Field>(Field& field) {
      if(input_index < inputs->getNumInputs())
      {
        const TD::OP_POPInput* pop_input = inputs->getInputPOP(input_index);
        if(pop_input)
        {
          read_pop_to_geometry(pop_input, field);
        }
      }
      input_index++;
    });
  }

  template <typename Field>
  void read_pop_to_geometry(const TD::OP_POPInput* pop_input, Field& field)
  {
    auto& geom = field.mesh;

    // Read position attribute
    TD::POP_GetBufferInfo get_info{};
    get_info.location = TD::POP_BufferLocation::CPU;

    const TD::POP_Attribute* pos_attr =
        pop_input->getAttribute(TD::POP_AttributeClass::Point, "P", nullptr);
    if(!pos_attr)
      return;

    TD::OP_SmartRef<TD::POP_Buffer> pos_buf = pos_attr->getBuffer(get_info, nullptr);
    if(!pos_buf)
      return;

    const float* pos_data = static_cast<const float*>(pos_buf->getData(nullptr));
    if(!pos_data)
      return;

    // Get point count from the PointInfo
    TD::OP_SmartRef<TD::POP_Buffer> pt_info_buf = pop_input->getPointInfo(get_info, nullptr);
    uint32_t num_points = 0;
    if(pt_info_buf)
    {
      auto* pt_info = static_cast<const TD::POP_PointInfo*>(pt_info_buf->getData(nullptr));
      if(pt_info)
        num_points = pt_info->numPoints;
    }

    if(num_points == 0)
      return;

    // Set vertex count
    if constexpr(requires { geom.vertices; })
    {
      geom.vertices = num_points;
    }

    // Copy position data into the first buffer
    // This assumes a simple layout where the first buffer holds positions
    if constexpr(requires { geom.buffers; })
    {
      uint32_t components = pos_attr->info.numComponents;
      avnd::for_each_field_ref(geom.buffers, [&](auto& buf) {
        if constexpr(std::is_same_v<std::decay_t<decltype(buf.elements)>, void*>)
        {
          // Dynamic buffer: just point to the data (valid until next cook)
          buf.elements = const_cast<void*>(static_cast<const void*>(pos_data));
          buf.byte_size = num_points * components * sizeof(float);
        }
        else if constexpr(requires { buf.element_count; })
        {
          using elem_t = std::decay_t<decltype(buf.elements[0])>;
          buf.element_count = num_points * components / sizeof(elem_t);
        }
      });
    }
  }

  void update_controls(const TD::OP_Inputs* inputs)
  {
    if constexpr(avnd::has_inputs<T>)
      parameter_update<T>{}.update(implementation, inputs);
  }
};

}

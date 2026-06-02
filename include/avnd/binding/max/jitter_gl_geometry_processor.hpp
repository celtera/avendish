#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

// Renders an Avendish geometry output port as a Jitter OpenGL 3D object (OB3D):
// a jit.gl.* node that draws itself in a named GL context. Mirrors the two-object
// pattern of jitter_processor.hpp, but swaps the Matrix Operator Protocol for the
// ob3d + t_jit_glchunk drawing path (modelled on jit.gl.gridshape).

#include <avnd/binding/max/attributes_setup.hpp>
#include <avnd/binding/max/helpers.hpp>
#include <avnd/binding/max/init.hpp>
#include <avnd/binding/max/jitter_geometry_chunk.hpp>
#include <avnd/binding/max/processor_common.hpp>
#include <avnd/concepts/gfx.hpp>
#include <avnd/introspection/input.hpp>
#include <avnd/introspection/output.hpp>
#include <avnd/wrappers/avnd.hpp>
#include <avnd/wrappers/controls.hpp>

#include <fmt/format.h>

#include <ext.h>
#include <ext_obex.h>
#include <jit.common.h>
#include <jit.max.h>
#include <jit.gl.h>

#include <cstring>
#include <new>

namespace max
{

template <typename T>
struct max_gl_geom_wrapper;

// --- The Jitter ob3d object: owns the C++ implementation, the ob3d and the chunk.
template <typename T>
struct avnd_gl_geom_jit_class
{
  t_jit_object x_obj;
  void* ob3d{}; // <-- jit_ob3d_setup oboffset points here; set by jit_ob3d_new

  max_gl_geom_wrapper<T>* wrapper{};
  avnd::effect_container<T> implementation;

  t_jit_glchunk* chunk{};
  float transform[16]{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};

  // Dirty-tracking: the geometry is recomputed in draw() only when a control /
  // bang marked it dirty (gridshape's recalc pattern), keeping everything on the
  // render thread. Starts dirty so the first draw builds the mesh.
  bool dirty{true};

  float m_bounds[6]{};
  bool m_has_bounds{false};

  void init() { }

  void free_chunk()
  {
    if(chunk)
    {
      jit_glchunk_delete(chunk);
      chunk = nullptr;
    }
  }

  // Run operator(), then refresh the chunk from the first geometry port — reusing
  // the chunk's matrices in place when the vertex/index counts are unchanged.
  void recompute()
  {
    auto& proc = implementation.effect;
    if constexpr(requires { proc(); })
      proc();

    bool done = false;
    avnd::geometry_output_introspection<T>::for_all(
        avnd::get_outputs(implementation), [&](auto& port) {
      if(done)
        return;
      auto handle = [&](auto& geom) {
        auto g = max::jitter::gather_geometry(geom);
        chunk = max::jitter::refresh_chunk(chunk, g);
        m_has_bounds = max::jitter::geometry_bounds(g, m_bounds);
      };
      if constexpr(requires { port.mesh; })
      {
        handle(port.mesh);
        if constexpr(requires { port.transform; })
          std::memcpy(transform, port.transform, sizeof(transform));
        if constexpr(requires { port.dirty_mesh; })
          port.dirty_mesh = false;
      }
      else
      {
        handle(port);
      }
      done = true;
    });
  }

  static bool is_identity(const float m[16]) noexcept
  {
    static constexpr float id[16]
        = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
    return std::memcmp(m, id, sizeof(id)) == 0;
  }

  // Called by the bound jit.gl.render on each render bang (ob3d_draw_begin/end
  // are issued by the render around this method). matrixoutput is handled inside
  // jit_ob3d_draw_chunk when the @matrixoutput attribute is set.
  t_jit_err draw()
  {
    if(dirty || !chunk)
    {
      recompute();
      dirty = false;
    }

    if(chunk && chunk->m_vertex)
    {
      // Report the AABB when the render context asks for bounds (auto-camera
      // framing, @viewalign, picking).
      if(m_has_bounds && jit_attr_getlong(&x_obj, _jit_sym_boundcalc))
        jit_attr_setfloat_array(&x_obj, _jit_sym_bounds, 6, m_bounds);

      const bool xf = !is_identity(transform);
      if(xf)
      {
        glMatrixMode(GL_MODELVIEW);
        glPushMatrix();
        glMultMatrixf(transform);
      }
      const t_jit_err e = jit_ob3d_draw_chunk(ob3d, chunk);
      if(xf)
        glPopMatrix();
      return e;
    }
    return JIT_ERR_NONE;
  }

  t_jit_err dest_closing()
  {
    free_chunk();
    return JIT_ERR_NONE;
  }
  t_jit_err dest_changed() { return JIT_ERR_NONE; }
};

// --- The Max wrapper: control inlets/attributes, forwards to the jit object.
template <typename T>
struct max_gl_geom_wrapper : processor_common<T>
{
  t_object x_obj;
  void* obex{};
  avnd_gl_geom_jit_class<T>* jit{};
  avnd::effect_container<T>& implementation;

  AVND_NO_UNIQUE_ADDRESS inputs<T> input_setup;
  AVND_NO_UNIQUE_ADDRESS outputs<T> output_setup;
  AVND_NO_UNIQUE_ADDRESS dict_storage<T> dicts;

  explicit max_gl_geom_wrapper(avnd_gl_geom_jit_class<T>* jit)
      : jit{jit}
      , implementation{jit->implementation}
  {
    this->jit->wrapper = this;
  }

  void init(int argc, t_atom* argv)
  {
    dicts.init(implementation);
    input_setup.init(implementation, x_obj);
    output_setup.init(implementation, x_obj);

    if constexpr(avnd::has_inputs<T>)
      avnd::init_controls(implementation);

    avnd::prepare(implementation, {});

    if constexpr(avnd::can_initialize<T>)
      init_arguments<T>{}.process(implementation.effect, argc, argv);

    if constexpr(avnd::attribute_input_introspection<T>::size > 0)
      avnd::attribute_input_introspection<T>::for_all_n(
          avnd::get_inputs(implementation.effect),
          attribute_object_register<max_gl_geom_wrapper<T>, T>{&x_obj});

    request_redraw();
  }

  void destroy() { dicts.release(implementation); }

  // Mark the geometry dirty and ask the bound render context to redraw; draw()
  // will recompute the mesh before drawing.
  void request_redraw()
  {
    if(jit)
      jit->dirty = true;
    if(jit && jit->ob3d)
      ob3d_dirty_set(jit->ob3d, 1);
  }

  template <typename V>
  void process(V value)
  {
    const int inlet = proxy_getinlet(&x_obj);
    processor_common<T>::process_inlet_control(
        implementation, this->input_setup, inlet, value);
    request_redraw();
  }

  void process() { request_redraw(); }

  // Called by the attribute setter (attributes_setup) after any @attr change.
  void on_attribute_changed() { request_redraw(); }

  void process_dict(t_symbol* name)
  {
    const int inlet = proxy_getinlet(&x_obj);
    processor_common<T>::process_inlet_dict(
        implementation, this->input_setup, inlet, name);
    request_redraw();
  }

  void process(t_symbol* s, int argc, t_atom* argv)
  {
    const int inlet = proxy_getinlet(&x_obj);
    if(inlet == 0 && messages<T>{}.process_messages(implementation, s, argc, argv))
      return;
    if(inlet == 0 && processor_common<T>::process_inputs(implementation, s, argc, argv))
    {
      request_redraw();
      return;
    }
    processor_common<T>::process_inlet_control(
        implementation, this->input_setup, inlet, s, argc, argv);
    request_redraw();
  }
};

template <typename T>
static inline T* jit_new_ob3d(t_class* cls, t_symbol* dest)
{
  if(auto obj = jit_object_alloc(cls))
  {
    t_jit_object tmp;
    std::memcpy(&tmp, obj, sizeof(t_jit_object));
    auto x = new(obj) T{};
    std::memcpy((void*)x, &tmp, sizeof(t_jit_object));
    jit_ob3d_new(x, dest); // creates the ob3d, stores it at x->ob3d
    x->init();
    return x;
  }
  return nullptr;
}

template <typename T>
struct jitter_gl_geometry_metaclass
{
  static inline t_class* g_jit_class = nullptr;
  static inline t_class* g_class = nullptr;
  static inline jitter_gl_geometry_metaclass* instance{};

  using attrs = avnd::attribute_input_introspection<T>;
  static constexpr int num_attributes = attrs::size;
  static inline std::array<t_object*, num_attributes> attributes;

  static inline const t_symbol* jit_class_name
      = gensym(fmt::format("jit_avnd_{}", avnd::get_c_name<T>()).c_str());

  jitter_gl_geometry_metaclass()
  {
    instance = this;
    jit_class_init();
    max_class_init();
  }

  static t_jit_err jit_class_init()
  {
    if(g_jit_class)
      return JIT_ERR_NONE;

    using jit_t = avnd_gl_geom_jit_class<T>;

    g_jit_class = (t_class*)jit_class_new(
        const_cast<char*>(jit_class_name->s_name),
        (method) + [](t_symbol* dest) -> void* {
          return jit_new_ob3d<jit_t>(g_jit_class, dest);
        },
        (method) + [](jit_t* x) {
          x->free_chunk();
          jit_ob3d_free(x);
          std::destroy_at(x);
        },
        sizeof(jit_t), A_DEFSYM, 0L);

    if(!g_jit_class)
      return JIT_ERR_OUT_OF_MEM;

    // Default ob3d flags: position/rotation/scale/color/texture/matrixoutput on.
    jit_ob3d_setup(g_jit_class, calcoffset(jit_t, ob3d), 0L);

    jit_class_addmethod(
        g_jit_class, (method) + [](jit_t* x) { return x->draw(); }, "ob3d_draw",
        A_CANT, 0L);
    jit_class_addmethod(
        g_jit_class, (method) + [](jit_t* x) { return x->dest_closing(); },
        "dest_closing", A_CANT, 0L);
    jit_class_addmethod(
        g_jit_class, (method) + [](jit_t* x) { return x->dest_changed(); },
        "dest_changed", A_CANT, 0L);

    jit_class_register(g_jit_class);
    return JIT_ERR_NONE;
  }

  static void max_class_init()
  {
    using instance_t = max_gl_geom_wrapper<T>;
    using jit_t = avnd_gl_geom_jit_class<T>;
    static const std::string max_class_name{avnd::get_c_name<T>().data()};

    static constexpr auto obj_new
        = +[](t_symbol* s, long argc, t_atom* argv) -> void* {
      auto* x = (instance_t*)max_jit_object_alloc(
          g_class, const_cast<t_symbol*>(jit_class_name));
      if(!x)
        return nullptr;

      // @drawto destination is the first non-attribute argument.
      t_symbol* dest = _jit_sym_nothing;
      const long attrstart = max_jit_attr_args_offset(argc, argv);
      if(attrstart && argv)
        jit_atom_arg_getsym(&dest, 0, attrstart, argv);

      void* o = jit_object_new(const_cast<t_symbol*>(jit_class_name), dest);
      if(!o)
      {
        object_free((t_object*)x);
        return nullptr;
      }

      max_jit_obex_jitob_set(x, o);
      max_jit_obex_dumpout_set(x, outlet_new(x, nullptr));

      // Construct the C++ wrapper, preserving the Max object header + obex.
      {
        t_object x_obj = x->x_obj;
        void* obex = x->obex;
        std::construct_at(x, (jit_t*)o);
        x->x_obj = x_obj;
        x->obex = obex;
      }

      x->init(argc, argv);
      max_jit_attr_args(x, argc, argv);
      max_jit_ob3d_attach(x, (t_jit_object*)o, outlet_new(x, "jit_matrix"));
      return x;
    };

    static constexpr auto obj_free = +[](instance_t* x) {
      if(x)
      {
        x->destroy();
        max_jit_ob3d_detach(x);
        jit_object_free(max_jit_obex_jitob_get(x));
        std::destroy_at(x);
        max_jit_object_free(x);
      }
    };

    g_class = class_new(
        max_class_name.c_str(), (method)obj_new, (method)obj_free,
        sizeof(instance_t), (method) nullptr, A_GIMME, 0);

    max_jit_class_obex_setup(g_class, calcoffset(instance_t, obex));
    max_jit_class_wrap_ob3d_inletinfo(g_class, g_jit_class, 0);
    max_jit_class_ob3d_wrap(g_class);

    static constexpr auto obj_process
        = +[](instance_t* o, t_symbol* s, int argc, t_atom* argv) -> void {
      o->process(s, argc, argv);
    };
    static constexpr auto obj_process_bang
        = +[](instance_t* o) -> void { o->process(); };
    static constexpr auto obj_process_int
        = +[](instance_t* o, t_atom_long v) -> void { o->process(v); };
    static constexpr auto obj_process_float
        = +[](instance_t* o, t_atom_float v) -> void { o->process(v); };
    static constexpr auto obj_process_sym
        = +[](instance_t* o, t_symbol* v) -> void { o->process(v); };
    static constexpr auto obj_process_dict
        = +[](instance_t* o, t_symbol* v) -> void { o->process_dict(v); };
    static constexpr auto obj_assist = processor_common<T>::obj_assist;

    class_addmethod(g_class, (method)obj_process_int, "int", A_LONG, 0);
    class_addmethod(g_class, (method)obj_process_float, "float", A_FLOAT, 0);
    class_addmethod(g_class, (method)obj_process_sym, "symbol", A_SYM, 0);
    class_addmethod(g_class, (method)obj_process_bang, "bang", A_NOTHING, 0);
    class_addmethod(g_class, (method)obj_process_dict, "dict", A_SYM, 0);
    class_addmethod(g_class, (method)obj_process, "anything", A_GIMME, 0);
    class_addmethod(g_class, (method)obj_assist, "assist", A_CANT, 0);

    if constexpr(attrs::size > 0)
      attrs::for_all(attribute_register<instance_t, T>{g_class, attributes});

    class_register(CLASS_BOX, g_class);
  }
};
}

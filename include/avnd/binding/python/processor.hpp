#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <avnd/common/enum_reflection.hpp>
#include <avnd/common/export.hpp>
//#include <halp/callback.hpp>
#include <avnd/concepts/field_names.hpp>
#include <avnd/concepts/tensor.hpp>
#include <avnd/binding/python/audio.hpp>
#include <avnd/concepts/audio_processor.hpp>
#include <avnd/introspection/messages.hpp>
#include <avnd/introspection/port.hpp>
#include <avnd/wrappers/avnd.hpp>
#include <avnd/wrappers/controls.hpp>
#include <avnd/wrappers/metadatas.hpp>
#include <avnd/wrappers/process_adapter.hpp>
#include <boost/type_index.hpp>
#include <cmath>
#include <cstring>
#include <pybind11/chrono.h>
#include <pybind11/complex.h>
#include <pybind11/functional.h>
#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <span>
#include <string>
#include <type_traits>
#include <vector>

namespace python
{
inline const char* c_str(const std::string& v) noexcept
{
  return v.c_str();
}
inline consteval const char* c_str(std::string_view v) noexcept
{
  return v.data();
}
inline consteval const char* c_str(const char* v) noexcept
{
  return v;
}

template <std::size_t Sz>
inline const char* c_str(const std::array<char, Sz>& v) noexcept
{
  return v.data();
}

namespace py = pybind11;

template <auto Idx, typename C>
constexpr auto input_name(avnd::field_reflection<Idx, C>)
{
  return avnd::get_static_symbol<C>();
}

template <auto Idx, typename C>
constexpr auto output_name(avnd::field_reflection<Idx, C>)
{
  return avnd::get_static_symbol<C>();
}

template <typename... Args>
struct register_types;
template <>
struct register_types<>
{
  void operator()(pybind11::module_& m) { }
};
template <typename Arg>
  requires std::is_aggregate_v<Arg>
struct register_types<Arg>
{
  // Sanitise the C++ pretty name into a Python-legal identifier:
  // strip everything up to and including the last "::" so a type
  // declared as `avcheck::PowerBands` is exported as `PowerBands`.
  // Without this, `m.PowerBands` can't reach the type because `::`
  // is not a valid Python identifier (you'd need
  // `getattr(m, "avcheck::PowerBands")`, which defeats discovery).
  static std::string python_name()
  {
    auto pn = boost::typeindex::type_id<Arg>().pretty_name();
    if(const auto pos = pn.rfind("::"); pos != std::string::npos)
      pn.erase(0, pos + 2);
    return pn;
  }

  void operator()(pybind11::module_& m)
  {
    static py::class_<Arg> class_def(m, python_name().c_str());
    static auto reg = [] {
      // Default constructor — without this, Python can't instantiate
      // the aggregate (it can still read instances returned by C++
      // ports, but `m.PowerBands()` would raise
      // "No constructor defined").
      class_def.def(py::init<>());
      Arg a;
      avnd::for_each_field_ref_n(
          a, []<typename M, std::size_t I>(const M&, avnd::field_index<I>) {
        static constexpr std::string_view nm = boost::pfr::get_name<I, Arg>();
        class_def.def_property(nm.data(), [](const Arg& a) {
          return avnd::pfr::get<I>(a);
        }, [](Arg& a, M x) { avnd::pfr::get<I>(a) = x; });
      });

      class_def.def("__repr__", [](const Arg& o) {
        std::string str = "{";

        avnd::for_each_field_ref_n(
            o, [&str]<typename M, std::size_t I>(const M& m, avnd::field_index<I>) {
          static constexpr std::string_view nm = boost::pfr::get_name<I, Arg>();
          str += nm;
          str += ": ";
          str += fmt::format("{}", m);
          str += ", ";
        });
        str += "}";
        return str;
      });
      return 0;
    }();
  }
};

template <typename Arg, typename... Args>
struct register_types<Arg, Args...>
{
  void operator()(pybind11::module_& m)
  {
    register_types<Arg>{}(m);
    register_types<Args...>{}(m);
  }
};

// Ensure a port value type is registered with pybind11 before any
// def_property tries to convert it. Generic — no halp:: knowledge.
//
// - Enums (std::is_enum_v): emit py::enum_<E> populated via magic_enum.
//   Each module instantiates its own static guard so the registration
//   happens exactly once per .so.
// - Aggregates with avnd::has_field_names (i.e. structs annotated via
//   the halp_field_names macro or any equivalent): delegate to the
//   existing register_types<V>{} aggregate path.
// - Everything else: trust pybind11/stl.h (std::map, std::vector,
//   std::variant, std::string, …).
template <typename V>
inline void ensure_value_type_registered(pybind11::module_& m)
{
  if constexpr(std::is_enum_v<V>)
  {
    static const bool done = [&] {
      const auto type_name = boost::typeindex::type_id<V>().pretty_name();
      auto e = py::enum_<V>(m, type_name.c_str());
      for(const auto& entry : avnd::enum_entries<V>())
        e.value(std::string{entry.second}.c_str(), entry.first);
      e.export_values();
      return true;
    }();
    (void)done;
  }
  else if constexpr(avnd::has_field_names<V>)
  {
    register_types<V>{}(m);
  }
}

template <typename, typename>
struct register_arg_types;
template <typename stdfunc_type, typename R, typename... Args>
struct register_arg_types<stdfunc_type, R(Args...)>
{
  auto operator()(pybind11::module_& m) { register_types<Args...>{}(m); }
};

template <typename, typename>
struct make_lambda_for_args;
template <typename stdfunc_type, typename R, typename... Args>
struct make_lambda_for_args<stdfunc_type, R(Args...)>
{
  auto operator()()
  {
    return +[](void* ctx, Args... args) {
      return (*(stdfunc_type*)(ctx))(std::forward<Args>(args)...);
    };
  }
};

template <typename T>
struct processor
{
  // m: pybind11 module
  py::class_<T> class_def;

  // Tensor-typed port: marshal as py::array_t<Elem> with shape +
  // strides preserved, zero-copy from numpy when layout matches.
  //
  // Dispatch on the port's value type via avnd::view_tensor_like:
  // view types receive a numpy buffer pointer + shape via
  // avnd::set_view_buffer (zero-copy when c_style + dtype match);
  // owning containers go through resize_to + memcpy.
  template <auto Idx, typename C>
  void setup_tensor_input(avnd::field_reflection<Idx, C>)
  {
    using value_type = std::remove_cvref_t<decltype(C::value)>;
    using elem_t = avnd::tensor_element<value_type>;

    if constexpr(avnd::view_tensor_like<value_type>)
    {
      class_def.def_property(
          c_str(input_name(avnd::field_reflection<Idx, C>{})),
          [](T& self) -> py::object {
            auto& v = avnd::pfr::get<Idx>(self.inputs).value;
            const auto* data = avnd::data_of(v);
            if(data == nullptr)
              return py::none();
            const auto sh = avnd::shape_of(v);
            std::vector<py::ssize_t> shv(sh.begin(), sh.end());
            return py::array_t<elem_t>(
                       shv,
                       const_cast<elem_t*>(data),
                       py::cast(&self, py::return_value_policy::reference));
          },
          [](T& self,
             py::array_t<elem_t,
                         py::array::c_style | py::array::forcecast> arr) {
            auto& v = avnd::pfr::get<Idx>(self.inputs).value;
            const auto info = arr.request();
            std::vector<std::size_t> shape(info.shape.begin(), info.shape.end());
            std::vector<std::size_t> strides(info.strides.size());
            for(std::size_t d = 0; d < info.strides.size(); ++d)
              strides[d] = static_cast<std::size_t>(info.strides[d]) / sizeof(elem_t);
            // GIL-acquiring deleter — the holder may be released from
            // a C++ destructor on an arbitrary thread.
            std::shared_ptr<void> keep_alive(
                new py::object(arr),
                [](void* p) {
                  py::gil_scoped_acquire gil;
                  delete static_cast<py::object*>(p);
                });
            avnd::set_view_buffer(
                v, static_cast<elem_t*>(info.ptr),
                std::move(shape), std::move(strides), std::move(keep_alive));
            auto& inputs = avnd::get_inputs(self);
            auto& field = boost::pfr::get<Idx>(inputs);
            if_possible(field.update(self));
          });
    }
    else
    {
      class_def.def_property(
          c_str(input_name(avnd::field_reflection<Idx, C>{})),
          // Reader: build a numpy view over the C++ tensor's data.
          // Base object keeps the processor alive while Python holds the array.
          [](T& self) -> py::array_t<elem_t> {
            auto& v = avnd::pfr::get<Idx>(self.inputs).value;
            const auto sh = avnd::shape_of(v);
            std::vector<py::ssize_t> shape_vec(sh.begin(), sh.end());
            return py::array_t<elem_t>(
                shape_vec, avnd::data_of(v),
                py::cast(&self, py::return_value_policy::reference));
          },
          // Writer: accept a numpy array, resize the C++ tensor and
          // copy. forcecast converts dtype if necessary; c_style ensures
          // contiguous source so memcpy is sound.
          [](T& self,
             py::array_t<elem_t,
                         py::array::c_style | py::array::forcecast> arr) {
            auto& v = avnd::pfr::get<Idx>(self.inputs).value;
            const auto info = arr.request();
            if constexpr(avnd::resizable_tensor_like<value_type>)
            {
              std::vector<std::size_t> sh;
              sh.reserve(static_cast<std::size_t>(info.ndim));
              for(auto s : info.shape)
                sh.push_back(static_cast<std::size_t>(s));
              avnd::resize_to(v, sh);
            }
            std::memcpy(
                avnd::data_of(v), info.ptr,
                static_cast<std::size_t>(info.itemsize)
                    * static_cast<std::size_t>(info.size));
            auto& inputs = avnd::get_inputs(self);
            auto& field = boost::pfr::get<Idx>(inputs);
            if_possible(field.update(self));
          });
    }
  }

  template <auto Idx, typename C>
  void setup_tensor_output(avnd::field_reflection<Idx, C>)
  {
    using value_type = std::remove_cvref_t<decltype(C::value)>;
    using elem_t = avnd::tensor_element<value_type>;

    class_def.def_property(
        c_str(output_name(avnd::field_reflection<Idx, C>{})),
        [](T& self) -> py::array_t<elem_t> {
          auto& v = avnd::pfr::get<Idx>(self.outputs).value;
          const auto sh = avnd::shape_of(v);
          std::vector<py::ssize_t> shape_vec(sh.begin(), sh.end());
          return py::array_t<elem_t>(
              shape_vec, avnd::data_of(v),
              py::cast(&self, py::return_value_policy::reference));
        },
        [](T& self,
           py::array_t<elem_t, py::array::c_style | py::array::forcecast> arr) {
          auto& v = avnd::pfr::get<Idx>(self.outputs).value;
          const auto info = arr.request();
          if constexpr(avnd::resizable_tensor_like<value_type>)
          {
            std::vector<std::size_t> sh;
            sh.reserve(static_cast<std::size_t>(info.ndim));
            for(auto s : info.shape)
              sh.push_back(static_cast<std::size_t>(s));
            avnd::resize_to(v, sh);
          }
          std::memcpy(
              avnd::data_of(v), info.ptr,
              static_cast<std::size_t>(info.itemsize)
                  * static_cast<std::size_t>(info.size));
        });
  }

  // Channels of a CPU texture = bytes-per-pixel / element size (handles both
  // the static-constexpr and member-function bytes_per_pixel forms).
  template <typename Tex>
  static int tex_channels(const Tex& t)
  {
    using elem_t = std::remove_pointer_t<std::decay_t<decltype(t.bytes)>>;
    if constexpr(requires { t.bytes_per_pixel(); })
      return std::max<int>(1, static_cast<int>(t.bytes_per_pixel() / sizeof(elem_t)));
    else
      return std::max<int>(1, static_cast<int>(Tex::bytes_per_pixel / sizeof(elem_t)));
  }

  // CPU texture port <-> numpy (H, W, C). uint8 for 8-bit formats, float32 for
  // float formats (dtype follows the texture's element pointer type).
  template <auto Idx, typename C>
  void setup_texture_input(avnd::field_reflection<Idx, C> refl)
  {
    using tex_t = std::decay_t<decltype(std::declval<C&>().texture)>;
    using elem_t
        = std::remove_pointer_t<std::decay_t<decltype(std::declval<tex_t&>().bytes)>>;
    std::string keep = "__keepalive_" + std::string{avnd::get_name<C>()};
    class_def.def_property(
        c_str(input_name(refl)),
        [](T& self) -> py::object {
          auto& tex = avnd::pfr::get<Idx>(self.inputs).texture;
          if(!tex.bytes || tex.width <= 0 || tex.height <= 0)
            return py::none();
          return py::array_t<elem_t>(
              std::vector<py::ssize_t>{tex.height, tex.width, tex_channels(tex)},
              tex.bytes, py::cast(&self, py::return_value_policy::reference));
        },
        [keep](
            T& self,
            py::array_t<elem_t, py::array::c_style | py::array::forcecast> arr) {
          const auto info = arr.request();
          if(info.ndim < 2)
            throw std::runtime_error("texture expects a (H, W[, C]) array");
          auto& tex = avnd::pfr::get<Idx>(self.inputs).texture;
          tex.height = static_cast<int>(info.shape[0]);
          tex.width = static_cast<int>(info.shape[1]);
          tex.bytes = static_cast<elem_t*>(info.ptr);
          if constexpr(requires { tex.changed; })
            tex.changed = true;
          py::cast(&self).attr(keep.c_str()) = arr;
        });
  }

  template <auto Idx, typename C>
  void setup_texture_output(avnd::field_reflection<Idx, C> refl)
  {
    using tex_t = std::decay_t<decltype(std::declval<C&>().texture)>;
    using elem_t
        = std::remove_pointer_t<std::decay_t<decltype(std::declval<tex_t&>().bytes)>>;
    class_def.def_property_readonly(
        c_str(output_name(refl)), [](T& self) -> py::object {
          auto& tex = avnd::pfr::get<Idx>(self.outputs).texture;
          if(!tex.bytes || tex.width <= 0 || tex.height <= 0)
            return py::none();
          return py::array_t<elem_t>(
              std::vector<py::ssize_t>{tex.height, tex.width, tex_channels(tex)},
              tex.bytes, py::cast(&self, py::return_value_policy::reference));
        });
  }

  // Buffer ports <-> 1-D numpy. raw/gpu buffers marshal as uint8 bytes (the GPU
  // handle is a host pointer on CPU hosts); typed buffers as their element dtype.
  template <typename Buf>
  static py::object buffer_to_numpy(Buf& buf, py::handle base)
  {
    if constexpr(requires { buf.elements; })
    {
      using e = std::decay_t<std::remove_pointer_t<std::decay_t<decltype(buf.elements)>>>;
      if(!buf.elements || buf.element_count <= 0)
        return py::none();
      return py::array_t<e>(
          std::vector<py::ssize_t>{buf.element_count}, buf.elements, base);
    }
    else if constexpr(requires { buf.raw_data; })
    {
      if(!buf.raw_data || buf.byte_size <= 0)
        return py::none();
      return py::array_t<std::uint8_t>(
          std::vector<py::ssize_t>{buf.byte_size},
          reinterpret_cast<std::uint8_t*>(buf.raw_data), base);
    }
    else // gpu_buffer: opaque handle, host pointer on CPU hosts
    {
      if(!buf.handle || buf.byte_size <= 0)
        return py::none();
      return py::array_t<std::uint8_t>(
          std::vector<py::ssize_t>{buf.byte_size},
          reinterpret_cast<std::uint8_t*>(buf.handle), base);
    }
  }

  template <auto Idx, typename C>
  void setup_buffer_input(avnd::field_reflection<Idx, C> refl)
  {
    using buf_t = std::decay_t<decltype(std::declval<C&>().buffer)>;
    std::string keep = "__keepalive_" + std::string{avnd::get_name<C>()};
    auto reader = [](T& self) -> py::object {
      return buffer_to_numpy(
          avnd::pfr::get<Idx>(self.inputs).buffer,
          py::cast(&self, py::return_value_policy::reference));
    };
    if constexpr(requires(buf_t b) { b.elements; })
    {
      using e = std::decay_t<std::remove_pointer_t<std::decay_t<decltype(buf_t{}.elements)>>>;
      class_def.def_property(
          c_str(input_name(refl)), reader,
          [keep](T& self, py::array_t<e, py::array::c_style | py::array::forcecast> arr) {
            const auto info = arr.request();
            auto& b = avnd::pfr::get<Idx>(self.inputs).buffer;
            b.elements = static_cast<e*>(info.ptr);
            b.element_count = info.size;
            if constexpr(requires { b.changed; })
              b.changed = true;
            py::cast(&self).attr(keep.c_str()) = arr;
          });
    }
    else
    {
      class_def.def_property(
          c_str(input_name(refl)), reader,
          [keep](
              T& self,
              py::array_t<std::uint8_t, py::array::c_style | py::array::forcecast> arr) {
            const auto info = arr.request();
            auto& b = avnd::pfr::get<Idx>(self.inputs).buffer;
            if constexpr(requires { b.raw_data; })
              b.raw_data = reinterpret_cast<unsigned char*>(info.ptr);
            else
              b.handle = info.ptr;
            b.byte_size = info.size;
            if constexpr(requires { b.changed; })
              b.changed = true;
            py::cast(&self).attr(keep.c_str()) = arr;
          });
    }
  }

  template <auto Idx, typename C>
  void setup_buffer_output(avnd::field_reflection<Idx, C> refl)
  {
    class_def.def_property_readonly(
        c_str(output_name(refl)), [](T& self) -> py::object {
          return buffer_to_numpy(
              avnd::pfr::get<Idx>(self.outputs).buffer,
              py::cast(&self, py::return_value_policy::reference));
        });
  }

  template <auto Idx, typename C>
  void setup_input(avnd::field_reflection<Idx, C> refl, pybind11::module_& m)
  {
    using value_type = std::remove_cvref_t<decltype(C::value)>;
    if constexpr(avnd::tensor_like<value_type>)
    {
      setup_tensor_input<Idx, C>(refl);
      return;
    }
    ensure_value_type_registered<value_type>(m);
    class_def.def_property(c_str(input_name(refl)), [](const T& t) {
      return avnd::pfr::get<Idx>(t.inputs).value;
    }, [](T& t, decltype(C::value) x) {
      avnd::pfr::get<Idx>(t.inputs).value = x;
      auto& inputs = avnd::get_inputs(t);
      auto& field = boost::pfr::get<Idx>(inputs);
      if_possible(field.update(t));
    });
  }

  template <auto Idx, typename C>
  void setup_output(avnd::field_reflection<Idx, C> refl, pybind11::module_& m)
  {
    if constexpr(requires { avnd::get_name<C>(); })
    {
      using value_type = std::remove_cvref_t<decltype(C::value)>;
      if constexpr(avnd::tensor_like<value_type>)
      {
        setup_tensor_output<Idx, C>(refl);
        return;
      }
      ensure_value_type_registered<value_type>(m);
      class_def.def_property(
          c_str(output_name(refl)),
          [](const T& t) { return avnd::pfr::get<Idx>(t.outputs).value; },
          [](T& t, decltype(C::value) x) { avnd::pfr::get<Idx>(t.outputs).value = x; });
    }
  }

  template <auto Idx, typename M>
  void setup_message(avnd::field_reflection<Idx, M>)
  {
    using func_type = decltype(avnd::message_get_func<M>());
    if constexpr(std::is_member_function_pointer_v<func_type>)
    {
      static constexpr auto func = avnd::message_get_func<M>();
      using refl = avnd::function_reflection<func>;
      using class_type = typename refl::class_type;
      if constexpr(std::is_same_v<class_type, M>)
      {
        auto synth_fun = []<typename... Args>(boost::mp11::mp_list<Args...>) {
          return [](Args... args) { M{}(args...); };
        };
        class_def.def(
            c_str(avnd::get_static_symbol<M>()), synth_fun(typename refl::arguments{}));
      }
      else
      {
        class_def.def(c_str(avnd::get_static_symbol<M>()), func);
      }
    }
    else if constexpr(requires { avnd::function_reflection<M::func()>::count; })
    {
      // TODO other cases: see pd
      class_def.def_static(
          c_str(avnd::get_static_symbol<M>()), avnd::message_get_func<M>());
    }
  }

  // dynamic_attr lets us stash keep-alive numpy buffers (texture/buffer inputs)
  // as per-instance Python attributes so the C++ pointers stay valid.
  explicit processor(pybind11::module_& m)
      : class_def(m, c_str(avnd::get_c_identifier<T>()), py::dynamic_attr())
  {
    m.doc() = c_str(avnd::get_description<T>());

    class_def.def(py::init<>());
    if constexpr(requires { T{}(); })
    {
      class_def.def("process", &T::operator());
    }

    if constexpr(avnd::audio_processor<T>)
    {
      class_def.def(
          "process_audio",
          [](T& self,
             py::array_t<float, py::array::c_style | py::array::forcecast> ins,
             double rate) { return run_audio(self, std::move(ins), rate); },
          py::arg("input"), py::arg("rate") = 48000.0);
    }

    if constexpr(avnd::inputs_is_value<T>)
    {
      avnd::parameter_input_introspection<T>::for_all(
          [this, &m](auto a) { setup_input(a, m); });
    }

    if constexpr(avnd::outputs_is_value<T>)
    {
      avnd::parameter_output_introspection<T>::for_all(
          [this, &m](auto a) { setup_output(a, m); });
      avnd::callback_output_introspection<T>::for_all(
          [this, &m](auto a) { setup_callback(a, m); });
    }

    if constexpr(avnd::cpu_texture_input_introspection<T>::size > 0)
      avnd::cpu_texture_input_introspection<T>::for_all(
          [this](auto a) { setup_texture_input(a); });
    if constexpr(avnd::cpu_texture_output_introspection<T>::size > 0)
      avnd::cpu_texture_output_introspection<T>::for_all(
          [this](auto a) { setup_texture_output(a); });

    if constexpr(avnd::buffer_input_introspection<T>::size > 0)
      avnd::buffer_input_introspection<T>::for_all(
          [this](auto a) { setup_buffer_input(a); });
    if constexpr(avnd::buffer_output_introspection<T>::size > 0)
      avnd::buffer_output_introspection<T>::for_all(
          [this](auto a) { setup_buffer_output(a); });

    avnd::messages_introspection<T>::for_all([this](auto a) { setup_message(a); });
  }

  template <std::size_t Idx, typename C>
  void setup_callback(const avnd::field_reflection<Idx, C>& out, pybind11::module_& m)
  {
    if constexpr(requires { avnd::get_static_symbol<C>(); })
    {
      using call_type = decltype(C::call);

      if constexpr(avnd::function_view_ish<call_type>)
      {
        using func_type = typename call_type::type;
        using stdfunc_type = std::function<func_type>;
        register_arg_types<stdfunc_type, func_type>{}(m);

        class_def.def_property(c_str(avnd::get_static_symbol<C>()), [](T& t) {
          return avnd::pfr::get<Idx>(t.outputs).call;
        }, [](T& t, stdfunc_type cb) {
          auto& cur = avnd::pfr::get<Idx>(t.outputs).call;
          if(cur.context)
            delete(stdfunc_type*)cur.context;

          cur.context = new stdfunc_type{std::move(cb)};
          cur.function = make_lambda_for_args<stdfunc_type, func_type>{}();
        });
      }
      else if constexpr(avnd::function_ish<call_type>)
      {
        class_def.def_property(c_str(avnd::get_static_symbol<C>()), [](T& t) {
          return avnd::pfr::get<Idx>(t.outputs).call;
        }, [](T& t, call_type cb) {
          avnd::pfr::get<Idx>(t.outputs).call = std::move(cb);
        });
      }
      else
      {
        static_assert(C::callback_type_unsupported);
      }
    }
  }
};
}

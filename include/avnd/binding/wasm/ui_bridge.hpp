#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

/**
 * UI bridge for the WASM backend. Provides getUILayout() (a JS tree describing
 * the plug-in's `ui`, with control indices matching setParameter) and
 * paintCustom() (draws a custom paint item via wasm::canvas_painter). With no
 * `ui` member, getUILayout() returns a flat vbox of every parameter.
 */

#include <avnd/binding/wasm/painter.hpp>
#include <avnd/common/for_nth.hpp>
#include <avnd/concepts/layout.hpp>
#include <avnd/introspection/input.hpp>
#include <avnd/introspection/layout.hpp>
#include <avnd/wrappers/metadatas.hpp>

#include <string>
#include <string_view>

#if defined(__EMSCRIPTEN__)
#include <emscripten/val.h>
#endif

namespace wasm
{

// A custom paint item: provides width()/height() and a paint(painter) method.
template <typename Item>
concept paintable_item = requires(Item it) {
  { Item::width() };
  { Item::height() };
} && requires(Item it) {
#if defined(__EMSCRIPTEN__)
  it.paint(std::declval<wasm::canvas_painter&>());
#else
  true;
#endif
};

template <typename T>
struct ui_custom_items;

// Layout-node classification helpers (work for both the enum{hbox} marker
// convention and the halp_meta(layout, ...) convention).
template <typename I>
concept marker_hbox = requires(I i) { i.hbox; };
template <typename I>
concept marker_vbox = requires(I i) { i.vbox; };
template <typename I>
concept marker_split = requires(I i) { i.split; };
template <typename I>
concept marker_grid = requires(I i) { i.grid; };
template <typename I>
concept marker_group = requires(I i) { i.group; };
template <typename I>
concept marker_tabs = requires(I i) { i.tabs; };
template <typename I>
concept marker_spacing = requires(I i) { i.spacing; };

template <typename I>
concept is_hbox = marker_hbox<I> || avnd::hbox_layout<I>;
template <typename I>
concept is_vbox = marker_vbox<I> || avnd::vbox_layout<I>;
template <typename I>
concept is_split = marker_split<I> || avnd::split_layout<I>;
template <typename I>
concept is_grid = marker_grid<I> || avnd::grid_layout<I>;
template <typename I>
concept is_group = marker_group<I> || avnd::group_layout<I>;
template <typename I>
concept is_tabs = marker_tabs<I> || avnd::tab_layout<I>;
template <typename I>
concept is_container = avnd::container_layout<I>;
template <typename I>
concept is_spacing = marker_spacing<I> || avnd::spacing_layout<I>;

// A custom item node: derives from the paint item, so it has paint().
template <typename I>
concept is_custom_node = paintable_item<std::decay_t<I>>
                         && (avnd::custom_layout<std::decay_t<I>>
                             || avnd::custom_control_layout<std::decay_t<I>>);

// A control item referencing an input member pointer (`decltype(&Inputs::x)`).
template <typename T, typename Impl, typename I>
concept is_member_ptr_control = requires(Impl& impl, const I& item) {
  (avnd::get_inputs<T>(impl).*item);
};

// A control item that carries a `.model` member pointer (halp::control<F>).
template <typename I>
concept has_model_ptr = requires(I i) { i.model; };

template <typename T>
class ui_bridge
{
public:
  using inputs_t = decltype(avnd::get_inputs<T>(std::declval<avnd::effect_container<T>&>()));
  using param_intro = avnd::parameter_input_introspection<T>;

  static constexpr bool has_custom()
  {
    if constexpr(avnd::has_ui<T>)
    {
      bool found = false;
      using ui_t = typename avnd::ui_type<T>::type;
      static const constexpr ui_t ui{};
      count_custom(ui, found);
      return found;
    }
    else
    {
      return false;
    }
  }

  static constexpr int custom_count()
  {
    if constexpr(avnd::has_ui<T>)
    {
      int n = 0;
      using ui_t = typename avnd::ui_type<T>::type;
      static const constexpr ui_t ui{};
      do_count(ui, n);
      return n;
    }
    else
    {
      return 0;
    }
  }

#if defined(__EMSCRIPTEN__)
  static emscripten::val layout(avnd::effect_container<T>& object)
  {
    if constexpr(avnd::has_ui<T>)
    {
      using ui_t = typename avnd::ui_type<T>::type;
      static const constexpr ui_t ui{};
      int custom_counter = 0;
      return node_to_val(object, ui, custom_counter);
    }
    else
    {
      // Flat fallback: a vbox of every parameter.
      emscripten::val root = emscripten::val::object();
      root.set("type", std::string{"vbox"});
      emscripten::val children = emscripten::val::array();
      for(int i = 0; i < (int)param_intro::size; ++i)
      {
        emscripten::val c = emscripten::val::object();
        c.set("type", std::string{"control"});
        c.set("index", i);
        children.set(i, c);
      }
      root.set("children", children);
      return root;
    }
  }

  // Find the itemIndex-th custom item, sync it from live ports, call paint().
  static void
  paint(avnd::effect_container<T>& object, int itemIndex, emscripten::val ctx2d)
  {
    if constexpr(avnd::has_ui<T>)
    {
      using ui_t = typename avnd::ui_type<T>::type;
      ui_t ui{};
      wasm::canvas_painter p{ctx2d};
      int counter = 0;
      paint_walk(object, ui, itemIndex, counter, p);
    }
  }
#endif

private:
  template <typename Item>
  static constexpr void count_custom(const Item& item, bool& found)
  {
    if constexpr(is_custom_node<Item>)
    {
      found = true;
    }
    else if constexpr(
        is_hbox<Item> || is_vbox<Item> || is_split<Item> || is_grid<Item>
        || is_group<Item> || is_tabs<Item> || is_container<Item>)
    {
      avnd::for_each_field_ref(
          item, [&](const auto& child) { count_custom(child, found); });
    }
  }

  template <typename Item>
  static constexpr void do_count(const Item& item, int& n)
  {
    if constexpr(is_custom_node<Item>)
    {
      ++n;
    }
    else if constexpr(
        is_hbox<Item> || is_vbox<Item> || is_split<Item> || is_grid<Item>
        || is_group<Item> || is_tabs<Item> || is_container<Item>)
    {
      avnd::for_each_field_ref(
          item, [&](const auto& child) { do_count(child, n); });
    }
  }

#if defined(__EMSCRIPTEN__)
  // Map a control item to its parameter index (the index setParameter uses), or
  // -1 when the referenced member is not a parameter port.
  template <typename Item>
  static int control_param_index(avnd::effect_container<T>& object, const Item& item)
  {
    auto& ins = avnd::get_inputs<T>(object);
    if constexpr(is_member_ptr_control<T, avnd::effect_container<T>, Item>)
    {
      const int field_idx = avnd::index_in_struct(ins, item);
      return param_intro::field_index_to_index(field_idx);
    }
    else if constexpr(has_model_ptr<Item>)
    {
      const int field_idx = avnd::index_in_struct(ins, item.model);
      return param_intro::field_index_to_index(field_idx);
    }
    else
    {
      return -1;
    }
  }

  static const char* type_name(auto&& item)
  {
    using Item = std::decay_t<decltype(item)>;
    if constexpr(is_tabs<Item>)
      return "tabs";
    else if constexpr(is_split<Item>)
      return "split";
    else if constexpr(is_grid<Item>)
      return "grid";
    else if constexpr(is_group<Item>)
      return "group";
    else if constexpr(is_hbox<Item>)
      return "hbox";
    else if constexpr(is_vbox<Item>)
      return "vbox";
    else
      return "container";
  }

  template <typename Item>
  static emscripten::val node_to_val(
      avnd::effect_container<T>& object, const Item& item, int& custom_counter)
  {
    emscripten::val o = emscripten::val::object();

    if constexpr(is_spacing<Item>)
    {
      o.set("type", std::string{"spacing"});
      if constexpr(requires { Item::width(); })
        o.set("width", (double)Item::width());
      else if constexpr(requires { item.width; })
        o.set("width", (double)item.width);
      if constexpr(requires { Item::height(); })
        o.set("height", (double)Item::height());
      else if constexpr(requires { item.height; })
        o.set("height", (double)item.height);
      return o;
    }
    else if constexpr(is_custom_node<Item>)
    {
      o.set("type", std::string{"custom"});
      o.set("itemIndex", custom_counter++);
      o.set("width", (double)Item::width());
      o.set("height", (double)Item::height());
      o.set("fullyCustom", (bool)avnd::tag_fully_custom_item<T>);
      return o;
    }
    else if constexpr(
        is_hbox<Item> || is_vbox<Item> || is_split<Item> || is_grid<Item>
        || is_group<Item> || is_tabs<Item> || is_container<Item>)
    {
      o.set("type", std::string{type_name(item)});
      if constexpr(requires { Item::name(); })
        o.set("name", std::string{std::string_view{Item::name()}});
      if constexpr(is_split<Item> || is_grid<Item>)
      {
        if constexpr(requires { Item::width(); })
          o.set("width", (double)Item::width());
        if constexpr(requires { Item::height(); })
          o.set("height", (double)Item::height());
        if constexpr(requires { Item::columns(); })
          o.set("columns", (int)Item::columns());
      }
      emscripten::val children = emscripten::val::array();
      int k = 0;
      avnd::for_each_field_ref(item, [&](const auto& child) {
        children.set(k++, node_to_val(object, child, custom_counter));
      });
      o.set("children", children);
      return o;
    }
    else if constexpr(
        std::convertible_to<Item, std::string_view>)
    {
      o.set("type", std::string{"label"});
      o.set("text", std::string{std::string_view{item}});
      return o;
    }
    else
    {
      const int pidx = control_param_index(object, item);
      if(pidx >= 0)
      {
        o.set("type", std::string{"control"});
        o.set("index", pidx);
      }
      else
      {
        o.set("type", std::string{"unknown"});
      }
      return o;
    }
  }

  // Find the itemIndex-th custom item and paint it.
  template <typename Item>
  static void paint_walk(
      avnd::effect_container<T>& object, Item& item, int target, int& counter,
      wasm::canvas_painter& p)
  {
    if constexpr(is_custom_node<Item>)
    {
      if(counter == target)
      {
        sync_custom_item(object, item);
        item.paint(p);
      }
      ++counter;
    }
    else if constexpr(
        is_hbox<Item> || is_vbox<Item> || is_split<Item> || is_grid<Item>
        || is_group<Item> || is_tabs<Item> || is_container<Item>)
    {
      avnd::for_each_field_ref(item, [&](auto& child) {
        paint_walk(object, child, target, counter, p);
      });
    }
  }

  // Sync a custom item from live port values before painting: (a) a
  // custom_control value mirror, then (b) a general update_from() bind hook.
  template <typename Item>
  static void sync_custom_item(avnd::effect_container<T>& object, Item& item)
  {
    auto& ins = avnd::get_inputs<T>(object);
    if constexpr(has_model_ptr<Item> && requires { item.value; })
    {
      item.value = (ins.*(item.model)).value;
    }
    if constexpr(requires { item.update_from(object.effect); })
    {
      item.update_from(object.effect);
    }
    else if constexpr(requires { item.update_from(ins); })
    {
      item.update_from(ins);
    }
  }
#endif
};

} // namespace wasm

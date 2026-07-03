#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

/**
 * Reference software UI runtime: walks a plug-in's declarative `struct ui`
 * layout tree, renders standard controls through Nuklear and custom paint()
 * widgets through the canvas_ity painter, all into a single RGBA8
 * framebuffer. Pure "events in, pixels out": no OS or GPU calls, so shells
 * range from a pugl child window in a VST editor to an ESP32 LCD flush or a
 * headless golden test. See CUSTOM_UI_PLAN.md §6.
 */

#include <avnd/binding/ui/soft/framebuffer.hpp>
#include <avnd/binding/ui/soft/nk_renderer.hpp>
#include <avnd/binding/ui/soft/painter.hpp>
#include <avnd/common/for_nth.hpp>
#include <avnd/concepts/gui_window.hpp>
#include <avnd/concepts/layout.hpp>
#include <avnd/concepts/ui.hpp>
#include <avnd/introspection/input.hpp>
#include <avnd/introspection/layout.hpp>
#include <avnd/introspection/output.hpp>
#include <avnd/introspection/range.hpp>
#include <avnd/wrappers/controls.hpp>
#include <avnd/wrappers/effect_container.hpp>
#include <avnd/wrappers/metadatas.hpp>
#include <avnd/wrappers/widgets.hpp>

#include <algorithm>
#include <cstdio>
#include <map>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace avnd::soft_ui
{
// ---- Layout tree classification (same shapes as binding/wasm/ui_bridge.hpp:
// halp_meta(layout, ...) enums or bare `enum { hbox };` markers) ----
template <typename I>
concept marker_hbox = requires(I i) { i.hbox; };
template <typename I>
concept marker_vbox = requires(I i) { i.vbox; };
template <typename I>
concept marker_tabs = requires(I i) { i.tabs; };
template <typename I>
concept marker_group = requires(I i) { i.group; };
template <typename I>
concept marker_spacing = requires(I i) { i.spacing; };

template <typename I>
concept is_hbox = marker_hbox<I> || avnd::hbox_layout<I>;
template <typename I>
concept is_vbox = marker_vbox<I> || avnd::vbox_layout<I>;
template <typename I>
concept is_tabs = marker_tabs<I> || avnd::tab_layout<I>;
template <typename I>
concept is_group = marker_group<I> || avnd::group_layout<I>;
template <typename I>
concept is_split = avnd::split_layout<I>;
template <typename I>
concept is_grid = avnd::grid_layout<I>;
template <typename I>
concept is_container = avnd::container_layout<I>;
// Require the geometry too: a container holding a field *named* `spacing`
// (e.g. a halp::spacing member) must not classify as spacing itself.
template <typename I>
concept is_spacing = (marker_spacing<I> || avnd::spacing_layout<I>)
                     && requires(I i) {
                          { i.width } -> std::convertible_to<int>;
                          { i.height } -> std::convertible_to<int>;
                        };

template <typename I>
concept is_label_item = requires(I i) {
  { i.text } -> std::convertible_to<std::string_view>;
};

// halp::item / halp::control / halp::custom_control: reference a port through
// a member object pointer
template <typename I>
concept has_control_model = requires(I i) { i.model; };

template <typename I>
concept paintable_item = requires(I i) {
  { I::width() };
  { I::height() };
};

template <typename I>
concept is_custom_item
    = paintable_item<I>
      && (avnd::custom_layout<I> || avnd::custom_control_layout<I>);

template <typename I>
concept is_any_container
    = is_hbox<I> || is_vbox<I> || is_tabs<I> || is_group<I> || is_split<I>
      || is_grid<I> || is_container<I>;

template <typename T>
class ui_runtime
{
public:
  using ui_t = typename avnd::ui_type<T>::type;
  static constexpr int row_height = 28;
  static constexpr int nested_box_height = 220;

  explicit ui_runtime(avnd::effect_container<T>& impl, font_registry fonts = {})
      : m_impl{impl}
      , m_fonts{std::move(fonts)}
      , m_renderer{m_fonts}
  {
    m_font = m_renderer.make_font(13.f);
    nk_init_default(&m_ctx, &m_font);

    if constexpr(requires { (int)ui_t::width(); })
      m_width = ui_t::width();
    if constexpr(requires { (int)ui_t::height(); })
      m_height = ui_t::height();

    init_items(m_ui);
    init_bus();
  }

  ~ui_runtime() { nk_free(&m_ctx); }

  ui_runtime(const ui_runtime&) = delete;
  ui_runtime& operator=(const ui_runtime&) = delete;

  avnd::gui_host host{};

  int width() const noexcept { return m_width; }
  int height() const noexcept { return m_height; }
  ui_t& ui() noexcept { return m_ui; }
  font_registry& fonts() noexcept { return m_fonts; }

  // ---- Bus transport overrides ----
  // By default the constructor wires the message bus synchronously
  // (headless / single-threaded shells). Plug-in bindings replace both
  // directions with lock-free queues: UI→processor via this setter,
  // processor→UI by reassigning effect.send_message to enqueue and calling
  // deliver_to_ui() from their UI-thread tick when draining.
  template <typename F>
  void set_bus_to_processor(F&& f)
  {
    if constexpr(requires { m_bus->send_message; })
      if(m_bus)
        m_bus->send_message = std::forward<F>(f);
  }

  void deliver_to_ui(auto&& msg)
  {
    if constexpr(requires {
                   ui_t::bus::process_message(
                       m_ui, std::forward<decltype(msg)>(msg));
                 })
    {
      ui_t::bus::process_message(m_ui, std::forward<decltype(msg)>(msg));
      m_dirty = true;
    }
  }

  void set_viewport(int w, int h, double scale = 1.)
  {
    m_width = w;
    m_height = h;
    m_scale = scale;
    m_canvas.reset();
    m_dirty = true;
  }

  // ---- Input (shell → runtime) ----
  void pointer_move(double x, double y)
  {
    m_events.push_back({event::motion, x, y, 0});
  }
  void pointer_button(double x, double y, bool pressed)
  {
    m_events.push_back({event::button, x, y, pressed ? 1 : 0});
  }
  void wheel(double x, double y, double delta)
  {
    m_events.push_back({event::scroll, x, y, (int)(delta * 120)});
  }

  // ---- Frame ----
  // Widget pass; returns true if a repaint is needed.
  bool tick()
  {
    if(m_needs_clear)
    {
      nk_clear(&m_ctx);
      m_needs_clear = false;
    }

    apply_input();
    handle_capture();

    m_custom_draws.clear();
    if(nk_begin(
           &m_ctx, "avnd", nk_rect(0, 0, (float)m_width, (float)m_height),
           NK_WINDOW_BACKGROUND | NK_WINDOW_NO_SCROLLBAR))
    {
      walk_children(m_ui, /*columns=*/1);
    }
    nk_end(&m_ctx);
    m_needs_clear = true;

    m_mouse_was_down = m_mouse_down;
    return std::exchange(m_dirty, true); // dirty-tracking refinement later:
                                         // for now every tick repaints
  }

  void render(framebuffer fb)
  {
    if(!m_canvas || fb.width != m_width || fb.height != m_height)
    {
      m_width = fb.width;
      m_height = fb.height;
      m_canvas.emplace(m_width, m_height);
    }

    auto& c = *m_canvas;
    c.set_transform(1.f, 0.f, 0.f, 1.f, 0.f, 0.f);
    c.clear_rectangle(0.f, 0.f, (float)m_width, (float)m_height);

    m_renderer.render(&m_ctx, c);
    nk_clear(&m_ctx);
    m_needs_clear = false;

    // Custom paint() widgets composite over the Nuklear pass, in
    // widget-local coordinates.
    painter p{c, m_fonts, m_dirty};
    for(auto& d : m_custom_draws)
    {
      p.set_base_translation(d.x, d.y);
      d.paint(d.widget, p);
    }
    p.set_base_translation(0., 0.);

    c.get_image_data(fb.data, fb.width, fb.height, fb.row_bytes(), 0, 0);
    m_dirty = false;
  }

private:
  // ================= Input =================
  struct event
  {
    enum kind : uint8_t
    {
      motion,
      button,
      scroll
    } type;
    double x, y;
    int data;
  };

  void apply_input()
  {
    nk_input_begin(&m_ctx);
    for(auto& e : m_events)
    {
      switch(e.type)
      {
        case event::motion:
          nk_input_motion(&m_ctx, (int)e.x, (int)e.y);
          break;
        case event::button:
          m_mouse_down = e.data != 0;
          nk_input_button(&m_ctx, NK_BUTTON_LEFT, (int)e.x, (int)e.y, e.data);
          break;
        case event::scroll:
          nk_input_scroll(&m_ctx, nk_vec2(0.f, e.data / 120.f));
          break;
      }
      m_mouse_x = e.x;
      m_mouse_y = e.y;
    }
    m_events.clear();
    nk_input_end(&m_ctx);
  }

  // Custom widgets capture the mouse while dragging.
  void handle_capture()
  {
    if(!m_capture)
      return;
    const double lx = m_mouse_x - m_capture_x;
    const double ly = m_mouse_y - m_capture_y;
    if(m_mouse_down)
    {
      m_capture_move(m_capture, lx, ly);
    }
    else
    {
      m_capture_release(m_capture, lx, ly);
      m_capture = nullptr;
    }
    m_dirty = true;
  }

  // ================= Layout walk =================
  void walk_children(auto& item, int columns)
  {
    avnd::for_each_field_ref(
        item, [this, columns](auto& child) { this->walk(child, columns); });
  }

  template <typename Item>
  void walk(Item& item, int columns)
  {
    using I = std::remove_cvref_t<Item>;
    if constexpr(is_custom_item<I>)
    {
      custom_widget(item);
    }
    else if constexpr(is_label_item<I>)
    {
      if(columns == 1)
        nk_layout_row_dynamic(&m_ctx, row_height, 1);
      const std::string_view txt = item.text;
      nk_text(&m_ctx, txt.data(), (int)txt.size(), NK_TEXT_LEFT);
    }
    else if constexpr(is_spacing<I>)
    {
      if(columns == 1)
        nk_layout_row_dynamic(&m_ctx, std::max<float>(1.f, item.height), 1);
      nk_spacer(&m_ctx);
    }
    else if constexpr(has_control_model<I>)
    {
      resolve_control(item.model, columns);
    }
    else if constexpr(std::is_member_object_pointer_v<I>)
    {
      // raw `decltype(&Inputs::ctl)` members, as in the original prototype
      resolve_control(item, columns);
    }
    else if constexpr(is_tabs<I>)
    {
      tabs(item);
    }
    else if constexpr(is_hbox<I> || is_split<I> || is_grid<I>)
    {
      hbox(item);
    }
    else if constexpr(is_group<I>)
    {
      group(item);
    }
    else if constexpr(is_vbox<I> || is_container<I>)
    {
      walk_children(item, 1);
    }
    else
    {
      // Unknown item: ignore.
    }
  }

  template <typename Item>
  void hbox(Item& item)
  {
    using I = std::remove_cvref_t<Item>;
    constexpr int n = avnd::pfr::tuple_size_v<I>;
    if constexpr(n > 0)
    {
      constexpr bool has_nested = []<std::size_t... Is>(std::index_sequence<Is...>) {
        return (is_any_container<std::remove_cvref_t<
                    decltype(avnd::pfr::get<Is>(std::declval<I&>()))>>
                || ...);
      }(std::make_index_sequence<n>{});

      if constexpr(has_nested)
      {
        nk_layout_row_dynamic(&m_ctx, nested_box_height, n);
        int k = 0;
        avnd::for_each_field_ref(item, [this, &k](auto& child) {
          char name[32];
          std::snprintf(name, sizeof(name), "hb_%d_%p", k++, (void*)&child);
          if(nk_group_begin(&m_ctx, name, NK_WINDOW_NO_SCROLLBAR))
          {
            this->walk(child, 1);
            nk_group_end(&m_ctx);
          }
        });
      }
      else
      {
        nk_layout_row_dynamic(&m_ctx, row_height, n);
        walk_children(item, n);
      }
    }
  }

  template <typename Item>
  void group(Item& item)
  {
    using I = std::remove_cvref_t<Item>;
    if constexpr(requires { I::name(); })
    {
      nk_layout_row_dynamic(&m_ctx, row_height, 1);
      const std::string_view name = I::name();
      nk_text(&m_ctx, name.data(), (int)name.size(), NK_TEXT_LEFT);
    }
    walk_children(item, 1);
  }

  template <typename Item>
  void tabs(Item& item)
  {
    using I = std::remove_cvref_t<Item>;
    constexpr int n = avnd::pfr::tuple_size_v<I>;
    if constexpr(n > 0)
    {
      // Tab bar: one selectable per child, then the active child.
      auto& state = m_tab_state[(void*)&item];
      nk_layout_row_dynamic(&m_ctx, row_height, n);
      int k = 0;
      avnd::for_each_field_ref(item, [&]<typename TT>(TT& child) {
        nk_bool active = (state == k);
        std::string_view name = "Tab";
        if constexpr(requires { std::string_view{TT::name()}; })
          name = TT::name();
        m_scratch.assign(name);
        if(nk_selectable_label(&m_ctx, m_scratch.c_str(), NK_TEXT_CENTERED, &active)
           && active)
          state = k;
        k++;
      });
      k = 0;
      avnd::for_each_field_ref(item, [&](auto& child) {
        if(k++ == state)
          this->walk(child, 1);
      });
    }
  }

  // ================= Standard controls =================
  // Layout items can reference inputs (editable) or outputs (display-only).
  template <typename P>
  void resolve_control(P member, int columns)
  {
    if constexpr(requires { avnd::get_inputs<T>(m_impl).*member; })
      control_widget(avnd::get_inputs<T>(m_impl).*member, columns);
    else if constexpr(requires { avnd::get_outputs<T>(m_impl).*member; })
      output_widget(avnd::get_outputs<T>(m_impl).*member, columns);
  }

  template <typename C>
  void output_widget(C& ctl, int columns)
  {
    if(columns == 1)
    {
      nk_layout_row_dynamic(&m_ctx, row_height, 2);
      label_for(ctl);
    }
    if constexpr(requires { avnd::map_control_to_01(ctl); })
    {
      const double norm = avnd::map_control_to_01(ctl);
      nk_size cur = (nk_size)(norm * 1000.);
      nk_progress(&m_ctx, &cur, 1000, nk_false);
    }
    else
    {
      label_for(ctl);
    }
  }

  template <typename C>
  void control_widget(C& ctl, int columns)
  {
    static constexpr auto widget = avnd::get_widget<C>();

    if(columns == 1)
    {
      // Own row: label + widget
      nk_layout_row_dynamic(&m_ctx, row_height, 2);
      label_for(ctl);
    }

    if constexpr(avnd::enum_parameter<C>)
    {
      enum_widget(ctl);
    }
    else if constexpr(avnd::float_parameter<C>)
    {
      static constexpr auto rng = avnd::get_range<C>();
      float v = ctl.value;
      const float step = ((float)rng.max - (float)rng.min) / 100.f;
      if constexpr(
          widget.widget == widget_type::knob
          || widget.widget == widget_type::log_knob)
        nk_knob_float(&m_ctx, rng.min, &v, rng.max, step, NK_DOWN, 15.f);
      else if constexpr(widget.widget == widget_type::spinbox)
        nk_property_float(&m_ctx, "#", rng.min, &v, rng.max, step, step);
      else
        nk_slider_float(&m_ctx, rng.min, &v, rng.max, step);
      if(v != (float)ctl.value)
        write_control(ctl, v);
    }
    else if constexpr(avnd::int_parameter<C>)
    {
      static constexpr auto rng = avnd::get_range<C>();
      int v = ctl.value;
      if constexpr(
          widget.widget == widget_type::knob
          || widget.widget == widget_type::iknob)
        nk_knob_int(&m_ctx, rng.min, &v, rng.max, 1, NK_DOWN, 15.f);
      else if constexpr(widget.widget == widget_type::spinbox)
        nk_property_int(&m_ctx, "#", rng.min, &v, rng.max, 1, 1.f);
      else
        nk_slider_int(&m_ctx, rng.min, &v, rng.max, 1);
      if(v != (int)ctl.value)
        write_control(ctl, v);
    }
    else if constexpr(avnd::bool_parameter<C>)
    {
      if constexpr(
          widget.widget == widget_type::button || widget.widget == widget_type::bang)
      {
        m_scratch.assign(avnd::get_name<C>());
        if(nk_button_label(&m_ctx, m_scratch.c_str()))
          write_control(ctl, true);
      }
      else
      {
        nk_bool v = (bool)ctl.value;
        m_scratch.assign(avnd::get_name<C>());
        if(nk_checkbox_label(&m_ctx, m_scratch.c_str(), &v))
          write_control(ctl, (bool)v);
      }
    }
    else if constexpr(avnd::string_parameter<C>)
    {
      char buf[256];
      const auto len
          = std::min(ctl.value.size(), sizeof(buf) - 1);
      std::copy_n(ctl.value.data(), len, buf);
      buf[len] = 0;
      nk_edit_string_zero_terminated(
          &m_ctx, NK_EDIT_FIELD, buf, sizeof(buf), nk_filter_default);
      if(ctl.value != buf)
        write_control(ctl, std::string(buf));
    }
    else
    {
      label_for(ctl);
    }
  }

  template <typename C>
  void label_for(C& ctl)
  {
    if constexpr(requires { std::string_view{avnd::get_name<C>()}; })
    {
      const std::string_view name = avnd::get_name<C>();
      nk_text(&m_ctx, name.data(), (int)name.size(), NK_TEXT_LEFT);
    }
    else
    {
      nk_spacer(&m_ctx);
    }
  }

  template <typename C>
  void enum_widget(C& ctl)
  {
    static constexpr auto choices = avnd::get_enum_choices<C>();
    constexpr int n = (int)choices.size();
    int idx = (int)static_cast<std::underlying_type_t<decltype(ctl.value)>>(ctl.value);
    if(idx < 0 || idx >= n)
      idx = 0;

    m_scratch.assign(choices[idx]);
    const auto sz = nk_vec2(
        nk_widget_width(&m_ctx), (float)(row_height * std::min(n, 8) + 8));
    if(nk_combo_begin_label(&m_ctx, m_scratch.c_str(), sz))
    {
      nk_layout_row_dynamic(&m_ctx, row_height, 1);
      for(int i = 0; i < n; i++)
      {
        m_scratch.assign(choices[i]);
        if(nk_combo_item_label(&m_ctx, m_scratch.c_str(), NK_TEXT_LEFT))
        {
          if(i != idx)
            write_control(ctl, static_cast<std::remove_cvref_t<decltype(ctl.value)>>(i));
        }
      }
      nk_combo_end(&m_ctx);
    }
  }

  // Central write path: mutate the control, fire its update() hook, notify
  // the host gesture protocol.
  // TODO proper begin/end edit edges from widget activity instead of
  // per-change perform_edit.
  template <typename C>
  void write_control(C& ctl, auto&& value)
  {
    ctl.value = std::forward<decltype(value)>(value);
    if constexpr(requires { ctl.update(m_impl.effect); })
      ctl.update(m_impl.effect);

    const int idx = param_index(ctl);
    if(idx >= 0)
    {
      double norm = 0.;
      if constexpr(requires { avnd::map_control_to_01(ctl); })
        norm = avnd::map_control_to_01(ctl);
      host.begin_edit(host.ctx, idx);
      host.perform_edit(host.ctx, idx, norm);
      host.end_edit(host.ctx, idx);
    }
    m_dirty = true;
  }

  template <typename C>
  int param_index(C& ctl)
  {
    int res = -1, k = 0;
    avnd::parameter_input_introspection<T>::for_all(
        avnd::get_inputs<T>(m_impl), [&](auto& field) {
          if((const void*)&field == (const void*)&ctl)
            res = k;
          k++;
        });
    return res;
  }

  // ================= Custom paint() widgets =================
  struct custom_draw
  {
    double x, y;
    void* widget;
    void (*paint)(void*, painter&);
  };

  template <typename Item>
  void custom_widget(Item& item)
  {
    using I = std::remove_cvref_t<Item>;
    const float w = (float)I::width();
    const float h = (float)I::height();

    nk_layout_row_static(&m_ctx, h, (int)w, 1);
    struct nk_rect bounds;
    const auto state = nk_widget(&bounds, &m_ctx);
    if(!state)
      return;

    if(state != NK_WIDGET_ROM && !m_capture && m_mouse_down && !m_mouse_was_down
       && nk_input_is_mouse_hovering_rect(&m_ctx.input, bounds))
    {
      const double lx = m_mouse_x - bounds.x;
      const double ly = m_mouse_y - bounds.y;
      bool grab = true;
      if constexpr(requires { (bool)item.mouse_press(lx, ly); })
        grab = item.mouse_press(lx, ly);
      else if constexpr(requires { item.mouse_press(lx, ly); })
        item.mouse_press(lx, ly);
      if(grab)
      {
        m_capture = (void*)&item;
        m_capture_x = bounds.x;
        m_capture_y = bounds.y;
        m_capture_move = [](void* p, double x, double y) {
          if constexpr(requires(I& i) { i.mouse_move(x, y); })
            static_cast<I*>(p)->mouse_move(x, y);
        };
        m_capture_release = [](void* p, double x, double y) {
          if constexpr(requires(I& i) { i.mouse_release(x, y); })
            static_cast<I*>(p)->mouse_release(x, y);
        };
      }
      m_dirty = true;
    }

    if constexpr(requires(painter& p) { item.paint(p); })
    {
      m_custom_draws.push_back(
          {bounds.x, bounds.y, (void*)&item, [](void* p, painter& pa) {
             static_cast<I*>(p)->paint(pa);
           }});
    }
  }

  // ================= Init: transactions, value sync, bus =================
  void init_items(auto& item)
  {
    using I = std::remove_cvref_t<decltype(item)>;
    if constexpr(is_custom_item<I>)
    {
      init_custom(item);
    }
    else if constexpr(is_any_container<I> || std::is_same_v<I, ui_t>)
    {
      avnd::for_each_field_ref(item, [this](auto& child) {
        using C = std::remove_cvref_t<decltype(child)>;
        if constexpr(
            is_any_container<C> || is_custom_item<C>)
          this->init_items(child);
      });
    }
  }

  template <typename Item>
  void init_custom(Item& item)
  {
    using I = std::remove_cvref_t<Item>;
    if constexpr(has_control_model<I>)
    {
      auto& ins = avnd::get_inputs<T>(m_impl);
      auto& ctl = ins.*(item.model);
      using C = std::remove_cvref_t<decltype(ctl)>;

      // Push the current control value into the widget
      if constexpr(requires { item.set_value(ctl, ctl.value); })
        item.set_value(ctl, ctl.value);

      // Wire the automation-gesture transaction (halp::transaction)
      if constexpr(requires { item.transaction.update; })
      {
        item.transaction.start = [this, &ctl] {
          if(const int idx = param_index(ctl); idx >= 0)
            host.begin_edit(host.ctx, idx);
        };
        item.transaction.update = [this, &item, &ctl](const auto& v) {
          if constexpr(requires { I::value_to_control(ctl, v); })
            ctl.value = I::value_to_control(ctl, v);
          else if constexpr(requires { ctl.value = v; })
            ctl.value = v;
          if constexpr(requires { ctl.update(m_impl.effect); })
            ctl.update(m_impl.effect);
          if(const int idx = param_index(ctl); idx >= 0)
          {
            double norm = 0.;
            if constexpr(requires { avnd::map_control_to_01(ctl); })
              norm = avnd::map_control_to_01(ctl);
            host.perform_edit(host.ctx, idx, norm);
          }
          m_dirty = true;
        };
        item.transaction.commit = [this, &ctl] {
          if(const int idx = param_index(ctl); idx >= 0)
            host.end_edit(host.ctx, idx);
        };
        item.transaction.rollback = [this, &ctl] {
          if(const int idx = param_index(ctl); idx >= 0)
            host.end_edit(host.ctx, idx);
        };
      }
    }
  }

  // Phase-1 bus transport: synchronous in-process delivery. The plug-in
  // bindings replace this with SPSC queues drained in process()/idle().
  void init_bus()
  {
    if constexpr(requires { typename ui_t::bus; })
    {
      m_bus.emplace();
      if constexpr(requires { m_impl.effect.process_message({}); }
                   || avnd::has_gui_to_processor_bus<T>)
      {
        if constexpr(requires { m_bus->send_message; })
        {
          m_bus->send_message = [this](auto&& msg) {
            if constexpr(requires { m_impl.effect.process_message(msg); })
              m_impl.effect.process_message(std::forward<decltype(msg)>(msg));
          };
        }
      }
      if constexpr(avnd::has_processor_to_gui_bus<T>)
      {
        if constexpr(requires { m_impl.effect.send_message; })
        {
          m_impl.effect.send_message = [this](auto&& msg) {
            ui_t::bus::process_message(m_ui, std::forward<decltype(msg)>(msg));
            m_dirty = true;
          };
        }
      }
      if constexpr(requires { m_bus->init(m_ui); })
        m_bus->init(m_ui);
    }
  }

  struct no_bus
  {
  };
  static constexpr auto bus_type()
  {
    if constexpr(requires { sizeof(typename ui_t::bus); })
      return std::type_identity<typename ui_t::bus>{};
    else
      return std::type_identity<no_bus>{};
  }
  using bus_t = typename decltype(bus_type())::type;

  avnd::effect_container<T>& m_impl;
  font_registry m_fonts;
  nk_canvas_renderer m_renderer;
  nk_user_font m_font{};
  nk_context m_ctx{};
  ui_t m_ui{};
  std::optional<bus_t> m_bus;
  std::optional<canvas_ity::canvas> m_canvas;
  std::vector<event> m_events;
  std::vector<custom_draw> m_custom_draws;
  std::map<void*, int> m_tab_state;
  std::string m_scratch;

  int m_width{640}, m_height{480};
  double m_scale{1.};
  double m_mouse_x{}, m_mouse_y{};
  bool m_mouse_down{}, m_mouse_was_down{};
  bool m_dirty{true};
  bool m_needs_clear{};

  void* m_capture{};
  double m_capture_x{}, m_capture_y{};
  void (*m_capture_move)(void*, double, double){};
  void (*m_capture_release)(void*, double, double){};
};
}

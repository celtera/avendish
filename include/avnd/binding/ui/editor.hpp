#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

/**
 * Editor resolution shared by the plug-in bindings (CLAP, VST2, VST3, ...):
 *
 *  - T::ui::window satisfying avnd::gui_windowed_ui wins: the author brings
 *    any UI framework;
 *  - otherwise, when the target is built with the soft UI backend
 *    (AVND_SOFT_UI_EDITOR, set by avnd_target_soft_ui), a declarative
 *    `struct ui` gets the reference pugl + Nuklear + canvas_ity editor;
 *  - otherwise there is no editor and the binding does not expose one.
 */

#include <avnd/concepts/gui_window.hpp>
#include <avnd/introspection/layout.hpp>
#include <avnd/wrappers/effect_container.hpp>

#if defined(AVND_SOFT_UI_EDITOR)
#include <avnd/binding/ui/soft/window.hpp>
#endif

#include <memory>
#include <type_traits>

namespace avnd
{
template <typename T>
concept has_layout_ui = requires { sizeof(typename avnd::ui_type<T>::type); };

namespace detail
{
template <typename T>
static constexpr auto ui_editor_type()
{
  if constexpr(avnd::has_gui_window<T>)
    return std::type_identity<typename T::ui::window>{};
#if defined(AVND_SOFT_UI_EDITOR)
  else if constexpr(has_layout_ui<T>)
    return std::type_identity<avnd::soft_ui::window_editor<T>>{};
#endif
  else
    return std::type_identity<void>{};
}
}

template <typename T>
using ui_editor_t = typename decltype(detail::ui_editor_type<T>())::type;

template <typename T>
static constexpr bool has_ui_editor = !std::is_void_v<ui_editor_t<T>>;

template <typename T>
  requires has_ui_editor<T>
auto make_ui_editor(avnd::effect_container<T>& fx)
{
  using editor_t = ui_editor_t<T>;
  if constexpr(std::is_constructible_v<editor_t, avnd::effect_container<T>&>)
    return std::make_unique<editor_t>(fx);
  else
    return std::make_unique<editor_t>();
}
}

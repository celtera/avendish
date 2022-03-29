#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <avnd/binding/ui/qml/enum_control.hpp>
#include <avnd/binding/ui/qml/float_control.hpp>
#include <avnd/binding/ui/qml/int_control.hpp>
#include <avnd/binding/ui/qml/toggle_control.hpp>
#include <avnd/common/for_nth.hpp>
#include <avnd/introspection/input.hpp>
#include <avnd/introspection/layout.hpp>
#include <avnd/introspection/messages.hpp>

#include <QDebug>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQmlProperty>
#include <QQuickItem>

#include <verdigris>

namespace qml
{

template <typename T>
class qml_layout_ui_base
{
public:
  using type = T;
  avnd::effect_container<T>& implementation;

  std::string componentData;
  QQuickItem* item{};
  QQmlApplicationEngine engine;

  explicit qml_layout_ui_base(avnd::effect_container<T>& impl)
      : implementation{impl}
  {
    componentData.reserve(20000);
  }

  void create(qml_layout_ui_base& self, auto& c, int control_k) { }

  template <typename Val>
  void value_changed(int idx, const Val& value)
  {
    avnd::parameter_input_introspection<T>::for_nth_raw(
        avnd::get_inputs(implementation),
        idx,
        [value]<typename C>(C& ctl) { ctl.value = value; });
  }

  template <typename Val>
  QString value_display(int idx, const Val& value)
  {
    QString res;

    avnd::parameter_input_introspection<T>::for_nth_raw(
        avnd::get_inputs(implementation),
        idx,
        [&res, value]<typename C>(C& ctl)
        {
          char buf[128] = {0};
          if constexpr (requires { ctl.display(buf, value); })
          {
            ctl.display(buf, value);
            res = QString::fromUtf8(buf);
          }
          else
          {
            res = QString::number(value);
          }
        });

    return res;
  }
};

template <typename T>
class qml_layout_ui
    : public QObject
    , public qml_layout_ui_base<T>
    , public int_control
    , public enum_control
    , public float_control
    , public toggle_control
{
  W_OBJECT(qml_layout_ui)

  using enum_control::create;
  using float_control::create;
  using int_control::create;
  using toggle_control::create;
  using qml_layout_ui_base<T>::create;

  int depth = 0;

public:
  explicit qml_layout_ui(avnd::effect_container<T>& impl)
      : qml_layout_ui_base<T>{impl}
  {
    this->engine.rootContext()->setContextProperty("_uiHandler", this);
    using namespace std::literals;

    /// Here we generate a QML file content
    // The header
    append(R"_(import QtQuick 2.15
import QtQuick.Layouts 1.15
import QtQuick.Controls 2.15
ApplicationWindow {{
  visible: true
  Item {{ objectName: "mainItem"; anchors.fill: parent


)_");

    // The layout
    createLayout();

    append("\n  }}\n}}\n");

    /// Create and instantiate the QML component
    this->engine.loadData(QByteArray::fromStdString(this->componentData), QUrl());
    auto objs = this->engine.rootObjects();
    assert(!objs.empty());

    this->item = objs[0]->template findChild<QQuickItem*>(QStringLiteral("mainItem"));
    assert(this->item);
  }

  template <typename... Args>
  void append(fmt::format_string<Args...> s, Args&&... args)
  {
    fmt::format_to(
        std::back_inserter(this->componentData), s, std::forward<Args>(args)...);
  }

  void recurseItem(const auto& item)
  {
    avnd::for_each_field_ref(item, [this](auto& child) { this->createItem(child); });
  }

  static inline int tab_bar_k = 0;
  void createWidget(const auto& item)
  {
    if constexpr (requires {
                    {
                      item
                      } -> std::convertible_to<std::string_view>;
                  })
    {
      append(
          R"_(Label {{ text: "{}";  }}
)_",
          item);
    }
    else if constexpr (requires { (avnd::get_inputs<T>(this->implementation).*item); })
    {
      auto& ins = avnd::get_inputs<T>(this->implementation);
      auto& control = ins.*item;
      create(*this, control, avnd::index_in_struct(ins, item));
    }
  }

  void createTabBar(const auto& item)
  {
    avnd::for_each_field_ref(
        item,
        [this]<typename Item>(const Item& child) {
          append(R"_(TabButton {{ text: "{}"; width: implicitWidth }})_", Item::name());
        });
  }

  template <typename Item>
  void createItem(const Item& item)
  {
    if constexpr (requires { item.spacing; })
    {
      append("Item {{ width: {}; height: {} }}\n", Item::width(), Item::height());
    }
    if constexpr (requires { item.hbox; })
    {
      append("RowLayout {{ width: parent.width;  \n");
      depth++;
      recurseItem(item);
      depth--;
      append("}}\n");
    }
    else if constexpr (requires { item.vbox; })
    {
      append("ColumnLayout {{ width: parent.width; \n");
      depth++;
      recurseItem(item);
      depth--;
      append("\n}}\n");
    }
    else if constexpr (requires { item.split; })
    {
      append("SplitView {{ width: {}; height: {} \n", Item::width(), Item::height());
      depth++;
      recurseItem(item);
      depth--;
      append("\n}}\n");
    }
    else if constexpr (requires { item.group; })
    {
      append("GroupBox {{ width: parent.width; title: \"{}\" \n", Item::name());
      depth++;
      recurseItem(item);
      depth--;
      append("\n}}\n");
    }
    else if constexpr (requires { item.tabs; })
    {
      const int bar = tab_bar_k++;
      append("TabBar {{ width: parent.width; id: tabbar_{}\n", bar);
      depth++;
      createTabBar(item);
      depth--;
      append("\n}}\n");
      append(
          "StackLayout {{ width: parent.width; currentIndex: tabbar_{}.currentIndex\n",
          bar);
      depth++;
      recurseItem(item);
      depth--;
      append("\n}}\n");
    }
    else
    {
      // Normal widget
      createWidget(item);
    }
  }

  void createLayout()
  {
    auto& lay = avnd::get_ui(this->implementation);
    createItem(lay);
  }

  void floatChanged(int idx, float value) noexcept
  {
    float_control::changed(*this, idx, value);
  }
  W_SLOT(floatChanged, (int, float))
  QString floatDisplay(int idx, float value) noexcept
  {
    return float_control::display(*this, idx, value);
  }
  W_SLOT(floatDisplay, (int, float))

  void intChanged(int idx, int value) noexcept
  {
    int_control::changed(*this, idx, value);
  }
  W_SLOT(intChanged, (int, int))
  QString intDisplay(int idx, int value) noexcept
  {
    return int_control::display(*this, idx, value);
  }
  W_SLOT(intDisplay, (int, int))

  void enumChanged(int idx, int value) noexcept
  {
    enum_control::changed(*this, idx, value);
  }
  W_SLOT(enumChanged, (int, int))

  void toggleChanged(int idx, bool value) noexcept
  {
    toggle_control::changed(*this, idx, value);
  }
  W_SLOT(toggleChanged, (int, bool))
};
}

#include <wobjectimpl.h>
W_OBJECT_IMPL(qml::qml_layout_ui<T>, template <typename T>)

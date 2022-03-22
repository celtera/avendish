#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <avnd/binding/ui/qml/enum_control.hpp>
#include <avnd/binding/ui/qml/float_control.hpp>
#include <avnd/binding/ui/qml/int_control.hpp>
#include <avnd/binding/ui/qml/toggle_control.hpp>
#include <avnd/introspection/input.hpp>
#include <avnd/introspection/messages.hpp>

#include <QDebug>
#include <QGuiApplication>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQmlProperty>
#include <QQuickItem>
#include <QQuickView>

#include <verdigris>

namespace qml
{

template <typename T>
class qml_ui_base
{
public:
  using type = T;
  avnd::effect_container<T>& implementation;

  std::string componentData;
  QQuickItem* item{};
  QQuickView view;
  QQmlComponent comp;

  explicit qml_ui_base(avnd::effect_container<T>& impl)
      : implementation{impl}
      , comp{view.engine()}
  {
    componentData.reserve(20000);
  }

  void create(qml_ui_base& self, auto& c, int control_k) { }

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
class qml_ui
    : public QObject
    , public qml_ui_base<T>
    , public int_control
    , public enum_control
    , public float_control
    , public toggle_control
{
  W_OBJECT(qml_ui)
  //using Parent = float_control<T, int_control<T, qml_ui_base<T>>>;

  using enum_control::create;
  using float_control::create;
  using int_control::create;
  using toggle_control::create;
  using qml_ui_base<T>::create;

public:
  explicit qml_ui(avnd::effect_container<T>& impl)
      : qml_ui_base<T>{impl}
  {
    this->view.rootContext()->setContextProperty("_uiHandler", this);
    using namespace std::literals;

    /// Here we generate a QML file content
    // The header
    fmt::format_to(
        std::back_inserter(this->componentData),
        R"_(import QtQuick 2.15
import QtQuick.Layouts 1.15
import QtQuick.Controls 2.15

Item {{
  ColumnLayout {{
   Text {{
     text: "{}"
   }}
  RowLayout {{

)_",
        T::name());

    // The controls
    int control_k = 0;
    avnd::input_introspection<T>::for_all(
        avnd::get_inputs(this->implementation),
        [this, &control_k]<typename C>(C& c)
        {
          if constexpr (avnd::parameter<C>)
            create(*this, c, control_k);

          control_k++;
        });

    // The footer
    fmt::format_to(std::back_inserter(this->componentData), "\n    }}\n  }}\n}}\n");

    /// Create and instantiate the QML component
    QObject::connect(
        &this->comp,
        &QQmlComponent::statusChanged,
        this,
        [this](auto status)
        {
          this->item = qobject_cast<QQuickItem*>(
              this->comp.create(this->view.engine()->rootContext()));
          assert(this->item);
          this->view.setContent(QUrl(), &this->comp, this->item);
        });

    qDebug() << this->componentData.c_str();

    this->comp.setData(QByteArray::fromStdString(this->componentData), QUrl());
    this->view.show();
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
W_OBJECT_IMPL(qml::qml_ui<T>, template <typename T>)

/* SPDX-License-Identifier: GPL-3.0-or-later */

R"_(
ColumnLayout {{
    Slider {{
      id: control_{0}
      objectName: "control_{0}"
      from: {1}
      to: {2}
      value: {3}
      Layout.maximumWidth: 130
      Layout.maximumHeight: 50
      onValueChanged: _uiHandler.floatChanged({0}, value)
      Layout.alignment: Qt.AlignHCenter
    }}
    Label {{
      text: "{5}: " + _uiHandler.floatDisplay({0}, control_{0}.value)
      horizontalAlignment: Text.AlignHCenter
      Layout.alignment: Qt.AlignHCenter
    }}
    Layout.maximumHeight: 100
    Layout.alignment: Qt.AlignVCenter
}}
)_"

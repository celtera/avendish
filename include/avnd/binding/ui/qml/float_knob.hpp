/* SPDX-License-Identifier: GPL-3.0-or-later */

R"_(
ColumnLayout {{
    Dial {{
      id: control_{0}
      objectName: "control_{0}"
      from: {1}
      to: {2}
      value: {3}
      inputMode: Dial.Vertical
      Layout.maximumWidth: 80
      Layout.maximumHeight: 80
      onValueChanged: _uiHandler.floatChanged({0}, value)
      Layout.alignment: Qt.AlignHCenter
    }}
    Label {{
      text: "{5}\n" + _uiHandler.floatDisplay({0}, control_{0}.value)
      horizontalAlignment: Text.AlignHCenter
      Layout.alignment: Qt.AlignHCenter
      Layout.maximumWidth: 80
      Layout.minimumWidth: 80
    }}
    Layout.maximumHeight: 100
    Layout.maximumWidth: 100
    Layout.alignment: Qt.AlignVCenter
}}
)_"

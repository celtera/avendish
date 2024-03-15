/* SPDX-License-Identifier: GPL-3.0-or-later */

R"_(
ColumnLayout {{
    Switch {{
      id: control_{0}
      objectName: "control_{0}"
      checked: {1}
      onCheckedChanged: _uiHandler.toggleChanged({0}, checked)
      Layout.alignment: Qt.AlignHCenter
    }}
    Label {{
      text: "{2}\n" + _uiHandler.toggleDisplay({0}, control_{0}.checked)
      horizontalAlignment: Text.AlignHCenter
      Layout.alignment: Qt.AlignHCenter
    }}
    Layout.alignment: Qt.AlignVCenter
}}
)_"

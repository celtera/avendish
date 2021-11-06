/* SPDX-License-Identifier: GPL-3.0-or-later */

R"_(
ColumnLayout {{
    CheckBox {{
      id: control_{0}
      objectName: "control_{0}"
      checkState: {1} ? Qt.Checked : Qt.Unchecked
      text: "{2}"
      Layout.maximumWidth: 200
      Layout.maximumHeight: 40
      onCheckStateChanged: _uiHandler.toggleChanged({0}, checkState == Qt.Checked)
      Layout.alignment: Qt.AlignHCenter
    }}
    Layout.maximumHeight: 40
    Layout.maximumWidth: 220
    Layout.alignment: Qt.AlignVCenter
}}
)_"

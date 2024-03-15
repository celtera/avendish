/* SPDX-License-Identifier: GPL-3.0-or-later */

R"_(
ColumnLayout {{
    ComboBox {{
      id: control_{0}
      objectName: "control_{0}"
      model: [ {1} ]
      currentIndex: {2}
      Layout.maximumWidth: 100
      Layout.maximumHeight: 80
      onCurrentIndexChanged: _uiHandler.enumChanged({0}, currentIndex)
      Layout.alignment: Qt.AlignHCenter
    }}
    Label {{
      text: "{3}\n"
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

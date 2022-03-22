/* SPDX-License-Identifier: GPL-3.0-or-later */

R"_(
ColumnLayout {{
     Dial {{
       id: control_{0}
       objectName: "control_{0}"
       from: {1}
       to: {2}
       value: {3}
       stepSize: {4}
       inputMode: Dial.Vertical
       snapMode: Dial.SnapAlways
       Layout.maximumWidth: 80
       Layout.maximumHeight: 80
       onValueChanged: _uiHandler.intChanged({0}, value)
       Layout.alignment: Qt.AlignHCenter
     }}
     Label {{
       text: "{5}: " + _uiHandler.intDisplay({0}, control_{0}.value)
       horizontalAlignment: Text.AlignHCenter
       Layout.alignment: Qt.AlignHCenter
     }}
     Layout.maximumHeight: 100
     Layout.alignment: Qt.AlignVCenter
 }}
)_"

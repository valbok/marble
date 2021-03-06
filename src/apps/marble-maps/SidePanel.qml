//
// This file is part of the Marble Virtual Globe.
//
// This program is free software licensed under the GNU LGPL. You can
// find a copy of this license in LICENSE.txt in the top directory of
// the source code.
//
// Copyright 2017      Dennis Nienhüser <nienhueser@kde.org>
//

import QtQuick 2.3
import QtQuick.Controls 2.0
import QtQuick.Window 2.2
import org.kde.marble 0.20

Drawer {
    id: root

    property var marbleMaps
    property alias showAccessibility: accessibilityAction.checked
    signal aboutActionTriggered()

    Settings {
        id: settings

        property bool showUpdateInfo: Number(value("MarbleMaps", "updateInfoVersion", "0")) < 1

        Component.onDestruction: {
            settings.setValue("MarbleMaps", "showAccessibility", accessibilityAction.checked ? "true" : "false")
            settings.setValue("MarbleMaps", "showPublicTransport", publicTransportAction.checked ? "true" : "false")
        }
    }

    Column {
        id: drawerContent
        anchors.fill: parent
        spacing: Screen.pixelDensity * 2

        Image {
            source: "drawer.svg"
            width: parent.width
            sourceSize.width: width
            fillMode: Image.PreserveAspectFit
        }

        MenuIcon {
            id: publicTransportAction
            anchors.leftMargin: Screen.pixelDensity * 2
            anchors.rightMargin: anchors.leftMargin

            checkable: true
            checked: settings.value("MarbleMaps", "showPublicTransport", "false") === "true"
            text: qsTr("Public Transport")
            icon: "qrc:/material/bus.svg"
            onTriggered: {
                root.marbleMaps.showPublicTransport = checked
                root.close()
            }
        }

        MenuIcon {
            id: accessibilityAction
            anchors.leftMargin: Screen.pixelDensity * 2
            anchors.rightMargin: anchors.leftMargin
            checkable: true
            checked: settings.value("MarbleMaps", "showAccessibility", "false") === "true"
            text: qsTr("Accessibility")
            icon: "qrc:/material/wheelchair.svg"
            onTriggered: root.close()
        }

        Rectangle {
            width: parent.width
            height: 1
            color: "gray"
        }

        MenuIcon {
            text: qsTr("About Marble Maps…")
            anchors.leftMargin: Screen.pixelDensity * 2
            anchors.rightMargin: anchors.leftMargin
            onTriggered: {
                root.close()
                root.aboutActionTriggered()
            }
        }
    }

}

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import QGroundControl
import QGroundControl.Controls
import QGroundControl.Palette
import QGroundControl.ScreenTools

Rectangle {
    color:          qgcPal.window
    anchors.fill:   parent
    

    readonly property real _margins: ScreenTools.defaultFontPixelHeight

    QGCPalette { id: qgcPal; colorGroupEnabled: true }

    ColumnLayout {
        anchors.leftMargin:  _margins
        anchors.rightMargin: _margins
        anchors.top:   parent.top
        anchors.left:  parent.left
        anchors.right: parent.right
        spacing:        20

        Rectangle {
            Layout.fillWidth: true

            height: 160
            radius: 14
            color: '#121212'              // nền đen tối giản
            antialiasing: true

            // đổ bóng nhẹ cho card
            layer.enabled: true

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 20
                spacing: 8

                QGCLabel {
                    text: "CTUAV GroundControl"
                    font.pixelSize: ScreenTools.defaultFontPixelHeight * 1.2
                    font.bold: true
                    color: "white"
                }

                QGCLabel {
                    text: "Get the latest release of CTUAV GroundControl for your system"
                    color: "#cccccc"
                    font.pixelSize: ScreenTools.defaultFontPixelHeight * 0.7
                }

                RowLayout {
                    spacing: 6

                    QGCLabel {
                        id: linkLabel
                        text: "<a href=\"https://ctuav-dowloadpage.vercel.app/\">Visit Page</a>"
                        color: "white"
                        linkColor: "white"
                        font.pixelSize: ScreenTools.defaultFontPixelHeight * 0.8
                        font.bold: true
                        onLinkActivated: (link) => Qt.openUrlExternally(link)
                    }

                    QGCLabel {
                        id: arrowLabel
                        text: "↗"
                        color: "white"
                        font.pixelSize: ScreenTools.defaultFontPixelHeight * 0.8
                        font.bold: true
                    }
                }
            }

            MouseArea {
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor

                onEntered: {
                    linkLabel.linkColor = "#00bfff"   // xanh nhạt khi hover
                    arrowLabel.color = "#00bfff"
                }
                onExited: {
                    linkLabel.linkColor = "white"
                    arrowLabel.color = "white"
                }

                onClicked: Qt.openUrlExternally("https://ctuav-dowloadpage.vercel.app/")
            }
        }


        // 📚 PHẦN PHỤ: Resources
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 10

            QGCLabel {
                text: "Resources"
                font.bold: true
                font.pixelSize: 18
                color: qgcPal.text
            }

            Repeater {
                model: [
                    { title: "QGroundControl User Guide", url: "https://docs.qgroundcontrol.com" },
                    { title: "PX4 Users Discussion Forum", url: "http://discuss.px4.io/c/qgroundcontrol" },
                    { title: "ArduPilot Users Forum", url: "https://discuss.ardupilot.org/c/ground-control-software/qgroundcontrol" },
                    { title: "QGroundControl Discord Channel", url: "https://discord.com/channels/1022170275984457759/1022185820683255908" }
                ]

                delegate: Rectangle {
                    Layout.fillWidth: true
                    height: 50
                    radius: 8
                    color: qgcPal.windowShade
                    border.color: "#dddddd"
                    border.width: 1

                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: 12
                        spacing: 12

                        QGCLabel {
                            text: modelData.title
                            font.bold: true
                        }
                        Item { Layout.fillWidth: true }
                        QGCLabel {
                            text: "<a href=\"" + modelData.url + "\">Open</a>"
                            linkColor: "skyblue"
                            onLinkActivated: (link) => Qt.openUrlExternally(link)
                        }
                    }

                    MouseArea {
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onEntered: parent.color = qgcPal.buttonHighlight
                        onExited: parent.color = qgcPal.windowShade
                        onClicked: Qt.openUrlExternally(modelData.url)
                    }
                }
            }
        }
    }
}

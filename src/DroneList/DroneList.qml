import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import Qt5Compat.GraphicalEffects
import QGroundControl
import QGroundControl.Controls
import QGroundControl.Controllers
import QGroundControl.Palette
import CTUAV.DroneData


Rectangle {
    anchors.fill: parent
    color: qgcPal.window
    // color: detailPageLoader.active ? "transparent" : qgcPal.window

    property var droneTypes: ["Quadcopter Type", "Hexacopter Type", "Lightshow", "Firefighting"]

    DroneDataLoader {
        id: droneDataLoader
    }

    property var droneMap: droneDataLoader.droneMap

    Component.onCompleted: {
        droneDataLoader.loadFromFile(":/json/DroneDataLoader.DroneData.json")
        // console.log("Loaded map:", JSON.stringify(droneMap))
    }

    
    property string selectedType: droneTypes[0]
    property bool isMobile: ScreenTools.isMobile
    property int fontSize: isMobile ? 10 : 10
    property var selectedDrone: null
    property bool showDetailOverlay: false

    property bool showImageGallery: false


    function onDroneItemClicked(drone) {
        selectedDrone = drone
        // detailPageLoader.active = true
        showDetailOverlay = true
    }

    Item {
        id: droneListWrapper
        anchors.fill: parent
        visible: true

        // visible: !detailPageLoader.active

        // Row với 3 phần
        Row {
            anchors.fill: parent
            anchors.margins: 10
            spacing: 10

            // Cột 1 - Danh sách loại drone (1 phần)
            Item {
                width: parent.width * 0.15 // Tỉ lệ 1
                height: parent.height

                ListView {
                    anchors.fill: parent
                    spacing: 8
                    model: droneTypes
                    delegate: Item {
                        width: parent.width
                        height: 40
                        property bool hovered: false

                        Rectangle {
                            anchors.fill: parent
                            radius: 4
                            border.color: "#3d81c2"
                            border.width: selectedType === modelData ? 1 : 0
                            color: selectedType === modelData ? "#3d81c2"
                                : hovered ? "#98ccff" : "transparent"

                            Behavior on color {
                                ColorAnimation { duration: 150 }
                            }

                            MouseArea {
                                anchors.fill: parent
                                hoverEnabled: true
                                onClicked: selectedType = modelData
                                onEntered: hovered = true
                                onExited: hovered = false
                                cursorShape: Qt.PointingHandCursor
                            }

                            Text {
                                anchors.centerIn: parent
                                text: modelData
                                color: qgcPal.globalTheme === QGCPalette.Light ? selectedType === modelData ? "#fff" : qgcPal.text : qgcPal.text
                                font.pixelSize: fontSize 
                                // font.bold: true
                            }
                        }
                    }
                }
            }

            // Cột 2 - Danh sách drone theo loại (3 phần)
            Item {
                width: parent.width * 0.85 // Tỉ lệ 3
                height: parent.height

                GridView {
                    anchors.fill: parent
                    // cellWidth: isMobile ? (width) : (width / 4)
                    // cellHeight: cellWidth
                    cellWidth: isMobile ? (width / 3) -  12 : (width / 3) -  12  // trừ đi khoảng spacing
                    cellHeight: cellWidth

                    model: droneMap[selectedType]

                    delegate: Column {
                        width: GridView.view.cellWidth
                        height: GridView.view.cellHeight
                        spacing: isMobile ? 12 : 12

                        Item {
                            width: parent.width * 0.9
                            height: width
                            anchors.horizontalCenter: parent.horizontalCenter

                            Rectangle {
                                id: droneCard
                                width: parent.width
                                height: parent.height
                                radius: 8
                                color: qgcPal.window
                                border.color: droneMouseArea.containsMouse ? "#0069b4" :"#333333"
                                border.width: 1
                                clip: true

                                Image {
                                    anchors.fill: parent
                                    source: modelData.image
                                    fillMode: Image.PreserveAspectFit
                                }

                                MouseArea {
                                    id: droneMouseArea
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    onClicked: onDroneItemClicked(modelData)
                                    cursorShape: Qt.PointingHandCursor
                                }
                                Text {
                                    width: parent.width
                                    anchors.bottom: parent.bottom
                                    anchors.bottomMargin: 8
                                    horizontalAlignment: Text.AlignHCenter
                                    wrapMode: Text.WordWrap
                                    text: modelData.name
                                    font.pixelSize: fontSize * 1.2
                                    color: qgcPal.text
                                }
                            }

                            DropShadow {
                                anchors.fill: droneCard
                                source: droneCard
                                radius: 12
                                samples: 24
                                color: "#9f101010"
                                horizontalOffset: 2
                                verticalOffset: 4
                                visible: droneMouseArea.containsMouse
                            }
                        }

                        
                    }
                }
            }
        }

        // Cột 3 - Chi tiết drone (1 phần)
        Item {
            id: detailPanelWrapper
            anchors.fill: parent
            z: 10
            Rectangle {
                id: overlay
                anchors.fill: parent
                color: "#bc1f1f1f"
                visible: showDetailOverlay
                z: 1
                opacity: showDetailOverlay ? 1 : 0

                Behavior on opacity {
                    NumberAnimation { duration: 200 }
                }

                MouseArea {
                    anchors.fill: parent
                    onClicked: {
                        showDetailOverlay = false
                        selectedDrone = null
                    }
                }
            }
            Rectangle {
                id: detailPanel
                width: parent.width * 0.5
                height: parent.height
                color: qgcPal.window
                border.color:  "#3b3b3b"
                border.width: 1
                radius: 8
                z: 2
                clip: true

                x: showDetailOverlay ? parent.width - width : parent.width

                Behavior on x {
                    NumberAnimation {
                        duration: 300
                        easing.type: Easing.InOutQuad
                    }
                }
                ColumnLayout {
                anchors.fill: parent
                anchors.margins: 20
                spacing: 14
                ScrollView {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    Column {
                        width: detailPanel.width - 60
                        spacing: 14
                        padding: 6
                        Column {
                            spacing: 12

                            Row {
                                spacing: 12
                                Rectangle {
                                    width: detailPanel.width * 0.4
                                    height: width
                                    color: qgcPal.window
                                    radius: 6

                                    Image {
                                        anchors.fill: parent
                                        anchors.margins: 4
                                        source: selectedDrone ? selectedDrone.image : ""
                                        fillMode: Image.PreserveAspectFit
                                    }
                                }
                                Column {
                                    spacing: 8

                                    Text {
                                        text: selectedDrone ? selectedDrone.name : ""
                                        font.pixelSize: fontSize * 2
                                        font.bold: true
                                        color: qgcPal.text
                                        wrapMode: Text.WordWrap
                                    }

                                    Text {
                                        text: selectedDrone ? qsTr("Code: ") + selectedDrone.code : ""
                                        font.pixelSize: fontSize * 1.2
                                        color: qgcPal.text
                                        wrapMode: Text.WordWrap
                                    }
                                    Item {
                                        width: isMobile ? 80 : 80
                                        height: isMobile ? 26 : 26

                                        Rectangle {
                                            anchors.fill: parent
                                            radius: 4
                                            color: mouseArea1.pressed ? "#444" : (mouseArea1.containsMouse ? "#333" : "#111")
                                            border.color: "#666"
                                            border.width: 1

                                            Text {
                                                anchors.centerIn: parent
                                                text: qsTr("See more photos")
                                                color: "white"
                                                font.pixelSize: fontSize
                                            }

                                            MouseArea {
                                                id: mouseArea1
                                                anchors.fill: parent
                                                onClicked: {
                                                    showImageGallery = true
                                                    // if (selectedDrone && selectedDrone.link)
                                                    //     Qt.openUrlExternally(selectedDrone.link)
                                                }
                                                hoverEnabled: true
                                                cursorShape: Qt.PointingHandCursor
                                            }
                                        }
                                    }
                                }
                            }
                            Column {
                                spacing: 4

                                Text {
                                    text: qsTr("Describe:")
                                    font.bold: true
                                    color: qgcPal.text
                                    font.pixelSize: fontSize * 1.2
                                }

                                Text {
                                    text: selectedDrone.description !== null ? selectedDrone.description : qsTr("No description available")
                                    wrapMode: Text.WordWrap
                                    font.pixelSize: fontSize
                                    color: qgcPal.text

                                }
                            }
                        }

                        Repeater {
                            model: selectedDrone ? [
                                { label: qsTr("Structure"), value: selectedDrone.structure_type },
                                { label: qsTr("Material"), value: selectedDrone.material },
                                { label: qsTr("Size"), value: selectedDrone.size },
                                { label: qsTr("Number of Axes"), value: selectedDrone.number_of_axes },
                                { label: qsTr("Wheelbase"), value: selectedDrone.wheelbase },
                                { label: qsTr("Weight"), value: selectedDrone.weight },
                                { label: qsTr("Payload"), value: selectedDrone.loading },
                                { label: qsTr("Flight Speed"), value: selectedDrone.flightspeed },
                                { label: qsTr("Max Altitude"), value: selectedDrone.Height },
                                { label: qsTr("Control Range"), value: selectedDrone.max_remote_control },
                                { label: qsTr("Power Mode"), value: selectedDrone.power_mode },
                                { label: qsTr("Operating Temperature"), value: selectedDrone.operating_temp },
                                { label: qsTr("Max Tilt Angle"), value: selectedDrone.max_tilt_angle },
                                { label: qsTr("Ascending Speed"), value: selectedDrone.max_rising_speed },
                                { label: qsTr("Descending Speed"), value: selectedDrone.max_down_speed },
                                { label: qsTr("Wind Resistance"), value: selectedDrone.max_resist_wind_speed },
                                { label: qsTr("Flight Time"), value: selectedDrone.overing_time },
                                { label: qsTr("Battery"), value: selectedDrone.battery },
                                { label: qsTr("Propeller"), value: selectedDrone.propeller },
                                { label: qsTr("Camera"), value: selectedDrone.camera },
                                { label: qsTr("LED Color"), value: selectedDrone.LED_color },
                                { label: qsTr("LED Power"), value: selectedDrone.LED_power },
                                { label: qsTr("Load Type"), value: selectedDrone.load_type },
                                { label: qsTr("Communication Mode"), value: selectedDrone.communication_mode },
                                { label: qsTr("Working Mode"), value: selectedDrone.working_mode },
                                { label: qsTr("Location Mode"), value: selectedDrone.location_mode }
                            ].filter(entry => entry.value !== undefined) : []

                            delegate: Row {
                                spacing: 8
                                Text {
                                    text: modelData.label + ":"
                                    font.bold: true
                                    font.pixelSize: fontSize
                                    width: 80
                                    wrapMode: Text.WordWrap
                                    color: qgcPal.text
                                }
                                Text {
                                    text: modelData.value
                                    width: detailPanel.width - 170
                                    font.pixelSize: fontSize
                                    wrapMode: Text.WordWrap
                                    color: qgcPal.text
                                }
                            }
                        }
                    }
                }

                RowLayout {
                    Layout.alignment: Qt.AlignRight
                    spacing: 8
                    visible: false //true

                    Item {
                        width: confirmText.paintedWidth + 20
                        height: confirmText.paintedHeight + 10
                        
                        Rectangle {
                            anchors.fill: parent
                            radius: 4
                            color: qgcPal.globalTheme === QGCPalette.Light ?  mouseArea2.pressed ? "#333" : "#222" : mouseArea2.pressed ? "#0061a2" : "#0070ba"

                            Text {
                                id: confirmText
                                anchors.centerIn: parent
                                text: qsTr("Confirm Upload Parameters")
                                color: "white"
                                font.pixelSize: fontSize
                            }

                            MouseArea {
                                id: mouseArea2
                                anchors.fill: parent
                                onClicked: {
                                    // sau nay sửa thành gọi api upload file tương ứng
                                    droneDataLoader.confirmUploadParameters(":/parameters/resources/parameters/Quad_N1.params")
                                    showDetailOverlay = false
                                    selectedDrone = null
                                    // thêm thông báo đã upload thành công
                                    // ...
                                }
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                            }
                        }
                    }
                }


            }

            }

        }

       Rectangle {
            id: imageGalleryOverlay
            anchors.fill: parent
            color:  "#80000000" 
            visible: showImageGallery
            z: 99

            MouseArea {
                anchors.fill: parent
                onClicked: showImageGallery = false
            }

            Rectangle {
                anchors.fill: parent
                color: "transparent"
                clip: true

                // Nút đóng
                Rectangle {
                    id: closeButton
                    width: isMobile ? 28 : 28
                    height: width
                    radius: 18
                    color: qgcPal.globalTheme === QGCPalette.Light ? "#d3d3d3" : "#3d3d3d"
                    anchors.top: parent.top
                    anchors.right: parent.right
                    anchors.margins: 12

                   QGCColoredImage {
                        anchors.margins:    parent.height / 4
                        anchors.fill:       parent
                        source:             "/res/XDelete.svg"
                        fillMode:           Image.PreserveAspectFit
                        color:              qgcPal.text
                    }

                    MouseArea {
                        anchors.fill: parent
                        onClicked: showImageGallery = false
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                    }
                }

                Column {
                    anchors.fill: parent
                    anchors.leftMargin: 24
                    anchors.rightMargin: 24
                    spacing: 12

                    ListView {
                        id: galleryListView
                        orientation: ListView.Horizontal
                        height: Screen.height *  0.8//(isMobile ? 0.8 : 0.6)
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.top: parent.top
                        anchors.topMargin:  Screen.height * 0.175//(isMobile ? 0.175 : 0.125)
                        spacing: 24
                        clip: false  // Cho phép phóng to vượt ra ngoài
                        model: selectedDrone && selectedDrone.gallery ? selectedDrone.gallery : []
                        snapMode: ListView.SnapToItem
                        preferredHighlightBegin: (width - (Screen.width * (isMobile ? 0.5 : 0.5))) / 2
                        preferredHighlightEnd: (width - (Screen.width * (isMobile ? 0.5 : 0.5))) / 2
                        highlightRangeMode: ListView.StrictlyEnforceRange
                        interactive: true

                        delegate: Item {
                            width: (Screen.width * (isMobile ? 0.5 : 0.5))
                            height: isMobile ? 240 : 240
                            property real centerPos: galleryListView.contentX + galleryListView.width / 2
                            property real itemCenter: x + width / 2
                            property real dist: Math.abs(itemCenter - centerPos)
                            property real scaleFactor: Math.max(0.8, 1.2 - dist / (isMobile ? 0.5 : 0.5))

                            opacity: dist < 20 ? 1.0 : 0.5

                            Behavior on opacity {
                                NumberAnimation { duration: 150 }
                            }

                            transform: Scale {
                                origin.x: width / 2
                                origin.y: height / 2
                                xScale: scaleFactor
                                yScale: scaleFactor
                            }

                            Rectangle {
                                anchors.fill: parent
                                radius: 12
                                border.color: qgcPal.globalTheme === QGCPalette.Light ? "#ccc" : "#555"
                                color: qgcPal.globalTheme === QGCPalette.Light ? "#ddd": "#333"

                                Image {
                                    anchors.fill: parent
                                    anchors.margins: 8
                                    source: modelData
                                    fillMode: Image.PreserveAspectFit
                                }
                            }
                        }
                    }

                }

            }
        }
    }

}

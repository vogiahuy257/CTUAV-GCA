/****************************************************************************
 *
 * (c) 2009-2020 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts

import QGroundControl
import QGroundControl.Controls
import QGroundControl.Palette
import QGroundControl.ScreenTools
import QGroundControl.Controllers
import QGroundControl.FactSystem
import QGroundControl.FactControls

Item {
    id:         _root

    property Fact   _editorDialogFact: Fact { }
    property int    _rowHeight:         ScreenTools.defaultFontPixelHeight * 2
    property int    _rowWidth:          10 // Dynamic adjusted at runtime
    property bool   _searchFilter:      searchText.text.trim() != "" || controller.showModifiedOnly  ///< true: showing results of search
    property var    _searchResults      ///< List of parameter names from search results
    property var    _activeVehicle:     QGroundControl.multiVehicleManager.activeVehicle
    property bool   _showRCToParam:     _activeVehicle.px4Firmware
    property var    _appSettings:       QGroundControl.settingsManager.appSettings
    property var    _controller:        controller

    ParameterEditorController {
        id: controller
    }

    Timer {
        id:         clearTimer
        interval:   100;
        running:    false;
        repeat:     false
        onTriggered: {
            searchText.text = ""
            controller.searchText = ""
        }
    }

    Rectangle {
        id: toolsPanel
        anchors.fill: tableView
        color: qgcPal.globalTheme === QGCPalette.Light ? "#7bffffff" : "#7b222222"
        visible: false
        z: 100
        opacity: visible ? 1 : 0
        Behavior on opacity { NumberAnimation { duration: 130 } }

        // Biến dùng chung
        property int panelWidth: 180
        property int buttonWidth: panelWidth - margin * 2
        property int margin: 10

        MouseArea {
            anchors.fill: parent
            onClicked: toolsPanel.visible = false
        }

        Rectangle {
            id: panelContent
            width: toolsPanel.panelWidth
            height: buttonColumn.implicitHeight
            color:qgcPal.globalTheme === QGCPalette.Light ? "#fff": "#222222"
            border.color:qgcPal.globalTheme === QGCPalette.Light ?"#b9b9b9": "#555555"
            border.width: 1
            radius: 8
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.topMargin: 4

            Column {
                id: buttonColumn
                spacing: toolsPanel.margin / 2
                padding: toolsPanel.margin

                QGCButton {
                    width: toolsPanel.buttonWidth
                    text: qsTr("Refresh")
                    //iconSource: "/qmlimages/refresh.svg"
                    onClicked: {
                        controller.refresh()
                        toolsPanel.visible = false
                    }
                }

                QGCButton {
                    width: toolsPanel.buttonWidth
                    text: qsTr("Reset to Defaults")
                    //iconSource: "/qmlimages/reset.svg"
                    onClicked: {
                        mainWindow.showMessageDialog(
                            qsTr("Reset All"),
                            qsTr("Select Reset to reset all parameters to their defaults.\n\nNote that this will also completely reset everything, including UAVCAN nodes, all vehicle settings, setup and calibrations."),
                            Dialog.Cancel | Dialog.Reset,
                            function() { controller.resetAllToDefaults() }
                        )
                        toolsPanel.visible = false
                    }
                }

                QGCButton {
                    width: toolsPanel.buttonWidth
                    text: qsTr("Load from file for review...")
                    //iconSource: "/qmlimages/load.svg"
                    onClicked: {
                        fileDialog.title = qsTr("Load Parameters")
                        fileDialog.openForLoad()
                        toolsPanel.visible = false
                    }
                }

                QGCButton {
                    width: toolsPanel.buttonWidth
                    text: qsTr("Save to file...")
                    //iconSource: "/qmlimages/save.svg"
                    onClicked: {
                        fileDialog.title = qsTr("Save Parameters")
                        fileDialog.openForSave()
                        toolsPanel.visible = false
                    }
                }

                QGCButton {
                    width: toolsPanel.buttonWidth
                    text: qsTr("Clear all RC to Param")
                    //iconSource: "/qmlimages/clear.svg"
                    visible: _showRCToParam
                    onClicked: {
                        _activeVehicle.clearAllParamMapRC()
                        toolsPanel.visible = false
                    }
                }

                QGCButton {
                    width: toolsPanel.buttonWidth
                    text: qsTr("Reboot Vehicle")
                    //iconSource: "/qmlimages/reboot.svg"
                    onClicked: {
                        mainWindow.showMessageDialog(
                            qsTr("Reboot Vehicle"),
                            qsTr("Select Ok to reboot vehicle."),
                            Dialog.Cancel | Dialog.Ok,
                            function() { _activeVehicle.rebootVehicle() }
                        )
                        toolsPanel.visible = false
                    }
                }
            }
        }
    }


    QGCFileDialog {
        id:             fileDialog
        folder:         _appSettings.parameterSavePath
        nameFilters:    [ qsTr("Parameter Files (*.%1)").arg(_appSettings.parameterFileExtension) , qsTr("All Files (*)") ]

        onAcceptedForSave: (file) => {
            controller.saveToFile(file)
            close()
        }

        onAcceptedForLoad: (file) => {
            close()
            if (controller.buildDiffFromFile(file)) {
                parameterDiffDialog.createObject(mainWindow).open()
            }
        }
    }

    Component {
        id: editorDialogComponent

        ParameterEditorDialog {
            fact:           _editorDialogFact
            showRCToParam:  _showRCToParam
        }
    }

    Component {
        id: parameterDiffDialog

        ParameterDiffDialog {
            paramController: _controller
        }
    }

    // menu trong parameter editor
    RowLayout {
        id:             header
        anchors.left:   parent.left
        anchors.right:  parent.right

        RowLayout {
            Layout.alignment:   Qt.AlignLeft
            spacing:            ScreenTools.defaultFontPixelWidth

            QGCTextField {
                id:                     searchText
                placeholderText:        qsTr("Search")
                onDisplayTextChanged:   controller.searchText = displayText
            }

            QGCButton {
                text: qsTr("Clear")
                onClicked: {
                    if(ScreenTools.isMobile) {
                        Qt.inputMethod.hide();
                    }
                    clearTimer.start()
                }
            }

            QGCCheckBox {
                text:       qsTr("Show modified only")
                checked:    controller.showModifiedOnly
                onClicked:  controller.showModifiedOnly = checked
                visible:    QGroundControl.multiVehicleManager.activeVehicle.px4Firmware
            }
        }

        QGCButton {
            Layout.alignment:   Qt.AlignRight
            text:               qsTr("Tools")
            onClicked:          toolsPanel.visible = !toolsPanel.visible //toolsMenu.popup()
        }
    }

    /// Group buttons
    QGCFlickable {
        id :                groupScroll
        width:              ScreenTools.defaultFontPixelWidth * 25
        anchors.top:        header.bottom
        anchors.bottom:     parent.bottom
        clip:               true
        pixelAligned:       true
        contentHeight:      groupedViewCategoryColumn.height
        flickableDirection: Flickable.VerticalFlick
        visible:            !_searchFilter

        ColumnLayout {
            id:             groupedViewCategoryColumn
            anchors.left:   parent.left
            anchors.right:  parent.right
            spacing:        Math.ceil(ScreenTools.defaultFontPixelHeight * 0.25)

            Repeater {
                model: controller.categories

                Column {
                    Layout.fillWidth:   true
                    spacing:            Math.ceil(ScreenTools.defaultFontPixelHeight * 0.25)


                    SectionHeader {
                        id:             categoryHeader
                        anchors.left:   parent.left
                        anchors.right:  parent.right
                        text:           object.name
                        checked:        object == controller.currentCategory

                        onCheckedChanged: {
                            if (checked) {
                                controller.currentCategory  = object
                            }
                        }
                    }

                    Repeater {
                        model: categoryHeader.checked ? object.groups : 0

                        QGCButton {
                            width:          ScreenTools.defaultFontPixelWidth * 25
                            text:           object.name
                            height:         _rowHeight
                            checked:        object == controller.currentGroup
                            autoExclusive:  true

                            onClicked: {
                                if (!checked) _rowWidth = 10
                                checked = true
                                controller.currentGroup = object
                            }
                        }
                    }
                }
            }
        }
    }

    TableView {
        id:                 tableView
        anchors.leftMargin: ScreenTools.defaultFontPixelWidth
        anchors.top:        header.bottom
        anchors.bottom:     parent.bottom
        anchors.left:       _searchFilter ? parent.left : groupScroll.right
        anchors.right:      parent.right
        columnSpacing:      ScreenTools.defaultFontPixelWidth
        rowSpacing:         ScreenTools.defaultFontPixelHeight / 4
        model:              controller.parameters
        contentWidth:       width
        clip:               true

        // Qt is supposed to adjust column widths automatically when larger widths come into view.
        // But it doesn't work. So we have to do it force a layout manually when we scroll.
        Timer {
            id:             forceLayoutTimer
            interval:       500
            repeat:         false
            onTriggered:    tableView.forceLayout()
        }

        onTopRowChanged: forceLayoutTimer.start()
        onModelChanged: {
            positionViewAtRow(0, TableView.AlignLeft | TableView.AlignTop)
            forceLayoutTimer.start()
        }

        delegate: Item {
            implicitWidth:  label.contentWidth
            implicitHeight: label.contentHeight
            clip:           true

            QGCLabel {
                id:                 label
                width:              column == 1 ? ScreenTools.defaultFontPixelWidth * 15 : contentWidth
                text:               column == 1 ? col1String() : display
                color:              column == 1 ? col1Color() : qgcPal.text
                maximumLineCount:   1
                elide:              column == 1 ? Text.ElideRight : Text.ElideNone

                Component.onCompleted: {
                    return
                    if (tableView.columnWidth(column) < width) {
                        console.log("setColumnWidth", column, width)
                        tableView.setColumnWidth(column, width)
                    }
                }

                function col1String() {
                    if (fact.enumStrings.length === 0) {
                        return fact.valueString + " " + fact.units
                    }
                    if (fact.bitmaskStrings.length != 0) {
                        return fact.selectedBitmaskStrings.join(',')
                    }
                    return fact.enumStringValue
                }

                function col1Color() {
                    if (fact.defaultValueAvailable) {
                        return fact.valueEqualsDefault ? qgcPal.text : qgcPal.warningText
                    } else {
                        return qgcPal.text
                    }
                }
            }

            QGCMouseArea {
                anchors.fill: parent
                onClicked: mouse => {
                    _editorDialogFact = fact
                    editorDialogComponent.createObject(mainWindow).open()
                }
            }
        }
    }
}

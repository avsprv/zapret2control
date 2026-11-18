pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

Kirigami.ApplicationWindow {
    id: root

    required property bool startMinimized
    required property var appController
    required property var settingsManager
    required property var autostartController
    required property var trayController
    required property var configController
    required property var profileController
    required property var logsController
    required property var listsController
    required property var blockcheckController
    required property var updateController

    title: qsTr("zapret2control")
    visible: !startMinimized
    width: Kirigami.Units.gridUnit * 54
    height: Kirigami.Units.gridUnit * 38
    minimumWidth: Kirigami.Units.gridUnit * 36
    minimumHeight: Kirigami.Units.gridUnit * 27

    pageStack.initialPage: overviewPage

    globalDrawer: Kirigami.GlobalDrawer {
        isMenu: false
        modal: root.width < Kirigami.Units.gridUnit * 50

        actions: [
            Kirigami.Action {
                text: qsTr("Обзор")
                icon.name: "view-dashboard"
                onTriggered: root.pageStack.replace(overviewPage)
            },
            Kirigami.Action {
                text: qsTr("Конфигурация")
                icon.name: "configure"
                onTriggered: root.pageStack.replace(configurationPage)
            },
            Kirigami.Action {
                text: qsTr("Списки")
                icon.name: "view-list-text"
                onTriggered: root.pageStack.replace(listsPage)
            },
            Kirigami.Action {
                text: qsTr("Диагностика")
                icon.name: "tools-report-bug"
                onTriggered: root.pageStack.replace(diagnosticsPage)
            },
            Kirigami.Action {
                text: qsTr("Журнал")
                icon.name: "view-list-details"
                onTriggered: root.pageStack.replace(journalPage)
            },
            Kirigami.Action {
                text: qsTr("Управление zapret2")
                icon.name: "system-software-install"
                onTriggered: root.pageStack.replace(upstreamPage)
            },
            Kirigami.Action {
                text: qsTr("Настройки приложения")
                icon.name: "settings-configure"
                onTriggered: root.pageStack.replace(applicationSettingsPage)
            },
            Kirigami.Action {
                text: qsTr("О программе")
                icon.name: "help-about"
                onTriggered: root.pageStack.replace(aboutPage)
            }
        ]
    }

    Component {
        id: overviewPage

        Kirigami.ScrollablePage {
            title: qsTr("Обзор")

            ColumnLayout {
                width: parent.width
                spacing: Kirigami.Units.largeSpacing

                Kirigami.InlineMessage {
                    Layout.fillWidth: true
                    type: Kirigami.MessageType.Information
                    text: qsTr("Новая оболочка приложения. Раздел будет подключён после визуальной проверки навигации.")
                }
            }
        }
    }

    Component {
        id: configurationPage

        Kirigami.ScrollablePage {
            title: qsTr("Конфигурация")

            ColumnLayout {
                width: parent.width

                Kirigami.InlineMessage {
                    Layout.fillWidth: true
                    type: Kirigami.MessageType.Information
                    text: qsTr("Здесь будут представлены штатные параметры zapret2, профили, фильтры, расширенные настройки и исходный config.")
                }
            }
        }
    }

    Component {
        id: listsPage

        Kirigami.ScrollablePage {
            title: qsTr("Списки")

            ColumnLayout {
                width: parent.width

                Kirigami.InlineMessage {
                    Layout.fillWidth: true
                    type: Kirigami.MessageType.Information
                    text: qsTr("Раздел предназначен для пользовательских и генерируемых списков, источников, обновления и системного планировщика.")
                }
            }
        }
    }

    Component {
        id: diagnosticsPage

        Kirigami.ScrollablePage {
            title: qsTr("Диагностика")

            ColumnLayout {
                width: parent.width

                Kirigami.InlineMessage {
                    Layout.fillWidth: true
                    type: Kirigami.MessageType.Information
                    text: qsTr("Диагностика будет запускать штатный blockcheck2.sh с соответствующими upstream-компонентами и настройками.")
                }
            }
        }
    }

    Component {
        id: journalPage

        Kirigami.ScrollablePage {
            title: qsTr("Журнал")

            ColumnLayout {
                width: parent.width

                Kirigami.InlineMessage {
                    Layout.fillWidth: true
                    type: Kirigami.MessageType.Information
                    text: qsTr("Здесь будут раздельно отображаться операции приложения и журналы системных компонентов zapret2.")
                }
            }
        }
    }

    Component {
        id: upstreamPage

        Kirigami.ScrollablePage {
            title: qsTr("Управление zapret2")

            ColumnLayout {
                width: parent.width

                Kirigami.InlineMessage {
                    Layout.fillWidth: true
                    type: Kirigami.MessageType.Information
                    text: qsTr("Установка, переустановка, обновление и удаление будут выполняться через штатные сценарии upstream-проекта.")
                }
            }
        }
    }

    Component {
        id: applicationSettingsPage

        Kirigami.ScrollablePage {
            title: qsTr("Настройки приложения")

            ColumnLayout {
                width: parent.width

                Kirigami.InlineMessage {
                    Layout.fillWidth: true
                    type: Kirigami.MessageType.Information
                    text: qsTr("Здесь будут находиться только настройки самого приложения: системный трей, запуск, уведомления и внешний вид.")
                }
            }
        }
    }

    Component {
        id: aboutPage

        Kirigami.ScrollablePage {
            title: qsTr("О программе")

            ColumnLayout {
                width: parent.width
                spacing: Kirigami.Units.largeSpacing

                Controls.Label {
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    text: qsTr("zapret2control — графический интерфейс для управления штатными возможностями upstream zapret2 в KDE Plasma.")
                }

                Controls.Label {
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    text: qsTr("Версия приложения: %1").arg(Qt.application.version)
                }
            }
        }
    }
}

import QtQuick
import QtQuick.Controls as Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

Kirigami.ScrollablePage {
    id: page

    required property var settingsManager
    required property var autostartController
    required property var trayController

    title: qsTr("Настройки")

    ColumnLayout {
        width: page.availableWidth
        spacing: Kirigami.Units.largeSpacing

        Controls.GroupBox {
            Layout.fillWidth: true
            title: qsTr("Внешний вид")

            Kirigami.FormLayout {
                anchors.fill: parent

                Controls.ComboBox {
                    Kirigami.FormData.label: qsTr("Тема оформления")
                    Layout.fillWidth: true
                    model: [
                        { text: qsTr("Системная"), value: "system" },
                        { text: qsTr("Светлая"), value: "light" },
                        { text: qsTr("Тёмная"), value: "dark" }
                    ]
                    textRole: "text"
                    Accessible.name: qsTr("Тема оформления")
                    currentIndex: {
                        const index = model.findIndex(item => item.value === page.settingsManager.theme)
                        return Math.max(0, index)
                    }
                    onActivated: page.settingsManager.theme = model[currentIndex].value
                }

                Controls.ComboBox {
                    Kirigami.FormData.label: qsTr("Язык интерфейса")
                    Layout.fillWidth: true
                    model: [
                        { text: qsTr("Системный"), value: "system" },
                        { text: qsTr("Русский"), value: "ru" },
                        { text: qsTr("English"), value: "en" }
                    ]
                    textRole: "text"
                    Accessible.name: qsTr("Язык интерфейса")
                    currentIndex: {
                        const index = model.findIndex(item => item.value === page.settingsManager.language)
                        return Math.max(0, index)
                    }
                    onActivated: page.settingsManager.language = model[currentIndex].value
                }

                Controls.Label {
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    opacity: 0.75
                    text: qsTr("Изменения применяются сразу. Системная тема следует настройкам рабочего стола.")
                }
            }
        }

        Controls.GroupBox {
            Layout.fillWidth: true
            title: qsTr("Запуск")

            ColumnLayout {
                anchors.fill: parent
                Controls.CheckBox {
                    Layout.fillWidth: true
                    text: qsTr("Запускать приложение при входе в систему")
                    checked: page.autostartController.enabled
                    onToggled: page.autostartController.setEnabled(checked)
                }

                Kirigami.InlineMessage {
                    Layout.fillWidth: true
                    visible: page.autostartController.errorMessage.length > 0
                    type: Kirigami.MessageType.Error
                    text: page.autostartController.errorMessage
                }
            }
        }

        Controls.GroupBox {
            Layout.fillWidth: true
            title: qsTr("Поведение окна")

            ColumnLayout {
                anchors.fill: parent

                Controls.CheckBox {
                    Layout.fillWidth: true
                    text: qsTr("Сворачивать окно в системный трей при закрытии")
                    checked: page.settingsManager.closeToTray
                    enabled: page.trayController.registered
                    onToggled: page.settingsManager.closeToTray = checked
                }

                Kirigami.InlineMessage {
                    Layout.fillWidth: true
                    visible: !page.trayController.registered
                    type: Kirigami.MessageType.Information
                    text: qsTr("Системный трей не зарегистрирован. Сворачивание окна станет доступно после успешного подключения к StatusNotifierWatcher.")
                }
            }
        }

        Controls.GroupBox {
            Layout.fillWidth: true
            title: qsTr("Обновления")

            Controls.CheckBox {
                anchors.left: parent.left
                anchors.right: parent.right
                text: qsTr("Автоматически проверять обновления")
                checked: page.settingsManager.autoCheckUpdates
                onToggled: page.settingsManager.autoCheckUpdates = checked
            }
        }
    }
}

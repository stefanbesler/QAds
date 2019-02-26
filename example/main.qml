import QtQuick 2.10
import QtQuick.Window 2.10
import QtQuick.Controls 1.4
import QtQuick.Layouts 1.3

Window {
    visible: true
    width: 640
    height: 480
    color: "#292927"
    title: "QAds";

    Text {
        id: text1
        anchors.centerIn: parent
        color: "white"
        text: plcDriver.value("MAIN.variable").value
    }

    // Start action on the plc, the plc should reset the button to false
    Button {
      objectName: "button1"
      id: button1
      anchors.bottom: parent.bottom
      anchors.left: parent.left
      text: qsTr("Start")
      onClicked: btnStartMotor.setPressed(true)
    }

    // Start action on the plc, the plc should reset the button to false
    Button {
      id: button2
      anchors.bottom: parent.bottom
      anchors.left: button1.right
      text: qsTr("Stop")
      onClicked: btnStopMotor.setPressed(true)
    }

    // Spinbox, which can write a value to the plc via binding
    SpinBox {
      id: spin1
      anchors.bottom: parent.bottom
      anchors.right: parent.right
      value: nominalMotorSpeed.value
      suffix: " rpm"
      maximumValue: 5000
    }
    Binding { target: nominalMotorSpeed; property: "value"; value: spin1.value }

    // Spinbox, which can only read from the plc
    SpinBox {
      id: spin2
      anchors.bottom: parent.bottom
      anchors.right: spin1.left
      value: actualMotorSpeed.value
      suffix: " rpm"
      maximumValue: 5000
      stepSize: 100
      enabled: false // lets disable it to make it clear to the user that it is readonly
    }

    ColumnLayout
    {
        Text { color: "white"; text: randval1.value }
        Text { color: "white"; text: randval2.value }
        Text { color: "white"; text: randval3.value }
        Text { color: "white"; text: randval4.value }
        Text { color: "white"; text: randval5.value }
        Text { color: "white"; text: randval6.value }
        Text { color: "white"; text: randval7.value }
        Text { color: "white"; text: randval8.value }
        Text { color: "white"; text: randval9.value }
    }
}

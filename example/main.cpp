#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QDebug>

// qads "library" headers
#include <tc3manager.h>
#include <tc3value.h>

// this header is required for all examples, one could do without this,
// but for large scale projects an object like this is useful
#include "plcdriver.h"

// this header is used for example 2, where complete structs are
// read and write, respectively. In large scale projects, a header
// like this should be generated (i.e. by parsing Twincat3's TMC file
// and generating C++ code from it)
#include "plcdef.h"

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);

    PlcDriver data("10.100.20.228.1.1:851", &app);
    Tc3Manager* manager = data.manager();


    // EXAMPLE 1: single variable access
    //  change something on the PLC with the set method
    qDebug() << manager->value("MAIN.variable")->get();
    manager->value("MAIN.variable")->set("connected to plc");
    qDebug() << manager->value("MAIN.variable")->get();

    // EXAMPLE 2: struct variable access
    //  read "complex" struct from plc
    //  change something in the struct and write it to the plc
    //  read the struct again and let's check, if we actually change something
    Tc3Value *exampleStruct = manager->value("MAIN.struct1", sizeof(Plc::ExampleStruct));
    Plc::ExampleStruct exampleStructData = exampleStruct->get<Plc::ExampleStruct>();

    qDebug() << "Initial value" << exampleStructData.sub1.sub2.integer1;

    exampleStructData.sub1.sub2.integer1 += 1;
    exampleStruct->set<Plc::ExampleStruct>(exampleStructData);

    exampleStructData = exampleStruct->get<Plc::ExampleStruct>();
    qDebug() << "New value" << exampleStructData.sub1.sub2.integer1;


    // EXAMPLE 3: QML example
    //  Primitive datatypes (INT, DINT, REAL, ...) - where one can use Tc3Manager::AutoType
    //  can be bound to properties within QML
    qmlRegisterType<Tc3Value>("Tc3Value", 1, 0, "Tc3Value");
    qmlRegisterType<PlcButton>("PlcButton", 1, 0, "PlcButton");
    qmlRegisterType<PlcAnalogIo>("PlcAnalogIo", 1, 0, "PlcAnalogIo");

    QQmlApplicationEngine engine;
    QQmlContext* context = engine.rootContext();

    // Make generic read/write access to plc available within qml
    context->setContextProperty("plcDriver", &data);

    // Make qml aware of two buttons - each could start a sequence on the plc
    PlcButton btnStartMotor(manager->value("MAIN.btnStartMotor", Tc3Manager::AutoType), &data);
    PlcButton btnStopMotor(manager->value("MAIN.btnStopMotor", Tc3Manager::AutoType), &data);
    context->setContextProperty("btnStartMotor", &btnStartMotor);
    context->setContextProperty("btnStopMotor", &btnStopMotor);

    // Editable analog output
    PlcAnalogIo nominalMotorSpeed(manager->value("MAIN.nominalSpeed", Tc3Manager::AutoType), &data);
    context->setContextProperty("nominalMotorSpeed", &nominalMotorSpeed);

    // Display the current value of an analog input with qml (this is readonly, see qml code)
    PlcAnalogIo actualMotorSpeed(manager->value("MAIN.actualSpeed", Tc3Manager::AutoType, Tc3Manager::NotificationType::Cycle, 100, 1000), &data);
    context->setContextProperty("actualMotorSpeed", &actualMotorSpeed);

    // Stress QML with random values, that should update the corresponding ui element whenever a change occurs
    // parameters 0,1000 forces to receive every change - you wouldn't want to do this in real life applications,
    // also for actual applications, one would implement a QModel for something like that, but we keep it simple here
    QVector<PlcAnalogIo*> randval;
    for(int i=0; i<10; ++i)
    {
        randval << new PlcAnalogIo(manager->value(QString("MAIN.randval[%1]").arg(i), Tc3Manager::AutoType, Tc3Manager::NotificationType::Change, 50, 200), &data);
        context->setContextProperty(QString("randval%1").arg(i), randval[i]);
    }

    engine.load(QUrl(QStringLiteral("qrc:/main.qml")));
    if (engine.rootObjects().isEmpty())
        return -1;

    return app.exec();
}


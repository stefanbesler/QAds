# QAds
Implements an object-orientated layer on top of Beckhoff ADS (TwinCAT2, TwinCAT3). The objects, implemented within the scope of this project, integrate nicely with Qt and Qml by using the Qt's signal/slot concept.

# prerequisites
This repository is using https://github.com/Beckhoff/ADS as submodule. Hence, in order to make use of this package, you should be familiar with ADS (setting up routes, etc.)

# usage
We provide two objects to communicate with TwinCAT devices. 

* Tc3Manager connects to the TwinCAT device by constructing the object with the netid and port (e.g. 192.168.0.1.1.1:851). If the device is available, Tc3Manager will connect to the device and register a state-change callback. Whenever the state of the Twincat devices changes, Tc3Manager throws a qt signal, which can be connected to. Additionally, Tc3Manager takes care of reconnecting to the TwinCAT devices, if the connection to the device is lost at some point.

* Tc3Value represents a single instance on the TwinCAT device (e.g. MAIN.machineState). The object has methods to read and write values on the TwinCAT device. When constructing Tc3Value, we can optionally enable a valueChanged signal - Whenever the value of the variable on the TwinCAT devices changes, the valueChanged signal is send by the Tc3Value object. Tc3Value can read/write primitive datatypes (INT, DINT, REAL, LREAL, ...), but also reading/writing of DUTs (Structs, Enumerations, Unions) is implemented and demonstrated.

# example (C++)
```c++
int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);

    Tc3Manager manager("192.168.0.1.1.1:851");
    Tc3Value *machineState = manager.value("MAIN.machineState");

    // EXAMPLE 1: single variable access
    //  change something on the PLC with the set method
    qDebug() << manager.value("MAIN.variable")->get();
    manager.value("MAIN.variable")->set("connected to plc");
    qDebug() << manager.value("MAIN.variable")->get();

    // EXAMPLE 2: struct variable access
    //  read "complex" struct from plc
    //  change something in the struct and write it to the plc
    //  read the struct again and let's check, if we actually change something
    Tc3Value *exampleStruct = manager.value("MAIN.struct1", sizeof(Plc::ExampleStruct));
    Plc::ExampleStruct exampleStructData = exampleStruct->get<Plc::ExampleStruct>();
    
    qDebug() << "Initial value" << exampleStructData.sub1.sub2.integer1;
    
    exampleStructData.sub1.sub2.integer1 += 1;
    exampleStruct->set<Plc::ExampleStruct>(exampleStructData);
    
    exampleStructData = exampleStruct->get<Plc::ExampleStruct>();
    qDebug() << "New value" << exampleStructData.sub1.sub2.integer1;

    return app.exec();
}
```

# example (Qml)
```qml
Window {
    title: sps.value("MAIN.somestring").value;
}
```

In main.cpp, we show how these objects can be made available for the Qml engine (see main.qml), such that variables of the TwinCAT device can be easily accessable in a qml gui. The main "trick" here
is to implement a driver class that is made available as a context property to qml. 


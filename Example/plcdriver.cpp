#include "plcdriver.h"
#include <QVariant>
#include <QDebug>

PlcAnalogIo::PlcAnalogIo(QObject *parent) :
    QObject(parent)
{
}

PlcAnalogIo::PlcAnalogIo(Tc3Value *v, QObject *parent) :
    QObject(parent),
    plcValue_(v)
{
    connect(v, &Tc3Value::changed, this,
            [this](QVariant v)
            {
                qDebug() << "AnalogIo changed to" << v;
                emit this->changed(v.toDouble());
            });
}

QVariant PlcAnalogIo::get() const
{
    qDebug() << "AnalogIo::get";

    return plcValue_->get().toDouble();
}

void PlcAnalogIo::set(QVariant v)
{
    qDebug() << "AnalogIo::set to" << v;
    plcValue_->set(v);
}


PlcButton::PlcButton(QObject *parent) :
    QObject(parent)
{
}

PlcButton::PlcButton(Tc3Value *v, QObject *parent) :
    QObject(parent),
    plcValue_(v)
{
    connect(v, &Tc3Value::changed, this,
                [this](QVariant v)
                {
                    qDebug() << "Button changed";
                    emit this->changed(v.toBool());
                });
}

bool PlcButton::isPressed() const
{
    qDebug() << "Button is pressed";

    return plcValue_->get().convert(QVariant::Bool);
}

void PlcButton::setPressed(bool v)
{
    qDebug() << "Button::setPressed";
    plcValue_->set(v);
}



PlcDriver::PlcDriver(QString amsnetid, QObject *parent) : QObject(parent)
{
    manager_ = new Tc3Manager(amsnetid, parent);

    // basic error handling
    connect(manager_, &Tc3Manager::error, this, [](QString error){ qWarning() << error; });
}

PlcDriver::~PlcDriver()
{

}

Tc3Manager *PlcDriver::manager()
{
    return manager_;
}

Tc3Value *PlcDriver::value(QString id)
{
    return manager_->value(id, -1, Tc3Manager::NotificationType::Change);
}


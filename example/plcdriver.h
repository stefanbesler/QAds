#pragma once

#include "tc3manager.h"
#include "tc3value.h"
#include <QColor>
#include <QTimer>

class PlcButton : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool pressed READ isPressed WRITE setPressed NOTIFY changed)

public:
    PlcButton(QObject* parent=nullptr);
    PlcButton(Tc3Value* v, QObject* parent=nullptr);

    bool isPressed() const;

public slots:
    void setPressed(bool v);

signals:
    void changed(bool);

private:
    Tc3Value* plcValue_;
};


class PlcAnalogIo : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariant value READ get WRITE set NOTIFY changed)

public:
    PlcAnalogIo(QObject* parent=nullptr);
    PlcAnalogIo(Tc3Value* v, QObject* parent=nullptr);

    QVariant get() const;

public slots:
    void set(QVariant v);

signals:
    void changed(QVariant);

private:
    Tc3Value* plcValue_;
};


class PlcDriver : public QObject
{
    Q_OBJECT
public:
    PlcDriver(QString amsnetid, QObject *parent=nullptr);
    virtual ~PlcDriver();

    Tc3Manager* manager();

    // Generic method to read/write primitive data type
    Q_INVOKABLE Tc3Value* value(QString id);

private:
    Tc3Manager* manager_;
};


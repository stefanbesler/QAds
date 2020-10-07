#pragma once

// qt
#include <QObject>
#include "qads_global.h"
#include <QVariant>
#include <QHash>
#include <QVector>
#include <QList>
#include <QMutex>
#include <QString>

// use the correct ads defintions, platform depend
#ifdef __linux__
#include "AdsDef.h"
#define __stdcall
#elif _WIN32
#include <TcAdsDef.h>
#include <TcAdsAPI.h>
#else
#endif


#define SPSSTRINGLENGTH 256

class Tc3Value;
class QADSSHARED_EXPORT Tc3Manager : public QObject
{
    Q_OBJECT
    friend class Tc3Value;

public:
    #ifdef __linux__
    typedef uint32_t utype;
    typedef uint32_t htype;
    #elif _WIN32
    typedef long unsigned int utype;
    typedef unsigned long htype;
    #else
    #endif

    enum NotificationType
    {
        None,
        Cycle,
        Change
    };

    struct SymbolInfo
    {
        int group;
        int offset;
        int size;
        char symbolName[SPSSTRINGLENGTH];
        char symbolType[SPSSTRINGLENGTH];
        char symbolComment[SPSSTRINGLENGTH];

        SymbolInfo();
    };

    Tc3Manager(const QString& amsnetid=QString(), QObject *parent=nullptr);
    virtual ~Tc3Manager();
    Tc3Value * value(const QString& name, int datatypeSizeInByte=Tc3Manager::AutoType, NotificationType notificationType=NotificationType::None, int cycleTime_ms=300, int maxDelay_ms=1000);
    bool isConnected() const;

    static constexpr int AutoType = -1; // Automatic Type detection

signals:
    void connectionChanged(bool);
    void error(QString);

private slots:
    void onConnectionChanged(bool);

protected:

    // router specific methods
    unsigned short adsState(long port);
    bool connect();
    void disconnect();

    // value/variable specific methods
    htype connectHandle(const QString& name);
    void disconnectHandle(htype);
    SymbolInfo symbolInfo(const QString& name);
    htype enableNotify(htype connectHandle, int size, Tc3Manager::NotificationType type, int cycleTimeMillisecond, int maxDelayMilliseconds, PAdsNotificationFuncEx callbackPtr);
    void disableNotify(htype connectHandle);
    void removeValue(Tc3Value *value);
    Tc3Value * value(htype nhandle);

    bool syncReadReq(htype connectHandle, void *data, int size);
    bool syncWriteReq(htype connectHandle, const void *data, int size);

    // timer for auto reconnected if connected is interrupted
    virtual void timerEvent (QTimerEvent * event);

#ifdef __linux__
    static void __stdcall onConnectionChanged(const AmsAddr *adr, const AdsNotificationHeader *header, utype userdata);
#elif _WIN32
    static void __stdcall onConnectionChanged(AmsAddr *adr, AdsNotificationHeader *header, utype userdata);
#endif

    // Conversions
    static int tc3ArraySize( const QString& symbolType, int* arrayStart=nullptr);
    static QVariant::Type tc3VariantType(const QString& symbolType, int size);
    static QString tc3AdsError(long errorId);
protected:
    QMutex mutex_;
    QList<Tc3Value*> vars_;
    int reconnectTimer_;
    bool connected_;

    // Tc3 router connection
    AmsAddr	adsadr_;
    long adsport_;
    htype mhandle_;
    htype mhandleMem_;

    // We can not use "this" in an ADS callbacks on 64-Bit systems.
    // hence, we need a different way to reference to managers in callbacks.
    // ADS only allows integers as userdata, so we can't simply use the pointer
    // on a manager as userdata if we want this to work on 64 bit systems.
    // We give each manager a unique (32bit id) such that we can identify it in a callback.
    // Unfortunately, this make the constructor not threadsafe, should not be a big issue
    // though, because managers should not be created too much dynamically in most use cases
    utype id_;
    static utype nid_;
    static QHash<utype, Tc3Manager*> uniqueInst_;
};

#pragma once
#include "qads_global.h"
#include <QObject>
#include <QVariant>

// use the correct ads defintions, platform depend
#ifdef __linux__
#include "AdsDef.h"

// helper from original TcAdsDef
#define	PADSSYMBOLNAME(p)			((char*)(((AdsSymbolEntry*)p)+1))
#define	PADSSYMBOLTYPE(p)			(((char*)(((AdsSymbolEntry*)p)+1))+((AdsSymbolEntry*)p)->nameLength+1)
#define	PADSSYMBOLCOMMENT(p)		(((char*)(((AdsSymbolEntry*)p)+1))+((AdsSymbolEntry*)p)->nameLength+1+((AdsSymbolEntry*)p)->typeLength+1)

#define	PADSNEXTSYMBOLENTRY(pEntry)	(*((unsigned long*)(((char*)pEntry)+((AdsSymbolEntry*)pEntry)->entryLength)) \
    ? ((AdsSymbolEntry*)(((char*)pEntry)+((AdsSymbolEntry*)pEntry)->entryLength)): nullptr)

#elif _WIN32
#include <TcAdsAPI.h>
#else
#endif

#include "tc3manager.h"



class QADSSHARED_EXPORT Tc3Value : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariant value READ get WRITE set NOTIFY changed)

    friend class Tc3Manager;

    // disable auto deduction struct
    template <typename T>
    struct Identity
    {
        typedef T type;
    };

public:
    Tc3Value(QObject *parent=nullptr);
    Tc3Value(const QString& name, Tc3Manager* manager, int datatypeSizeInByte=Tc3Manager::AutoType, QObject *parent=nullptr);
    virtual ~Tc3Value();

    void enableNotify(Tc3Manager::NotificationType type, int cycleTimeMillisecond, int maxDelayMillisecond);
    bool isConnected() const;

    QVariant get() const;

    // disable automatic template deduction, to make this work with QVariant ::get
    template<class T>
    typename Identity<T>::type get() const
    {
        T ret;
        memset(&ret, 0, sizeof(T));

        if(!isConnected())
        {
            emit manager_->error(QString("%1: not connected").arg(name_));
            return ret;
        }


        if(sizeof(T) != vsymbolinfo_.size)
        {
            emit manager_->error(QString("%1: symbolsize not matching template argument (%2b!=%3b)").arg(name_, sizeof(T), vsymbolinfo_.size));
            return ret;
        }

        if(!manager_->syncReadReq(h_, &ret, vsymbolinfo_.size))
        {
            emit manager_->error(QString("%1: error while writing %s").arg(name_));
            return ret;
        }
        return ret;
    }

    // disable automatic template deduction, to make this work with ::set(QVariant)
    template<class T>
    void set(typename Identity<const T&>::type v)
    {
        if(!isConnected())
        {
            emit manager_->error(QString("%1: not connected").arg(name_));
            return;
        }

        if(sizeof(T) != vsymbolinfo_.size)
        {
            emit manager_->error(QString("%1: symbolsize not matching template argument (%2b!=%3b)").arg(name_, sizeof(T), vsymbolinfo_.size));
            return;
        }

        if(!manager_->syncWriteReq(h_, &v, vsymbolinfo_.size))
        {
            emit manager_->error(QString("%1: error while writing %s").arg(name_));
            return;
        }
    }

public slots:
    void set(const QVariant& v);

signals:
    void changed(const QVariant& v);

protected:
    void connect();
    void invalidate();

    QString name_;
    mutable QVariant cached_;

    int astart_;
    int asize_;

    Tc3Manager::htype h_;
    Tc3Manager::SymbolInfo vsymbolinfo_;

    QVariant::Type variantType_;
    int vdatasizeInByte_;

    Tc3Manager *manager_;

    Tc3Manager::htype nh_;
    Tc3Manager::NotificationType notificationType_;
    int cycleTimeMillisecond_;
    int maxDelayMillisecond_;

#ifdef __linux__
    static void __stdcall onNotification(const AmsAddr *addr, const AdsNotificationHeader *header, Tc3Manager::utype userdata);
#elif _WIN32
    static void __stdcall onNotification(AmsAddr *addr, AdsNotificationHeader *header, Tc3Manager::utype userdata);
#endif


};


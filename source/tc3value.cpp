#include <include/tc3manager.h>
#include <include/tc3value.h>
#include <QDebug>

Tc3Value::Tc3Value(QObject *parent) : QObject(parent)
{

}

Tc3Value::Tc3Value( const QString& name, Tc3Manager* manager, int datatypeSizeInByte/*=Tc3Manager::AutoType*/, QObject *parent/*=nullptr*/ ) : QObject(parent)
{
    name_= name;

    h_ = 0;
    variantType_ = QVariant::Type::Invalid;
    notificationType_ = Tc3Manager::NotificationType::None;
    vdatasizeInByte_ = datatypeSizeInByte;

    nh_ = 0;
    astart_ = 0;

    manager_ = manager;
    connect();	// try to connect
}

Tc3Value::~Tc3Value()
{
    if(isConnected())
    {
        manager_->disableNotify(nh_);
        manager_->disconnectHandle(h_);
    }

    // remove this value from the manager when it is deleted
    manager_->removeValue(this);
}

void Tc3Value::connect()
{
    if(isConnected())
        return;

    h_ = manager_->connectHandle(name_);
    if(!h_)
    {
        variantType_ = QVariant::Type::Invalid;
        return;
    }

    vsymbolinfo_ = manager_->symbolInfo(name_);
    asize_ = Tc3Manager::tc3ArraySize(vsymbolinfo_.symbolType, &astart_);
    variantType_ = vdatasizeInByte_ < 0 ? Tc3Manager::tc3VariantType(vsymbolinfo_.symbolType, vsymbolinfo_.size) : QVariant::Type::UserType;
    enableNotify(notificationType_, cycleTimeMillisecond_, maxDelayMillisecond_);

    // read current value from the plc
    // todo this can be removed if notification for user types is implemented
    if(variantType_ != QVariant::Type::UserType)
    {
        emit changed(get());
    }
}

bool Tc3Value::isConnected() const
{
    return variantType_ != QVariant::Type::Invalid;
}

void Tc3Value::enableNotify(Tc3Manager::NotificationType type, int cycleTimeMillisecond=500, int maxDelayMillisecond=1000)
{
    notificationType_ = type;
    cycleTimeMillisecond_ = cycleTimeMillisecond;
    maxDelayMillisecond_ = maxDelayMillisecond;

    if(!isConnected())
        return;

    if(type != Tc3Manager::NotificationType::None)
    {
        if(nh_ > 0)
        {
            manager_->disableNotify(nh_);
            nh_ = 0;
        }

        nh_ = manager_->enableNotify(h_, vsymbolinfo_.size, type, maxDelayMillisecond, cycleTimeMillisecond, onNotification);
    }
    else if(type == Tc3Manager::NotificationType::None && nh_ > 0)
    {
        manager_->disableNotify(nh_);
        nh_ = 0;
    }
}

QVariant Tc3Value::get() const
{
    if(!isConnected())
        return QVariant();

    if(variantType_ == QVariant::Type::UserType)
    {
        emit manager_->error("use Tc3Value::get<T> template for usertypes");
        return QVariant();
    }

    QVariant v(variantType_);

    if(asize_ > 1)
    {
        // \todo implement arrays
    }
    else if(variantType_ == QVariant::String)
    {
        char* text = new char[vsymbolinfo_.size];

        if(!manager_->syncReadReq(h_, text, vsymbolinfo_.size))
            return QVariant();

        if(asize_ == 1)
        {
            cached_ = text;
        }
        else if(asize_ == 2)
        {
            cached_ = QString::fromWCharArray(reinterpret_cast<wchar_t*>(text), vsymbolinfo_.size >> 1);
        }
        else
        {
            emit manager_->error(QString("%1: incompatible string").arg(name_));
        }

        delete [] text;
        return cached_;
    }
    else if(!manager_->syncReadReq(h_, v.data(), vsymbolinfo_.size))
    {
        return QVariant();
    }

    cached_ = v;
    return v;
}

void Tc3Value::set(const QVariant& value)
{
     if(!isConnected())
        return;

    QVariant v(value);

    if(variantType_ == QVariant::Type::UserType)
    {
        emit manager_->error("use Tc3Value::set<T> template for usertypes");
        return;
    }

    // \todo implement array handling
    if(asize_ > 1)
        return;

    if(!v.convert(static_cast<int>(variantType_)))
        return;

    if(variantType_ == QVariant::String)
    {
        QString text = v.toString();
        text.resize(vsymbolinfo_.size);

        if(asize_ == 1)
        {
            manager_->syncWriteReq(h_, text.toLatin1().data(), static_cast<int>(static_cast<unsigned long>(text.size()) * sizeof(char)));
        }
        else if(asize_ == 2)
        {
            manager_->syncWriteReq(h_, text.data_ptr(), vsymbolinfo_.size * 2);
        }
        else
        {
            emit manager_->error(QString("%1: incompatible string").arg(name_));
        }

        cached_ = v;
        return;
    }
    else if(variantType_ == QVariant::Bool && vsymbolinfo_.size == 1 )
    {
        bool b = v.toBool(); /* ? 1 : 0*/
        manager_->syncWriteReq(h_, &b , vsymbolinfo_.size);
        cached_ = v;
        return;
    }

    manager_->syncWriteReq(h_, v.data(), vsymbolinfo_.size);
    cached_ = v;
}

void Tc3Value::invalidate()
{
    variantType_ = QVariant::Type::Invalid;
    emit changed(QVariant());
}

/*static*/
#ifdef __linux__
void Tc3Value::onNotification(const AmsAddr *addr, const AdsNotificationHeader *header, Tc3Manager::utype userdata)
#elif _WIN32
void Tc3Value::onNotification(AmsAddr *addr, AdsNotificationHeader *header, Tc3Manager::utype userdata)
#endif
{  
    Q_UNUSED(addr);

    // find the manager for this value. it would be great if we could simply use the pointer to it
    // as userdata, however, this doesn't work with AdsAPI on 64 bit systems.
    Tc3Manager* manager = nullptr;
    if(Tc3Manager::uniqueInst_.contains(userdata))
        manager = Tc3Manager::uniqueInst_[userdata];

    if(!manager)
        return;

    Tc3Value* self = manager->value(header->hNotification);
    if(!self)
        return;

    if(!self->isConnected())
        return;

    char* data = new char[header->cbSampleSize];
    // copy data, which start right after the header, don't use header->data as this is not implemented on Linux
#ifdef __linux__
    memcpy(data, static_cast<void*>(&header + sizeof(AdsNotificationHeader)), sizeof(char) * header->cbSampleSize);
#elif _WIN32
    memcpy(data, static_cast<void*>(&header->data), sizeof(char) * header->cbSampleSize);
#endif

    QVariant v;
    if(self->asize_ > 1)
    {
        // \todo implement array handling
    }
    else if(self->variantType_ == QVariant::String)
    {
        if(self->asize_ == 1)
        {
            v = QString(data);
        }
        else if(self->asize_ == 2)
        {
            v = QString::fromWCharArray(reinterpret_cast<wchar_t*>(data), header->cbSampleSize >> 1); // wchar uses 2 chars per character
        }
        else
        {
            emit self->manager_->error(QString("%1: incompatible string").arg(self->name_));
        }
    }
    else
    {
        v = QVariant(static_cast<int>(self->variantType_), static_cast<void*>(data));
    }
    delete[] data;

    // only emit a signal if the value actually changed
    if(self->cached_ != v)
    {
        self->cached_ = v;
        emit self->changed(v);
#ifdef QT_DEBUG
        qDebug() << self->name_ << " changed to " << v;
#endif
    }
}

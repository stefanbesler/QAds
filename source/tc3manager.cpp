#include <include/tc3manager.h>
#include <include/tc3value.h>
#include <QRegExp>
#include <QMutexLocker>
#include <QAbstractSocket>

// use the correct ads defintions, platform depend
#ifdef __linux__
#include "AdsDef.h"
#include "AdsLib.h"
#elif _WIN32
#include <TcAdsDef.h>
#include <TcAdsAPI.h>
#else
#endif

/*static*/
Tc3Manager::utype Tc3Manager::nid_ = 0;

/*static*/
QHash<Tc3Manager::utype, Tc3Manager*> Tc3Manager::uniqueInst_;

Tc3Manager::Tc3Manager(const QString& amsnetid/*=QString()*/, QObject *parent/*=nullptr*/) :
    QObject(parent),
    mutex_(QMutex::Recursive)
{
    // UniqueInst is for handling multiple manager instances on a system
    id_ = nid_++;
    uniqueInst_.insert(id_, this);

    // Auto reconnection handling if connection state changes
    mhandle_ = 0;
    mhandleMem_ = 0;
    adsport_ = 0;
    reconnectTimer_ = -1;
    QObject::connect(this, SIGNAL(connectionChanged(bool)), this, SLOT(onConnectionChanged(bool)));

    // If AmsNetId seems valid, try to connect to the ads router
    static QRegExp rx("(\\d+.\\d+\\.\\d+\\.\\d+\\.\\d+\\.\\d+):(\\d+)");
    if(rx.exactMatch(amsnetid))
    {
        adsadr_.port = rx.cap(2).toUShort();
        QStringList subadr = rx.cap(1).split(".");
        for(int i=0; i<subadr.count(); i++)
            adsadr_.netId.b[i] = static_cast<unsigned char>(subadr[i].toInt());

        connect();
    }
    else
    {
        emit connectionChanged(false);
        emit error("AmsNetId format should be 192.168.0.1.1.1:851");
    }
}

Tc3Manager::~Tc3Manager()
{
    if(uniqueInst_.contains(id_))
        uniqueInst_.remove(id_);
    disconnect();
}

bool Tc3Manager::connect()
{	
    // if we are already connected, we don't do anything
    if(isConnected())
        return true;

    long adsport = AdsPortOpenEx();

    if(!adsport)
    {
        emit connectionChanged(false);
        emit error("Could not open AdsPort - check route");
        return false;
    }
    
    // check if plc is running
    if(adsState(adsport) != ADSSTATE_RUN)
    {
        AdsPortCloseEx(adsport);
        emit connectionChanged(false);
        emit error("AdsState is not ADSSTATE_RUN - check if PLC is still running");
        return false;
    }

    // deal with old adsport, if we got to this point the plc is running
    // so we actually do some stuff (if stopped ads router doesn't respond
    // to most commands
    if(adsport_)
    {
        // deal with old notification handle - probably ads takes care of this automatically,
        // but let's be sure
        if(mhandleMem_)
        {
            AdsSyncDelDeviceNotificationReqEx(adsport_, &adsadr_, mhandleMem_);
            mhandleMem_ = 0;
        }
        
        // disconnect old port, we want to continue with the fresh adsport
        // and don't want multiple connections
        long errorId = AdsPortCloseEx(adsport_);
        if (errorId)
        {
            AdsPortCloseEx(adsport);
            emit connectionChanged(false);
            emit error(tc3AdsError(errorId));
            return false;
        }
        else
        {
            adsport_ = 0;
        }
    }

    adsport_ = adsport;

    AdsNotificationAttrib   attrib;
    attrib.cbLength       = sizeof(short);
    attrib.nTransMode     = ADSTRANS_SERVERONCHA;
    attrib.nMaxDelay      = 0; // report each change directly
    attrib.dwChangeFilter = 0;

    // This automatically runs onConnectionChanged as well
    long errorId = AdsSyncAddDeviceNotificationReqEx(adsport_, &adsadr_, ADSIGRP_DEVICE_DATA, ADSIOFFS_DEVDATA_ADSSTATE,
                                                     &attrib, onConnectionChanged, static_cast<Tc3Manager::utype>(id_), &mhandle_);
    if (errorId)
    {
        mhandle_ = 0;
        emit connectionChanged(false);
        emit error(tc3AdsError(errorId));
        return false;
    }

    // connect to all already registered variables
    foreach(Tc3Value* v, vars_)
    {
        // try to reconnect to all values
        v->connect();
    }

    // kill reconnection timer (only relevant of ::connect got called by timerEvent)
    if(reconnectTimer_ >= 0)
    {
        killTimer(reconnectTimer_);
        reconnectTimer_ = -1;
    }

    emit connectionChanged(true);

    return true;
}

void Tc3Manager::disconnect()
{
    QMutexLocker locker(&mutex_);
    foreach(Tc3Value* v, vars_)
        delete v;
    vars_.clear();

    if(!isConnected())
        return;

    long errorId = AdsSyncDelDeviceNotificationReqEx(adsport_, &adsadr_, mhandle_);
    if(errorId)
    {
        emit error(tc3AdsError(errorId));
    }
    else
    {
        mhandle_ = 0;
    }

    errorId = AdsPortCloseEx(adsport_);
    if(errorId)
    {
        emit error(tc3AdsError(errorId));
    }

    emit connectionChanged(false);
    return;
}

Tc3Manager::htype Tc3Manager::connectHandle(const QString& name)
{
    if (!isConnected())
        return 0;

    int h=0;
    long errorId = AdsSyncReadWriteReqEx2(adsport_, &adsadr_, ADSIGRP_SYM_HNDBYNAME, 0, sizeof(unsigned long),
                                          &h, static_cast<unsigned int>(name.size()), name.toLatin1().data(), nullptr);
    if (errorId)
    {
        emit error(tc3AdsError(errorId));
        return 0;
    }

    return static_cast<unsigned int>(h);
} 

void Tc3Manager::disconnectHandle(utype h)
{
    long errorId = AdsSyncWriteReqEx(adsport_,  &adsadr_, ADSIGRP_SYM_RELEASEHND, 0, sizeof(h), &h);
    if (errorId)
    {
        emit error(tc3AdsError(errorId));
    }
}

Tc3Manager::SymbolInfo Tc3Manager::symbolInfo(const QString& name)
{
    if (!isConnected())
        return Tc3Manager::SymbolInfo();

    Tc3Manager::SymbolInfo r;

    bool buffer[0xFFFF];
    AdsSymbolEntry* pAdsSymbolEntry;

    long errorId = AdsSyncReadWriteReqEx2(adsport_, &adsadr_, ADSIGRP_SYM_INFOBYNAMEEX, 0, sizeof(buffer), buffer,
                                          name.size(), name.toLatin1().data(), nullptr);

    if (errorId)
    {
        emit error(tc3AdsError(errorId));
        return Tc3Manager::SymbolInfo();
    }

    pAdsSymbolEntry = reinterpret_cast<AdsSymbolEntry*>(buffer);

    r.group = pAdsSymbolEntry->iGroup;
    r.offset = pAdsSymbolEntry->iOffs;
    r.size = pAdsSymbolEntry->size;

    int size=0;
    size = std::min(SPSSTRINGLENGTH, (int)strlen(PADSSYMBOLNAME(pAdsSymbolEntry)));
    memcpy(r.symbolName, PADSSYMBOLNAME(pAdsSymbolEntry), size);

    size = std::min(SPSSTRINGLENGTH, (int)strlen(PADSSYMBOLTYPE(pAdsSymbolEntry)));
    memcpy(r.symbolType, PADSSYMBOLTYPE(pAdsSymbolEntry), size);

    size = std::min(SPSSTRINGLENGTH, (int)strlen(PADSSYMBOLCOMMENT(pAdsSymbolEntry)));
    memcpy(r.symbolComment, PADSSYMBOLCOMMENT(pAdsSymbolEntry), size);

    return r;
}

bool Tc3Manager::syncReadReq(htype h, void *data, int size)
{
    if(!h || !data || !isConnected() || size <= 0)
        return false;

    long errorId = AdsSyncReadReqEx2(adsport_, &adsadr_, ADSIGRP_SYM_VALBYHND, h, size, data, nullptr);
    if (errorId)
    {
        emit error(tc3AdsError(errorId));
    }
    return !errorId;
}

bool Tc3Manager::syncWriteReq(htype h, const void *data, int size )
{
    if(!h || !data || !isConnected())
        return false;

    // ugly const cast, but Twincat3 Api want's it that way
    long errorId = AdsSyncWriteReqEx(adsport_, &adsadr_, ADSIGRP_SYM_VALBYHND, h, size,  const_cast<void*>(data));
    if (errorId)
    {
        emit error(tc3AdsError(errorId));
    }
    return !errorId;
}

Tc3Manager::htype Tc3Manager::enableNotify(htype h, int size, Tc3Manager::NotificationType type, int cycleTimeMillisecond, int maxDelayMilliseconds, PAdsNotificationFuncEx callbackPtr)
{
    if (!h || !isConnected())
        return 0;

    AdsNotificationAttrib attrib;
    attrib.cbLength = size;
    attrib.nTransMode = type == Tc3Manager::NotificationType::Cycle ? ADSTRANS_SERVERCYCLE : ADSTRANS_SERVERONCHA;
    attrib.nMaxDelay = maxDelayMilliseconds * 10000;
    attrib.nCycleTime = cycleTimeMillisecond * 10000;

    htype nh;
    long errorId = AdsSyncAddDeviceNotificationReqEx(adsport_, &adsadr_, ADSIGRP_SYM_VALBYHND, h, &attrib, callbackPtr, id_, &nh );
    if (errorId)
    {
        emit error(tc3AdsError(errorId));
        return 0;
    }

    return nh;
}

void Tc3Manager::disableNotify(htype h)
{
    if(!h)
        return;

    long errorId = AdsSyncDelDeviceNotificationReqEx(adsport_, &adsadr_, h);
    if (errorId)
    {
        emit error(tc3AdsError(errorId));
    }
}


Tc3Value * Tc3Manager::value(const QString& name, int datatypeSizeInByte/*=Tc3Manager::AutoType*/, Tc3Manager::NotificationType notificationType, int cycleTime_ms/*=300*/, int maxDelay_ms/*=1000*/)
{
    QMutexLocker locker(&mutex_);

    // check if we are already managing the requested value
    foreach(Tc3Value* v, vars_)
    {
        if(v->name_ == name)
            return v;
    }

    // the value that has been requested is not yet connected to, so create a new value
    Tc3Value *v = new Tc3Value(name, this, datatypeSizeInByte);
    vars_.append(v);
    v->enableNotify(notificationType, cycleTime_ms, maxDelay_ms);

    return v;
}

Tc3Value *Tc3Manager::value(Tc3Manager::htype nhandle)
{
    QMutexLocker locker(&mutex_);

    // check if we are managing a value with the specified notification handle
    foreach(Tc3Value* v, vars_)
    {
        if(v->nh_ == nhandle)
            return v;
    }

    return nullptr;
}

void Tc3Manager::removeValue(Tc3Value *v)
{
    QMutexLocker locker(&mutex_);
    vars_.erase(std::remove_if(vars_.begin(), vars_.end(), [v](Tc3Value* vit){ return v == vit; }), vars_.end());
}

bool Tc3Manager::isConnected() const
{
    return mhandle_ > 0;
}

void Tc3Manager::onConnectionChanged(bool connected)
{
    QMutexLocker locker(&mutex_);

    // AdsRouter is disconnected, but Tc3Manager is not aware of it right now
    if(isConnected() && !connected)
    {
        mhandleMem_ = mhandle_;
        mhandle_ = 0;
        emit error("AdsRouter disconnected!");

        // Set all values to invalid
        foreach(Tc3Value* v, vars_)
        {
            v->invalidate();
        }
    }

    // start a timer to reconnect to twincat in case we lose the connection - reconnecting will set all values to "not connected state"
    if(!connected && reconnectTimer_ < 0)
        reconnectTimer_ = startTimer(5000);
}

void Tc3Manager::timerEvent(QTimerEvent * event)
{
    QMutexLocker locker(&mutex_);
    Q_UNUSED(event)

    // try to reconnect
    if(!mhandle_)
        connect();
}

unsigned short Tc3Manager::adsState(long port)
{
    unsigned short adsState=0;
    unsigned short deviceState;
    long errorId = AdsSyncReadStateReqEx(port, &adsadr_, &adsState, &deviceState);
    if(errorId)
    {
        emit error(tc3AdsError(errorId));
    }

    return adsState;
}

/*static*/
int Tc3Manager::tc3ArraySize(const QString& symbolType, int *arrayStart /*= nullptr*/ )
{
    static QRegExp rx("ARRAY.\\[(\\d+)..(\\d+)\\].OF.(\\d+)");

    if(symbolType.left(5) == "ARRAY")
    {
        rx.indexIn(symbolType);
        if(arrayStart)
            *arrayStart = rx.cap(1).toInt();

        return rx.cap(2).toInt() - rx.cap(1).toInt() + 1;
    }
    else if(symbolType.left(7) == "WSTRING")
    {
        if(arrayStart)
            *arrayStart = 0;

        // abuse arraysize to distinct between 16bit/character strings (WSTRING) and 8bit/character strings (STRING),
        // this make kinda sense
        return 2;

    }

    return 1;
}


/*static*/
QVariant::Type Tc3Manager::tc3VariantType(const QString& symbolType, int size)
{
    // convert native twincat types to qvariant types
    if(symbolType == "INT" || size == 2)
        return static_cast<QVariant::Type>(QMetaType::Short);
    else if(symbolType == "DINT")
        return QVariant::Int;
    else if(symbolType == "UDINT")
        return QVariant::UInt;
    else if(symbolType == "UINT")
        return  static_cast<QVariant::Type>(QMetaType::UShort);
    else if(symbolType == "BYTE")
        return QVariant::Char;
    else if(symbolType == "BOOL" || size == 1)
        return QVariant::Bool;
    else if(symbolType == "REAL" || size == 4)
        return static_cast<QVariant::Type>(QMetaType::Float);
    else if(symbolType == "LREAL" || size == 8)
        return QVariant::Double;
    else if(symbolType == "WORD")
        return static_cast<QVariant::Type>(QMetaType::UShort);
    else if(symbolType == "LWORD")
        return QVariant::ULongLong;
    else if(symbolType.left(7) == "WSTRING" || symbolType.left(6) == "STRING" || symbolType.right(QString("T_MaxString").length()) == "T_MaxString")
        return QVariant::String; // Distinct between wchar and char with tc3ArraySize

    // if we are dealing with an array, try to find it's type (only works if this is a native type)
    static QRegExp rx("ARRAY.\\[(\\d+)..(\\d+)\\].OF.(\\D+)");
    if(symbolType.left(5) == "ARRAY")	// e.g. ARRAY [1..12] OF REAL
    {
        rx.indexIn(symbolType);
        return tc3VariantType(rx.cap(3), -1);
    }

    return QVariant::Type::Invalid;
}

/*static*/
QString Tc3Manager::tc3AdsError(long errorId)
{
    switch(errorId)
    {
    case ADSERR_DEVICE_ERROR				: return QString("Error class < device error >");
    case ADSERR_DEVICE_SRVNOTSUPP			: return QString("Service is not supported by server");
    case ADSERR_DEVICE_INVALIDGRP 			: return QString("invalid indexGroup");
    case ADSERR_DEVICE_INVALIDOFFSET		: return QString("invalid indexOffset");
    case ADSERR_DEVICE_INVALIDACCESS		: return QString("reading/writing not permitted");
    case ADSERR_DEVICE_INVALIDSIZE			: return QString("parameter size not correct");
    case ADSERR_DEVICE_INVALIDDATA			: return QString("invalid parameter value(s)");
    case ADSERR_DEVICE_NOTREADY				: return QString("device is not in a ready state");
    case ADSERR_DEVICE_BUSY					: return QString("device is busy");
    case ADSERR_DEVICE_INVALIDCONTEXT		: return QString("invalid context (must be InWindows)");
    case ADSERR_DEVICE_NOMEMORY				: return QString("out of memory");
    case ADSERR_DEVICE_INVALIDPARM			: return QString("invalid parameter value(s)");
    case ADSERR_DEVICE_NOTFOUND				: return QString("not found (files, ...)");
    case ADSERR_DEVICE_SYNTAX				: return QString("syntax error in comand or file");
    //case ADSERR_DEVICE_INCOMPATIBLE			: return QString("objects do not match");
    case ADSERR_DEVICE_EXISTS				: return QString("object already exists");
    case ADSERR_DEVICE_SYMBOLNOTFOUND		: return QString("symbol not found");
    case ADSERR_DEVICE_SYMBOLVERSIONINVALID	: return QString("symbol version invalid");
    case ADSERR_DEVICE_INVALIDSTATE			: return QString("server is in invalid state");
    case ADSERR_DEVICE_TRANSMODENOTSUPP		: return QString("AdsTransMode not supported");
    case ADSERR_DEVICE_NOTIFYHNDINVALID		: return QString("Notification handle is invalid");
    case ADSERR_DEVICE_CLIENTUNKNOWN		: return QString("Notification client not registered");
    case ADSERR_DEVICE_NOMOREHDLS			: return QString("no more notification handles");
    case ADSERR_DEVICE_INVALIDWATCHSIZE		: return QString("size for watch to big");
    case ADSERR_DEVICE_NOTINIT				: return QString("device not initialized");
    case ADSERR_DEVICE_TIMEOUT				: return QString("device has a timeout");
    case ADSERR_DEVICE_NOINTERFACE			: return QString("query interface failed");
    case ADSERR_DEVICE_INVALIDINTERFACE		: return QString("wrong interface required");
    case ADSERR_DEVICE_INVALIDCLSID			: return QString("class ID is invalid");
    case ADSERR_DEVICE_INVALIDOBJID			: return QString("object ID is invalid");
    //case ADSERR_DEVICE_PENDING				: return QString("request is pending");
    case ADSERR_DEVICE_ABORTED				: return QString("request is aborted");
    case ADSERR_DEVICE_WARNING				: return QString("signal warning");
    case ADSERR_DEVICE_INVALIDARRAYIDX		: return QString("invalid array index");
    case ADSERR_DEVICE_SYMBOLNOTACTIVE		: return QString("symbol not active -> release handle and try again");
    case ADSERR_DEVICE_ACCESSDENIED			: return QString("access denied");
    case ADSERR_DEVICE_LICENSENOTFOUND		: return QString("no license found");
    case ADSERR_DEVICE_LICENSEEXPIRED		: return QString("license expired");
    case ADSERR_DEVICE_LICENSEEXCEEDED		: return QString("license exceeded");
    case ADSERR_DEVICE_LICENSEINVALID		: return QString("license invalid");
    case ADSERR_DEVICE_LICENSESYSTEMID		: return QString("license invalid system id");
    case ADSERR_DEVICE_LICENSENOTIMELIMIT	: return QString("license not time limited");
    case ADSERR_DEVICE_LICENSEFUTUREISSUE	: return QString("license issue time in the future");
    case ADSERR_DEVICE_LICENSETIMETOLONG	: return QString("license time period to long");
    case ADSERR_DEVICE_EXCEPTION			: return QString("exception in device specific code");
    case ADSERR_DEVICE_LICENSEDUPLICATED	: return QString("license file read twice");
    //case ADSERR_DEVICE_SIGNATUREINVALID		: return QString("invalid signature");
    case ADSERR_DEVICE_CERTIFICATEINVALID	: return QString("public key certificate");
    case ADSERR_CLIENT_ERROR				: return QString("Error class < client error >");
    case ADSERR_CLIENT_INVALIDPARM			: return QString("invalid parameter at service call");
    case ADSERR_CLIENT_LISTEMPTY			: return QString("polling list	is empty");
    case ADSERR_CLIENT_VARUSED				: return QString("var connection already in use");
    case ADSERR_CLIENT_DUPLINVOKEID			: return QString("invoke id in use");
    case ADSERR_CLIENT_SYNCTIMEOUT			: return QString("timeout elapsed");
    case ADSERR_CLIENT_W32ERROR				: return QString("error in win32 subsystem");
    case ADSERR_CLIENT_TIMEOUTINVALID		: return QString("unkown error");
    case ADSERR_CLIENT_PORTNOTOPEN			: return QString("ads dll");
    case ADSERR_CLIENT_NOAMSADDR			: return QString("ads dll");
    case ADSERR_CLIENT_SYNCINTERNAL			: return QString("internal error in ads sync");
    case ADSERR_CLIENT_ADDHASH				: return QString("hash table overflow");
    case ADSERR_CLIENT_REMOVEHASH			: return QString("key not found in hash table");
    case ADSERR_CLIENT_NOMORESYM			: return QString("no more symbols in cache");
    case ADSERR_CLIENT_SYNCRESINVALID		: return QString("invalid response received");
    case ADSERR_CLIENT_SYNCPORTLOCKED		: return QString("sync port is locked");
    }

    return QString();
}

/*static*/
#ifdef __linux__
void Tc3Manager::onConnectionChanged(const AmsAddr* addr, const AdsNotificationHeader* header, Tc3Manager::utype userdata)
#elif _WIN32
void Tc3Manager::onConnectionChanged(AmsAddr* addr, AdsNotificationHeader* header, Tc3Manager::utype userdata)
#endif
{
    Q_UNUSED(addr)
#ifdef __linux__
    const char data = *(reinterpret_cast<char*>(const_cast<AdsNotificationHeader*>(header)) + sizeof(header));
#elif _WIN32
    unsigned char data = header->data[0];
#else
#endif

    Tc3Manager *self = nullptr;
    if(uniqueInst_.contains(userdata))
        self = uniqueInst_[userdata];

    if(!self)
        return;

    // only emit signal if state changed
    if(data == ADSSTATE_RUN && !self->isConnected())
    {
        emit self->connectionChanged(QAbstractSocket::SocketState::ConnectedState);
    }
    else if(data != ADSSTATE_RUN && self->isConnected())
    {
        emit self->connectionChanged(QAbstractSocket::SocketState::UnconnectedState);
    }
}


Tc3Manager::SymbolInfo::SymbolInfo()
{
    memset(symbolName, 0, sizeof(char)*SPSSTRINGLENGTH);
    memset(symbolType, 0, sizeof(char)*SPSSTRINGLENGTH);
    memset(symbolComment, 0, sizeof(char)*SPSSTRINGLENGTH);
    group = 0;
    offset = 0;
    size = 0;
}

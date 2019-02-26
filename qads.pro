#-------------------------------------------------
#
# Project created by QtCreator 2019-02-24T18:58:39
#
#-------------------------------------------------

QT       -= gui
QT       += network

TARGET = qads
TEMPLATE = lib

DEFINES += QADS_LIBRARY

# The following define makes your compiler emit warnings if you use
# any feature of Qt which has been marked as deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS

# You can also make your code fail to compile if you use deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    ./source/tc3manager.cpp \
    ./source/tc3value.cpp

HEADERS += \
        ./include/qads_global.h \
        ./include/tc3manager.h \
        ./include/tc3value.h

unix {
    target.path = /usr/lib
    INSTALLS += target

    INCLUDEPATH += $$PWD/lib/ADS/AdsLib

    SOURCES += \
        lib/ADS/AdsLib/AdsDef.cpp \
        lib/ADS/AdsLib/AdsLib.cpp \
        lib/ADS/AdsLib/AmsConnection.cpp \
        lib/ADS/AdsLib/Frame.cpp \
        lib/ADS/AdsLib/AmsPort.cpp \
        lib/ADS/AdsLib/Log.cpp \
        lib/ADS/AdsLib/AmsRouter.cpp \
        lib/ADS/AdsLib/NotificationDispatcher.cpp \
        lib/ADS/AdsLib/Sockets.cpp

    HEADERS += \
        lib/ADS/AdsLib/AdsDef.h \
        lib/ADS/AdsLib/AdsLib.h \
        lib/ADS/AdsLib/AmsConnection.h \
        lib/ADS/AdsLib/Frame.h \
        lib/ADS/AdsLib/AmsHeader.h \
        lib/ADS/AdsLib/Log.h \
        lib/ADS/AdsLib/AmsPort.h \
        lib/ADS/AdsLib/NotificationDispatcher.h \
        lib/ADS/AdsLib/AmsRouter.h \
        lib/ADS/AdsLib/RingBuffer.h \
        lib/ADS/AdsLib/Router.h \
        lib/ADS/AdsLib/Semaphore.h \
        lib/ADS/AdsLib/Sockets.h \
        lib/ADS/AdsLib/wrap_endian.h \
        lib/ADS/AdsLib/wrap_socket.h \
        lib/ADS/AdsLib/AdsNotification.h

}

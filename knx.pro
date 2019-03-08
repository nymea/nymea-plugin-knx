exists(/home/timon/guh/development/qtknx/qtknx/build/mkspecs/modules/qt_lib_knx.pri) {
    include(/home/timon/guh/development/qtknx/qtknx/build/mkspecs/modules/qt_lib_knx.pri)
}

exists(/opt/qtknx/build/mkspecs/modules/qt_lib_knx.pri) {
    include(/opt/qtknx/build/mkspecs/modules/qt_lib_knx.pri)
}

include(/usr/include/nymea/plugin.pri)

QT += network knx

TARGET = $$qtLibraryTarget(nymea_devicepluginknx)

message(============================================)
message("Qt version: $$[QT_VERSION]")
message("Building $$deviceplugin$${TARGET}.so")

SOURCES += \
    devicepluginknx.cpp \
    knxtunnel.cpp

HEADERS += \
    devicepluginknx.h \
    knxtunnel.h


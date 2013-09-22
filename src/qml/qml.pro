include(../../common.pri)

TEMPLATE = lib
TARGET = socialcache
TARGET = $$qtLibraryTarget($$TARGET)

MODULENAME = org/nemomobile/socialcache
TARGETPATH = $$[QT_INSTALL_QML]/$$MODULENAME

LIBS += -L../lib -lsocialcache$${DASH_QT_VERSION}
INCLUDEPATH += ../lib/


QT += gui qml sql network dbus
CONFIG += plugin

CONFIG(nokeyprovider):{
DEFINES += NO_KEY_PROVIDER
} else {
CONFIG += link_pkgconfig
PKGCONFIG += libsailfishkeyprovider
}


HEADERS += \
    abstractsocialcachemodel.h \
    abstractsocialcachemodel_p.h \
    facebook/abstractfacebookcachemodel.h \
    facebook/abstractfacebookcachemodel_p.h \
    facebook/facebookimagecachemodel.h \
    facebook/facebookimagedownloader.h \
    facebook/facebookimagedownloader_p.h

SOURCES += plugin.cpp \
    abstractsocialcachemodel.cpp \
    facebook/abstractfacebookcachemodel.cpp \
    facebook/facebookimagecachemodel.cpp \
    facebook/facebookimagedownloader.cpp

OTHER_FILES += qmldir
import.files = qmldir

import.path = $$TARGETPATH
target.path = $$TARGETPATH

INSTALLS += target import

# translations
TS_FILE = $$OUT_PWD/socialcache.ts
EE_QM = $$OUT_PWD/socialcache_eng_en.qm

ts.commands += lupdate $$PWD/.. -ts $$TS_FILE
ts.CONFIG += no_check_exist no_link
ts.output = $$TS_FILE
ts.input = ..

ts_install.files = $$TS_FILE
ts_install.path = /usr/share/translations/source
ts_install.CONFIG += no_check_exist

# should add -markuntranslated "-" when proper translations are in place (or for testing)
engineering_english.commands += lrelease -idbased $$TS_FILE -qm $$EE_QM
engineering_english.CONFIG += no_check_exist no_link
engineering_english.depends = ts
engineering_english.input = $$TS_FILE
engineering_english.output = $$EE_QM

engineering_english_install.path = /usr/share/translations
engineering_english_install.files = $$EE_QM
engineering_english_install.CONFIG += no_check_exist

QMAKE_EXTRA_TARGETS += ts engineering_english
PRE_TARGETDEPS += ts engineering_english
INSTALLS += ts_install engineering_english_install

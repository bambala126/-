QT       += core gui widgets sql

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++17

INCLUDEPATH += $$PWD/include
DEPENDPATH += $$PWD/include

TARGET = TreeNote
TEMPLATE = app

DEFINES += QT_DEPRECATED_WARNINGS

SOURCES += \
    src/main.cpp \
    src/mainwindow.cpp \
    src/logindialog.cpp \
    src/databasemanager.cpp \
    src/treemodel.cpp \
    src/nodedialog.cpp \
    src/admindialog.cpp \
    src/accessrightsdialog.cpp \
    src/searchdialog.cpp

HEADERS += \
    include/mainwindow.h \
    include/logindialog.h \
    include/databasemanager.h \
    include/treemodel.h \
    include/nodedialog.h \
    include/admindialog.h \
    include/accessrightsdialog.h \
    include/searchdialog.h

FORMS += \
    ui/mainwindow.ui \
    ui/logindialog.ui \
    ui/nodedialog.ui \
    ui/admindialog.ui \
    ui/accessrightsdialog.ui \
    ui/searchdialog.ui

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

QT += core gui widgets network

CONFIG += c++17

SOURCES += \
    coursesearchengine.cpp \
    main.cpp \
    mainwindow.cpp \
    monitoringworker.cpp \
    spellchecker.cpp \
    stepikapiclient.cpp \
    textprocessor.cpp

HEADERS += \
    coursesearchengine.h \
    mainwindow.h \
    monitoringworker.h \
    spellchecker.h \
    stepikapiclient.h \
    textprocessor.h

FORMS += \
    mainwindow.ui

SOURCES +=
HEADERS += \
    Helpers.h

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

DISTFILES += \
    .gitignore
# Unit tests of the periodontal recall rules (no database, no user interface)
# Build and run: qmake RecallTests.pro && make && release/RecallTests.exe (or debug/)

QT += core testlib
QT -= gui
CONFIG += console c++20
CONFIG -= app_bundle

TARGET = RecallTests

INCLUDEPATH += ../../src

SOURCES += \
    tst_recall.cpp \
    ../../src/Model/Recall.cpp

HEADERS += \
    ../../src/Model/Recall.h

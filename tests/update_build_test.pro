QT += core network
QT -= gui
CONFIG += console c++11 release
CONFIG -= app_bundle debug_and_release
TEMPLATE = app
TARGET = update_build_test
INCLUDEPATH += ../src/utils ../src/core
include(../src/utils/shadow-update-config.pri)
SOURCES += update_build_test.cpp ../src/utils/Updater.cpp
HEADERS += ../src/utils/Updater.h
win32-msvc*: QMAKE_CXXFLAGS += /utf-8

QT = core
CONFIG += console c++11 release
CONFIG -= app_bundle debug_and_release
TEMPLATE = app
TARGET = github_release_test
INCLUDEPATH += $$PWD/../src/utils $$PWD/../src/core
SOURCES += $$PWD/github_release_test.cpp
win32-msvc*: QMAKE_CXXFLAGS += /utf-8

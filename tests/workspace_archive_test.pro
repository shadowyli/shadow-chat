QT += core gui widgets network xml multimedia printsupport
CONFIG += console c++11
CONFIG -= app_bundle
TEMPLATE = app
TARGET = workspace_archive_test

win32-msvc*: QMAKE_CXXFLAGS += /utf-8

INCLUDEPATH += ../src ../src/core ../src/desktop

SOURCES += workspace_archive_test.cpp \
           ../src/desktop/ShadowWorkspaceArchive.cpp

HEADERS += ../src/desktop/ShadowWorkspaceArchive.h

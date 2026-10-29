QT += core network
QT -= gui
CONFIG += console c++11 release no_batch
CONFIG -= app_bundle debug_and_release
TEMPLATE = app
TARGET = tls_probe
SOURCES += tls_probe.cpp
win32-msvc*: QMAKE_CXXFLAGS += /utf-8

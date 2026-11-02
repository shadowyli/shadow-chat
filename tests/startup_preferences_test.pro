include(dev_preview_test.pro)
QT += testlib
TARGET = startup_preferences_test
SOURCES = $$PWD/startup_preferences_test.cpp
win32: LIBS += advapi32.lib

include(dev_preview_test.pro)
TARGET = popup_placement_test
QT += testlib
SOURCES = $$PWD/popup_placement_test.cpp
win32-msvc*: QMAKE_CXXFLAGS += /I$$SHADOWCHAT_PREVIEW_BUILD/generated

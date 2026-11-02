include(dev_preview_test.pro)
QT += testlib
TARGET = sticker_rendering_test
SOURCES = $$PWD/sticker_rendering_test.cpp
win32-msvc*: QMAKE_CXXFLAGS += /I$$SHADOWCHAT_PREVIEW_BUILD/generated

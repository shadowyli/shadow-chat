include(dev_preview_test.pro)
QT += testlib
TARGET = transfer_presentation_test
SOURCES = $$PWD/transfer_presentation_test.cpp
win32-msvc*: QMAKE_CXXFLAGS += /I$$SHADOWCHAT_PREVIEW_BUILD/generated

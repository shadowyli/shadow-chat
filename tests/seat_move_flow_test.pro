include(dev_preview_test.pro)
TARGET = seat_move_flow_test
QT += testlib
SOURCES = $$PWD/seat_move_flow_test.cpp
win32-msvc*: QMAKE_CXXFLAGS += /I$$SHADOWCHAT_PREVIEW_BUILD/generated

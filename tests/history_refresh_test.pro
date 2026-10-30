include(dev_preview_test.pro)
TARGET = history_refresh_test
SOURCES = $$PWD/history_refresh_test.cpp
win32-msvc*: QMAKE_CXXFLAGS += /I$$SHADOWCHAT_PREVIEW_BUILD/generated

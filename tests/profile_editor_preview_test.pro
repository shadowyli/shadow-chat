include(dev_preview_test.pro)
TARGET = profile_editor_preview_test
SOURCES = $$PWD/profile_editor_test.cpp
win32-msvc*: QMAKE_CXXFLAGS += /I$$SHADOWCHAT_PREVIEW_BUILD/generated

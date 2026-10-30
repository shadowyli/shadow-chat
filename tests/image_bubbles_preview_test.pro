include(dev_preview_test.pro)
TARGET = image_bubbles_preview_test
SOURCES = $$PWD/image_bubbles_test.cpp
win32-msvc*: QMAKE_CXXFLAGS += /I$$SHADOWCHAT_PREVIEW_BUILD/generated

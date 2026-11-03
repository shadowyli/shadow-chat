include(dev_preview_test.pro)
TARGET = avatar_profile_interaction_test
QT += testlib
SOURCES = $$PWD/avatar_profile_interaction_test.cpp
win32-msvc*: QMAKE_CXXFLAGS += /I$$SHADOWCHAT_PREVIEW_BUILD/generated

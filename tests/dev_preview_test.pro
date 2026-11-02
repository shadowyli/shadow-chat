QT += core gui widgets network xml multimedia printsupport
CONFIG += console c++11 release
CONFIG -= app_bundle debug_and_release
TEMPLATE = app
TARGET = dev_preview_test
INCLUDEPATH += $$PWD/../src/core $$PWD/../src/utils $$PWD/../src/desktop $$PWD/../src/ecdh $$PWD/../src/sharedesktop $$PWD/../src/voicechat $$PWD/../src/gui $$PWD/../src/override $$PWD/../src/qxt $$PWD/../src
SOURCES += $$PWD/dev_preview_test.cpp
DEFINES += SHADOWCHAT_BUILD_LOCAL SHADOWCHAT_SAFE_BUILD MAKE_BEEBEEP_PORTABLE BEEBEEP_USE_VOICE_CHAT BEEBEEP_USE_HUNSPELL HUNSPELL_STATIC BEEBEEP_USE_QXT BEEBEEP_USE_SHAREDESKTOP
win32-msvc*: QMAKE_CXXFLAGS += /utf-8
isEmpty(SHADOWCHAT_PREVIEW_BUILD): error(Set SHADOWCHAT_PREVIEW_BUILD to dist/dev-preview/production)
INCLUDEPATH += $$SHADOWCHAT_PREVIEW_BUILD/generated
objects = $$files($$SHADOWCHAT_PREVIEW_BUILD/objects/*.obj)
objects -= $$SHADOWCHAT_PREVIEW_BUILD/objects/Main.obj
PRE_TARGETDEPS += $$objects
LIBS += $$objects user32.lib wtsapi32.lib shell32.lib

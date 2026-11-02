QT += core gui widgets testlib
CONFIG += console c++11 release
CONFIG -= app_bundle debug_and_release
TEMPLATE = app
TARGET = window_shadow_test
INCLUDEPATH += $$PWD/../src/desktop
SOURCES += $$PWD/window_shadow_test.cpp \
  $$PWD/../src/desktop/ShadowWindowShadow.cpp \
  $$PWD/../src/desktop/ShadowWindowResize.cpp
HEADERS += $$PWD/../src/desktop/ShadowWindowShadow.h \
  $$PWD/../src/desktop/ShadowWindowResize.h \
  $$PWD/../src/desktop/ShadowWindowShape.h
win32: LIBS += user32.lib gdi32.lib
win32-msvc*: QMAKE_CXXFLAGS += /utf-8

!isEmpty(SHADOWCHAT_PREVIEW_BUILD) {
  QT += network xml multimedia printsupport
  DEFINES += SHADOW_WINDOW_MAIN_PREVIEW SHADOWCHAT_BUILD_LOCAL SHADOWCHAT_SAFE_BUILD \
    MAKE_BEEBEEP_PORTABLE BEEBEEP_USE_VOICE_CHAT BEEBEEP_USE_HUNSPELL HUNSPELL_STATIC \
    BEEBEEP_USE_QXT BEEBEEP_USE_SHAREDESKTOP
  INCLUDEPATH += $$PWD/../src/core $$PWD/../src/utils $$PWD/../src/ecdh \
    $$PWD/../src/sharedesktop $$PWD/../src/voicechat $$PWD/../src/gui \
    $$PWD/../src/override $$PWD/../src/qxt $$PWD/../src \
    $$SHADOWCHAT_PREVIEW_BUILD/generated
  SOURCES += $$PWD/../src/desktop/GuiMain.cpp
  objects = $$files($$SHADOWCHAT_PREVIEW_BUILD/objects/*.obj)
  objects -= $$SHADOWCHAT_PREVIEW_BUILD/objects/Main.obj \
    $$SHADOWCHAT_PREVIEW_BUILD/objects/GuiMain.obj \
    $$SHADOWCHAT_PREVIEW_BUILD/objects/ShadowWindowShadow.obj \
    $$SHADOWCHAT_PREVIEW_BUILD/objects/ShadowWindowResize.obj
  PRE_TARGETDEPS += $$objects
  LIBS += $$objects wtsapi32.lib shell32.lib
}

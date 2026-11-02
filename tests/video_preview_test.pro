isEmpty(SHADOWCHAT_PREVIEW_BUILD) {
  QT += core gui widgets testlib
  CONFIG += console c++11 release
  CONFIG -= app_bundle debug_and_release
  TEMPLATE = app
  INCLUDEPATH += $$PWD/../src/desktop
  SOURCES += $$PWD/video_preview_test.cpp \
             $$PWD/../src/desktop/ShadowVideoPreview.cpp \
             $$PWD/../src/desktop/ShadowStyle.cpp \
             $$PWD/../src/desktop/ShadowDialogChrome.cpp \
             $$PWD/../src/desktop/ShadowWindowResize.cpp
  HEADERS += $$PWD/../src/desktop/ShadowVideoPreview.h
} else {
  include(dev_preview_test.pro)
  QT += testlib
  DEFINES += SHADOW_VIDEO_WITH_MAIN
  SOURCES = $$PWD/video_preview_test.cpp
}
TARGET = video_preview_test
win32-msvc*: QMAKE_CXXFLAGS += /utf-8
win32: LIBS += mfplat.lib mfreadwrite.lib mfuuid.lib ole32.lib

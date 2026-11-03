contains(CONFIG, transcript_integration) {
  include(dev_preview_test.pro)
  QT += testlib multimediawidgets
  production_transcript = $$SHADOWCHAT_PREVIEW_BUILD/objects/ShadowChatTranscript.obj
  PRE_TARGETDEPS -= $$production_transcript
  LIBS -= $$production_transcript
  DEFINES += SHADOW_TRANSCRIPT_REAL_IMAGES
  SOURCES = $$PWD/transcript_layout_animation_test.cpp \
            $$PWD/../src/desktop/ShadowChatTranscript.cpp
} else:isEmpty(SHADOWCHAT_PREVIEW_BUILD) {
  QT += widgets testlib network multimedia printsupport
  CONFIG += console c++11 release testcase
  CONFIG -= app_bundle debug_and_release
  TEMPLATE = app
  INCLUDEPATH += $$PWD/../src/desktop $$PWD/../src/core
  SOURCES = $$PWD/transcript_layout_animation_test.cpp \
            $$PWD/../src/desktop/ShadowChatTranscript.cpp
  HEADERS = $$PWD/../src/desktop/ShadowChatTranscript.h
} else {
  include(dev_preview_test.pro)
  QT += testlib
  SOURCES = $$PWD/transcript_layout_animation_test.cpp
}
TARGET = transcript_layout_animation_test
contains(CONFIG, transcript_integration): TARGET = transcript_image_integration_test
win32-msvc*: QMAKE_CXXFLAGS += /utf-8

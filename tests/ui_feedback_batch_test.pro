include(dev_preview_test.pro)
TARGET = ui_feedback_batch_test
QT += testlib
SOURCES = $$PWD/ui_feedback_batch_test.cpp
contains(CONFIG, height_probe) {
  production_transcript = $$SHADOWCHAT_PREVIEW_BUILD/objects/ShadowChatTranscript.obj
  PRE_TARGETDEPS -= $$production_transcript
  LIBS -= $$production_transcript
  SOURCES += $$PWD/../src/desktop/ShadowChatTranscript.cpp
}

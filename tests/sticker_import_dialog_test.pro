isEmpty(SHADOWCHAT_PREVIEW_BUILD) {
  QT += core gui gui-private widgets testlib
  CONFIG += console c++11 release
  CONFIG -= app_bundle debug_and_release
  TEMPLATE = app
  INCLUDEPATH += $$PWD/../src/core $$PWD/../src/desktop
  SOURCES += $$PWD/sticker_import_dialog_test.cpp \
             $$PWD/../src/desktop/GuiStickerPicker.cpp \
             $$PWD/../src/desktop/ShadowPopup.cpp \
             $$PWD/../src/core/ShadowStickerStore.cpp
  HEADERS += $$PWD/../src/desktop/GuiStickerPicker.h \
             $$PWD/../src/desktop/ShadowPopup.h
} else {
  include(dev_preview_test.pro)
  QT += gui-private testlib
  DEFINES += SHADOWCHAT_VERIFY_PRODUCTION_OBJECTS
  SOURCES = $$PWD/sticker_import_dialog_test.cpp
}
TARGET = sticker_import_dialog_test
win32-msvc*: QMAKE_CXXFLAGS += /utf-8

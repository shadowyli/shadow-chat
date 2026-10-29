QT += core gui widgets network xml multimedia printsupport
CONFIG += console c++11 release
CONFIG -= app_bundle debug_and_release
TEMPLATE = app
TARGET = update_core_gate_test
INCLUDEPATH += ../src/core ../src/utils ../src/desktop ../src/ecdh ../src/sharedesktop ../src/voicechat ../src/gui ../src
INCLUDEPATH += ../src/qxt ../src/hunspell ../src/override
include(../src/utils/shadow-update-config.pri)
SOURCES += update_core_gate_test.cpp
DEFINES += SHADOWCHAT_SAFE_BUILD MAKE_BEEBEEP_PORTABLE BEEBEEP_USE_VOICE_CHAT BEEBEEP_USE_HUNSPELL HUNSPELL_STATIC BEEBEEP_USE_QXT BEEBEEP_USE_SHAREDESKTOP
win32-msvc*: QMAKE_CXXFLAGS += /utf-8
objects = $$files($$PWD/../src/release/*.obj)
objects -= $$PWD/../src/release/Main.obj
objects -= $$PWD/../src/release/ShadowUpdateController.obj $$PWD/../src/release/moc_ShadowUpdateController.obj
contains(CONFIG, isolated_core_gate) {
  objects -= $$PWD/../src/release/Core.obj $$PWD/../src/release/CoreChat.obj $$PWD/../src/release/CoreConnection.obj $$PWD/../src/release/CoreParser.obj $$PWD/../src/release/CoreFileTransfer.obj
  objects -= $$PWD/../src/release/CoreVoiceChat.obj
  SOURCES += ../src/core/Core.cpp ../src/core/CoreChat.cpp ../src/core/CoreConnection.cpp ../src/core/CoreParser.cpp ../src/core/CoreFileTransfer.cpp ../src/core/CoreVoiceChat.cpp
  objects -= $$PWD/../src/release/moc_Core.obj $$PWD/../src/release/Updater.obj $$PWD/../src/release/moc_Updater.obj
  objects -= $$PWD/../src/release/ShadowUpdateGate.obj $$PWD/../src/release/moc_ShadowUpdateGate.obj
  objects -= $$PWD/../src/release/GuiMain.obj $$PWD/../src/release/moc_GuiMain.obj $$PWD/../src/release/Settings.obj
  objects -= $$PWD/../src/release/ShadowUpdateController.obj $$PWD/../src/release/moc_ShadowUpdateController.obj
  SOURCES += ../src/core/Settings.cpp ../src/desktop/GuiMain.cpp ../src/utils/Updater.cpp
  HEADERS += ../src/core/Core.h ../src/desktop/GuiMain.h ../src/utils/Updater.h
}
SOURCES += ../src/desktop/ShadowUpdateController.cpp
HEADERS += ../src/desktop/ShadowUpdateController.h
LIBS += $$objects user32.lib wtsapi32.lib shell32.lib

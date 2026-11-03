QT += core gui widgets network xml multimedia printsupport
CONFIG += console c++11 release
CONFIG -= app_bundle debug_and_release
TEMPLATE = app
TARGET = upgrade_profile_test
isEmpty(UPGRADE_SOURCE_ROOT): UPGRADE_SOURCE_ROOT = $$clean_path($$PWD/..)
isEmpty(UPGRADE_PRODUCTION_ROOT): error(Set UPGRADE_PRODUCTION_ROOT to the selected production object directory)
isEmpty(UPGRADE_BUILD_DEFINE): UPGRADE_BUILD_DEFINE = SHADOWCHAT_BUILD_LOCAL
INCLUDEPATH += $$UPGRADE_SOURCE_ROOT/src/core $$UPGRADE_SOURCE_ROOT/src/utils $$UPGRADE_SOURCE_ROOT/src/desktop $$UPGRADE_SOURCE_ROOT/src/ecdh $$UPGRADE_SOURCE_ROOT/src/sharedesktop $$UPGRADE_SOURCE_ROOT/src/voicechat $$UPGRADE_SOURCE_ROOT/src/gui $$UPGRADE_SOURCE_ROOT/src/override $$UPGRADE_SOURCE_ROOT/src/qxt $$UPGRADE_SOURCE_ROOT/src $$UPGRADE_PRODUCTION_ROOT/generated
DEFINES += $$UPGRADE_BUILD_DEFINE SHADOWCHAT_SAFE_BUILD MAKE_BEEBEEP_PORTABLE BEEBEEP_USE_VOICE_CHAT BEEBEEP_USE_HUNSPELL HUNSPELL_STATIC BEEBEEP_USE_QXT BEEBEEP_USE_SHAREDESKTOP
SOURCES = $$PWD/upgrade_profile_test.cpp
objects = $$files($$UPGRADE_PRODUCTION_ROOT/objects/*.obj)
objects -= $$UPGRADE_PRODUCTION_ROOT/objects/Main.obj
isEmpty(objects): error(No production objects found)
!exists($$UPGRADE_PRODUCTION_ROOT/objects/Settings.obj): error(Selected production objects lack Settings.obj)
!exists($$UPGRADE_PRODUCTION_ROOT/objects/MessageManager.obj): error(Selected production objects lack MessageManager.obj)
PRE_TARGETDEPS += $$objects
LIBS += $$objects user32.lib wtsapi32.lib shell32.lib gdi32.lib advapi32.lib
win32-msvc*: QMAKE_CXXFLAGS += /utf-8

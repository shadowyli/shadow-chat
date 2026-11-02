include(dev_preview_test.pro)
QT += testlib
TARGET = branding_icons_test
SOURCES = $$PWD/branding_icons_test.cpp \
          $$PWD/../src/desktop/GuiSystemTray.cpp \
          $$PWD/../src/gui/IconManager.cpp \
          $$PWD/../src/desktop/BeeApplication.cpp
RESOURCES += $$PWD/../src/beebeep.qrc
replaced = $$SHADOWCHAT_PREVIEW_BUILD/objects/GuiSystemTray.obj \
           $$SHADOWCHAT_PREVIEW_BUILD/objects/IconManager.obj \
           $$SHADOWCHAT_PREVIEW_BUILD/objects/BeeApplication.obj \
           $$SHADOWCHAT_PREVIEW_BUILD/objects/qrc_beebeep.obj
LIBS -= $$replaced
PRE_TARGETDEPS -= $$replaced

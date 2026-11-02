include(dev_preview_test.pro)
TARGET = dev_preview_gate_test
SOURCES = $$PWD/dev_preview_gate_test.cpp $$PWD/../src/desktop/ShadowDevPreview.cpp
SOURCES += $$PWD/../src/utils/Updater.cpp
DEFINES -= SHADOWCHAT_BUILD_LOCAL
isEmpty(PREVIEW_GATE_ENV): PREVIEW_GATE_ENV = MAIN
DEFINES += SHADOWCHAT_BUILD_$$PREVIEW_GATE_ENV
LIBS -= $$SHADOWCHAT_PREVIEW_BUILD/objects/ShadowDevPreview.obj
LIBS -= $$SHADOWCHAT_PREVIEW_BUILD/objects/Updater.obj

include(dev_preview_test.pro)
TARGET = onboarding_startup_test
SOURCES = $$PWD/onboarding_startup_test.cpp $$PWD/../src/desktop/GuiWizard.cpp $$PWD/../src/desktop/ShadowDialogChrome.cpp
objects -= $$SHADOWCHAT_PREVIEW_BUILD/objects/GuiWizard.obj $$SHADOWCHAT_PREVIEW_BUILD/objects/ShadowDialogChrome.obj
LIBS = $$objects user32.lib wtsapi32.lib shell32.lib

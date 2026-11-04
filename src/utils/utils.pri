
INCLUDEPATH += $$PWD

HEADERS += utils/BeeUtils.h \
  utils/GAnalytics.h \
  utils/HttpDownloader.h \
  utils/SpellChecker.h \
  utils/SpellCheckerHighlighter.h \
  utils/Updater.h \
  utils/ShadowUpdateManifest.h \
  utils/ShadowGitHubRelease.h \
  utils/ShadowGitHubFallback.h

SOURCES += utils/BeeUtils.cpp \
  utils/GAnalytics.cpp \
  utils/HttpDownloader.cpp \
  utils/SpellChecker.cpp \
  utils/SpellCheckerHighlighter.cpp \
  utils/Updater.cpp

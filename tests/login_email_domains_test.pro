QT += core gui widgets network xml multimedia printsupport testlib
CONFIG += console c++11 testcase release
CONFIG -= app_bundle debug_and_release
TEMPLATE = app
TARGET = login_email_domains_test
win32-msvc*: QMAKE_CXXFLAGS += /utf-8
INCLUDEPATH += $$PWD/../src $$PWD/../src/core $$PWD/../src/desktop
SOURCES += $$PWD/login_email_domains_test.cpp \
           $$PWD/../src/desktop/ShadowLocalAccounts.cpp \
           $$PWD/../src/desktop/ShadowWorkspaceArchive.cpp
HEADERS += $$PWD/../src/desktop/ShadowLocalAccounts.h \
           $$PWD/../src/desktop/ShadowWorkspaceArchive.h
emailDomainArrow.files = $$PWD/../src/images/shadowchat/icons/chevron-down.svg
emailDomainArrow.prefix = /
emailDomainArrow.base = $$PWD/../src
RESOURCES += emailDomainArrow

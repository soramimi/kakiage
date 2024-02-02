TARGET = kakiage
TEMPLATE = app
CONFIG += console c++17
CONFIG -= app_bundle
CONFIG -= qt

DESTDIR = $$PWD/out

LIBS += -lcurl -lssl -lcrypto

SOURCES += \
        base64.cpp \
        htmlencode.cpp \
        main.cpp \
        misc.cpp \
        strtemplate.cpp \
        urlencode.cpp \
        webclient.cpp

HEADERS += \
	base64.h \
	htmlencode.h \
	misc.h \
	strformat.h \
	strtemplate.h \
	urlencode.h \
	webclient.h

win32 {
	SOURCES += Win32Process.cpp
	HEADERS += Win32Process.h
}
!win32 {
	SOURCES += UnixProcess.cpp
	HEADERS += UnixProcess.h
}

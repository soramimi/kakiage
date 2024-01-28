TARGET = kakiage
TEMPLATE = app
CONFIG += console c++17
CONFIG -= app_bundle
CONFIG -= qt

DESTDIR = $$PWD/out

LIBS += -lcurl

SOURCES += \
        htmlencode.cpp \
        main.cpp \
        strtemplate.cpp \
        urlencode.cpp

HEADERS += \
	htmlencode.h \
	strtemplate.h \
	urlencode.h

win32 {
	SOURCES += Win32Process.cpp
	HEADERS += Win32Process.h
}
!win32 {
	SOURCES += UnixProcess.cpp
	HEADERS += UnixProcess.h
}

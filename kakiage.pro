TARGET = kakiage
TEMPLATE = app
CONFIG += console c++17
CONFIG -= app_bundle
CONFIG -= qt

DESTDIR = $$PWD/out

linux {
	LIBS += -lssl -lcrypto
}
macx:INCLUDEPATH += /usr/local/Cellar/openssl@3/3.1.1/include
macx:LIBS += /usr/local/Cellar/openssl@3/3.1.1/lib/libssl.a /usr/local/Cellar/openssl@3/3.1.1/lib/libcrypto.a

win32:msvc {
	INCLUDEPATH += "C:\Qt\Tools\OpenSSLv3\Win_x64\include"
	LIBS += "-LC:\Qt\Tools\OpenSSLv3\Win_x64\lib"
	LIBS += -llibcrypto -llibssl
}

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

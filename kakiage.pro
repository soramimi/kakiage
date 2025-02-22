QMAKE_PROJECT_DEPTH = 0

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
	INCLUDEPATH += "C:\openssl-ci\OpenSSL\include"
	LIBS += "-LC:\openssl-ci\OpenSSL\lib"
	LIBS += -llibcrypto -llibssl
}

SOURCES += \
        base64.cpp \
        htmlencode.cpp \
        kakiage.cpp \
        main.cpp \
        urlencode.cpp \
        webclient.cpp

HEADERS += \
	base64.h \
	htmlencode.h \
	kakiage.h \
	strformat.h \
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

DISTFILES += \
	test.in \
	test.txt \
	test.ka \
	test.sh

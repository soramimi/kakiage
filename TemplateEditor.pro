TARGET = ted
TEMPLATE = app
CONFIG += console c++17
CONFIG -= app_bundle
CONFIG -= qt

DESTDIR = $$PWD/out

LIBS += -lcurl

SOURCES += \
        charvec.cpp \
        htmlencode.cpp \
        main.cpp \
        strtemplate.cpp \
        urlencode.cpp

HEADERS += \
	charvec.h \
	htmlencode.h \
	strtemplate.h \
	urlencode.h

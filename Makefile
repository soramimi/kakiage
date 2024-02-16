
TARGET := kakiage

CXXFLAGS := -O3 -I..
LIBS := -lssl -lcrypto

SOURCES := \
	htmlencode.cpp \
	kakiage.cpp \
	urlencode.cpp \
	UnixProcess.cpp \
	webclient.cpp \
	base64.cpp \
	htmlencode.cpp \
	main.cpp

OBJECTS := $(SOURCES:%.cpp=%.o)

all: $(TARGET)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $^ -o $@

$(TARGET): $(OBJECTS)
	$(CXX) $(LDFLAGS) $^ -o $@ $(LIBS)

clean:
	-rm $(TARGET)
	-rm *.o

install: $(TARGET)
	install -m 755 $(TARGET) /usr/local/bin/$(TARGET)

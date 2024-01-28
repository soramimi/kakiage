
TARGET := kakiage

CXXFLAGS := -O3 -I..
LIBS := -lcurl

SOURCES := \
	htmlencode.cpp \
	strtemplate.cpp \
	urlencode.cpp \
	UnixProcess.cpp \
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

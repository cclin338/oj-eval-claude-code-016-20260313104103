CXX = g++
CXXFLAGS = -std=c++14 -O2 -Wall

TARGET = code
SOURCES = main.cpp
HEADERS = bptree.hpp

all: $(TARGET)

$(TARGET): $(SOURCES) $(HEADERS)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SOURCES)

clean:
	rm -f $(TARGET) *.o *.dat *.db

.PHONY: all clean

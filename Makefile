CXX = g++
CXXFLAGS = -std=c++11 -Wall -Wextra -O2

# Target executable and source file(s)
TARGET = ishell
SRCS = ishell.cpp

all: $(TARGET)

$(TARGET): $(SRCS)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SRCS)

clean:
	rm -f $(TARGET)

.PHONY: all clean

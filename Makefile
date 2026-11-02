CXX ?= g++
CXXFLAGS = -O3 -std=c++17 -Wall -Wextra -Iinclude

SRCS = src/u_baseline.cpp src/tsf_index.cpp src/msf_index.cpp src/main.cpp
TARGET = build/window_cc.exe

all: $(TARGET)

$(TARGET): $(SRCS) include/*.h
	$(CXX) $(CXXFLAGS) $(SRCS) -o $(TARGET)

clean:
	-rm -f $(TARGET)
	-del /f /q build\window_cc.exe 2>nul

.PHONY: all clean

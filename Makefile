CXX = g++
CXXFLAGS = -std=c++20 -funsigned-char -O2 -Wall -Wextra -Wno-unused-parameter
LDFLAGS = -lm

SRCS = main.cpp bytecode/bytecode.cpp bytecode/prototype.cpp ast/ast.cpp lua/lua.cpp
OBJS = $(SRCS:.cpp=.o)
TARGET = luajit-decompiler-v2

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all clean

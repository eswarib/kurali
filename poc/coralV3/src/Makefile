# Compiler and flags
CXX := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -O2

# Include directories
INCLUDES := -I/usr/local/include \
            -I../../whispercpp/include \
            -I../../whispercpp/ggml/include

# Library directories
LIBDIRS := -L/usr/local/lib -L../../whispercpp/lib

# Libraries to link
LIBS:= -lwhisper -lpthread -lm -lportaudio -lsndfile -lX11

# Paths
SRC_DIR := .
BIN_DIR := ../bin
TARGET := $(BIN_DIR)/kurali

# Source and object files
SRCS := $(wildcard $(SRC_DIR)/*.cpp)
OBJS := $(patsubst $(SRC_DIR)/%.cpp, $(BIN_DIR)/%.o, $(SRCS))

# Default target
all: $(TARGET)

# Link object files into final binary
$(TARGET): $(OBJS)
	@mkdir -p $(BIN_DIR)
	$(CXX) $(CXXFLAGS) -Wl,-rpath,/opt/coral/lib $(LIBDIRS) -o $@ $^ $(LIBS)

# Compile source files to object files
$(BIN_DIR)/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# Clean
clean:
	rm -rf $(BIN_DIR)

.PHONY: all clean


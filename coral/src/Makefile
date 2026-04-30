# Compiler and flags
CXX := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -O2 -lX11

# Wayland portal support (libportal) - detect pkg name
PORTAL_PKG := $(shell pkg-config --exists libportal-1 && echo libportal-1 || (pkg-config --exists libportal && echo libportal))
PORTAL_CFLAGS := $(shell pkg-config --cflags $(PORTAL_PKG) gio-2.0 gobject-2.0 glib-2.0)
PORTAL_LIBS := $(shell pkg-config --libs $(PORTAL_PKG) gio-2.0 gobject-2.0 glib-2.0)
CXXFLAGS += -DHAVE_LIBPORTAL $(PORTAL_CFLAGS)
LIBS += $(PORTAL_LIBS)

# AT-SPI (accessibility) optional support
ATSPI_CFLAGS := $(shell pkg-config --cflags atspi-2 2>/dev/null)
ATSPI_LIBS := $(shell pkg-config --libs atspi-2 2>/dev/null)
ifneq (,$(ATSPI_CFLAGS))
  CXXFLAGS += -DHAVE_ATSPI $(ATSPI_CFLAGS)
  LIBS += $(ATSPI_LIBS)
endif

# xdo static library and include
XDO_LIB := ../../libxdo/libxdo.a
XDO_INC := -I../../libxdo

# Include directories
INCLUDES := -I/usr/local/include \
            -I/usr/include \
            -I../../whispercpp/include \
            -I../../whispercpp/ggml/include \
            $(XDO_INC)

# Library directories
LIBDIRS := -L/usr/local/lib -L../lib

# Libraries to link
LIBS:= -lwhisper -lpthread -lm -lportaudio -lsndfile $(LIBS)

# Paths
SRC_DIR := .
BIN_DIR := ../bin
TARGET := $(BIN_DIR)/kurali

# Source and object files
SRCS := $(wildcard $(SRC_DIR)/*.cpp)
OBJS := $(patsubst $(SRC_DIR)/%.cpp, $(BIN_DIR)/%.o, $(SRCS))

RPATH := /usr/local/lib:../lib

# Default target
all: $(TARGET)

# Link object files into final binary
$(TARGET): $(OBJS)
	@mkdir -p $(BIN_DIR)
		$(CXX) $(CXXFLAGS) $(LIBDIRS) -Wl,-rpath,$(RPATH) -o $@ $^ $(XDO_LIB) -lXinerama -lXtst -lX11 -lxkbcommon $(LIBS)

# Compile source files to object files
$(BIN_DIR)/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# Clean
clean:
	rm -rf $(BIN_DIR)

.PHONY: all clean


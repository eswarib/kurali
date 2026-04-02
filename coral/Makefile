# Unit test sources
TEST_DIR := test
BIN_DIR := bin
TEST_SRCS := $(TEST_DIR)/TestA11yInjector.cpp $(TEST_DIR)/TestWaylandInjector.cpp $(TEST_DIR)/TestTextInjector.cpp
TEST_OBJS := $(patsubst $(TEST_DIR)/%.cpp, $(BIN_DIR)/%.test.o, $(TEST_SRCS))
TEST_BINS := $(patsubst $(TEST_DIR)/%.cpp, $(BIN_DIR)/%, $(TEST_SRCS))

# Build all tests
.PHONY: tests

alltests: backend $(TEST_BINS) $(BIN_DIR)/TestTextUtils

# Standalone (links only TextUtils.o; avoids heavy injector deps)
$(BIN_DIR)/TestTextUtils: $(TEST_DIR)/TestTextUtils.cpp $(BIN_DIR)/TextUtils.o
	@mkdir -p $(BIN_DIR)
	$(CXX) $(CXXFLAGS) -o $@ $< $(BIN_DIR)/TextUtils.o

# Link libraries and objects for tests
# Prefer vendored static libxdo if available; otherwise fall back to system -lxdo
XDO_CAND1 := ../libxdo/libxdo.a
XDO_CAND2 := ../../libxdo/libxdo.a
ifeq (,$(wildcard $(XDO_CAND1)))
  ifeq (,$(wildcard $(XDO_CAND2)))
    XDO_STATIC :=
    XDO_FALLBACK := -lxdo
  else
    XDO_STATIC := $(XDO_CAND2)
  endif
else
  XDO_STATIC := $(XDO_CAND1)
endif

# Library search paths and rpath so libwhisper can find ggml libs in ./lib (not needed for tests now)
LIBDIRS := -L/usr/local/lib -L./lib
RPATH := /usr/local/lib:./lib

# AT-SPI optional libs (needed if A11yInjector was compiled with HAVE_ATSPI)
ATSPI_LIBS := $(shell pkg-config --libs atspi-2 2>/dev/null)

# Core libs for tests (no whisper/ggml)
TEST_LIBS := -lXinerama -lXtst -lX11 -lxkbcommon -lpthread -lm -lportaudio -lsndfile -lportal -lgio-2.0 -lgobject-2.0 -lglib-2.0 $(ATSPI_LIBS) $(XDO_FALLBACK)

# Source files and objects from src
SRC_DIR := src
SRCS := $(wildcard $(SRC_DIR)/*.cpp)
OBJS := $(patsubst $(SRC_DIR)/%.cpp, $(BIN_DIR)/%.o, $(SRCS))
# Exclude main.o when linking tests to avoid multiple-definition of main
OBJS_NO_MAIN := $(filter-out $(BIN_DIR)/main.o,$(OBJS))

# Minimal object set needed by tests (avoid transcriber/whisper linkage)
TEST_LINK_OBJS := \
	$(BIN_DIR)/A11yInjector.o \
	$(BIN_DIR)/WaylandDetector.o \
	$(BIN_DIR)/WaylandInjector.o \
	$(BIN_DIR)/WaylandSession.o \
	$(BIN_DIR)/X11Injector.o \
	$(BIN_DIR)/X11WindowUtils.o \
	$(BIN_DIR)/ClipboardOwner.o \
	$(BIN_DIR)/Logger.o \
	$(BIN_DIR)/Utils.o \
	$(BIN_DIR)/TextInjector.o \
	$(BIN_DIR)/TextUtils.o

$(BIN_DIR)/%: $(TEST_DIR)/%.cpp $(TEST_LINK_OBJS)
	@mkdir -p $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $(LIBDIRS) -Wl,-rpath,$(RPATH) -o $@ $< $(TEST_LINK_OBJS) $(XDO_STATIC) $(TEST_LIBS)

# Run all tests
.PHONY: runtests
runtests: alltests
	@for test in $(TEST_BINS); do echo "Running $$test"; $$test; done
	@echo "Running $(BIN_DIR)/TestTextUtils"
	@$(BIN_DIR)/TestTextUtils

# Add TextInjector.cpp and Recorder.cpp explicitly if not already included
SRCS += $(SRC_DIR)/TextInjector.cpp
OBJS += $(BIN_DIR)/TextInjector.o
SRCS += $(SRC_DIR)/Recorder.cpp
OBJS += $(BIN_DIR)/Recorder.o 

.PHONY: all backend frontend clean

all: backend frontend

# Invoke the backend Makefile in src/
backend:
	$(MAKE) -C src

# Build Electron frontend
frontend:
	cd ../coral-electron && npm install
	# Uncomment if you have a build step:
	# cd ../coral-electron && npm run build

clean:
	$(MAKE) -C src clean
	cd ../coral-electron && rm -rf node_modules

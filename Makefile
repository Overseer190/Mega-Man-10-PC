# ============================================================================
# Makefile - MegaMan 10 Remake (SDL3 + BASS + BASSMIDI + MinGW-w64, C++)
# ============================================================================
# Usage:
#   make          - build the project
#   make run      - build and run
#   make clean    - remove build artifacts
# ============================================================================

# --- Project ----------------------------------------------------------------
TARGET := out/megaman10.exe
SRCDIR := src
BUILDDIR := out

# --- Toolchain (x86_64 MinGW-w64) ------------------------------------------
CXX := g++
CXXFLAGS := -Wall -Wextra -O2 -std=c++17
LDFLAGS :=

# --- SDL3 paths -------------------------------------------------------------
SDL_DIR := SDL3-3.4.10/x86_64-w64-mingw32
SDL_INC := -I$(SDL_DIR)/include
SDL_LIB := -L$(SDL_DIR)/lib
SDL_LIBS := -lSDL3

# --- BASS paths --------------------------------------------------------------
BASS_DIR := bass24
BASS_INC := -I$(BASS_DIR)/c
BASS_LIB := -L$(BASS_DIR)/c/x64
BASS_LIBS := -lbass

# --- BASSMIDI paths ---------------------------------------------------------
BASSMIDI_DIR := bassmidi24
BASSMIDI_INC := -I$(BASSMIDI_DIR)/c
BASSMIDI_LIB := -L$(BASSMIDI_DIR)/c/x64
BASSMIDI_LIBS := -lbassmidi

# --- Extra libraries --------------------------------------------------------
LDFLAGS += -lm
TEST_LIBS := -lSDL3_test

# --- Source files -----------------------------------------------------------
SRCS := $(wildcard $(SRCDIR)/*.cpp)
OBJS := $(patsubst $(SRCDIR)/%.cpp,$(BUILDDIR)/%.o,$(SRCS))

# --- Default target ---------------------------------------------------------
all: $(BUILDDIR) $(TARGET)

$(BUILDDIR):
	@mkdir -p $(BUILDDIR)

# --- Link -------------------------------------------------------------------
$(TARGET): $(OBJS)
	$(CXX) $^ -o $@ $(BASS_LIB) $(BASSMIDI_LIB) $(SDL_LIB) $(BASS_LIBS) $(BASSMIDI_LIBS) $(TEST_LIBS) $(SDL_LIBS) $(LDFLAGS)
	@echo "Build done: $(TARGET)"

# --- Compile ----------------------------------------------------------------
$(BUILDDIR)/%.o: $(SRCDIR)/%.cpp
	$(CXX) $(CXXFLAGS) $(SDL_INC) $(BASS_INC) $(BASSMIDI_INC) -c $< -o $@

# --- Run -------------------------------------------------------------------
run: all
	cd out && megaman10.exe

# --- Clean ------------------------------------------------------------------
clean:
	rm -f $(OBJS) $(TARGET)
	rmdir $(BUILDDIR) 2>nul || exit 0
	-del /f out\SDL3.dll 2>nul

.PHONY: all clean run

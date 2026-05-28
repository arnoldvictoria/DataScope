CXX      := g++
CXXFLAGS := -std=c++17 -Wall -O2 -DUNICODE
LDFLAGS  := -static
LDLIBS   := -lopengl32 -lgdi32 -ldwmapi -lsetupapi -luuid -lole32

IMGUI_DIR  := third_party/imgui
IMPLOT_DIR := third_party/implot
SRC_DIR    := src
BUILD_DIR  := build
TARGET     := $(BUILD_DIR)/DataScope.exe

INCLUDES := -I$(IMGUI_DIR) -I$(IMGUI_DIR)/backends -I$(IMPLOT_DIR) -I$(SRC_DIR)

SOURCES := $(SRC_DIR)/main.cpp \
           $(SRC_DIR)/parser.cpp \
           $(SRC_DIR)/serial_reader.cpp \
           $(IMGUI_DIR)/imgui.cpp \
           $(IMGUI_DIR)/imgui_draw.cpp \
           $(IMGUI_DIR)/imgui_tables.cpp \
           $(IMGUI_DIR)/imgui_widgets.cpp \
           $(IMGUI_DIR)/backends/imgui_impl_win32.cpp \
           $(IMGUI_DIR)/backends/imgui_impl_opengl3.cpp \
           $(IMPLOT_DIR)/implot.cpp \
           $(IMPLOT_DIR)/implot_items.cpp

OBJS := $(addprefix $(BUILD_DIR)/,$(notdir $(SOURCES:.cpp=.o)))

.PHONY: all clean run

all: $(TARGET)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(TARGET): $(BUILD_DIR) $(OBJS)
	$(CXX) $(LDFLAGS) -o $@ $(OBJS) $(LDLIBS)

# Each .o depends on its specific .cpp + compile rule
$(BUILD_DIR)/main.o:                      $(SRC_DIR)/main.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@
$(BUILD_DIR)/parser.o:                    $(SRC_DIR)/parser.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@
$(BUILD_DIR)/serial_reader.o:             $(SRC_DIR)/serial_reader.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@
$(BUILD_DIR)/imgui.o:                     $(IMGUI_DIR)/imgui.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@
$(BUILD_DIR)/imgui_draw.o:                $(IMGUI_DIR)/imgui_draw.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@
$(BUILD_DIR)/imgui_tables.o:              $(IMGUI_DIR)/imgui_tables.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@
$(BUILD_DIR)/imgui_widgets.o:             $(IMGUI_DIR)/imgui_widgets.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@
$(BUILD_DIR)/imgui_impl_win32.o:          $(IMGUI_DIR)/backends/imgui_impl_win32.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@
$(BUILD_DIR)/imgui_impl_opengl3.o:        $(IMGUI_DIR)/backends/imgui_impl_opengl3.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@
$(BUILD_DIR)/implot.o:                    $(IMPLOT_DIR)/implot.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@
$(BUILD_DIR)/implot_items.o:              $(IMPLOT_DIR)/implot_items.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

clean:
	rm -rf $(BUILD_DIR)

run: $(TARGET)
	$(TARGET)

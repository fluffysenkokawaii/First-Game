BUILD_DIR:=build
SRC_DIR:=src
OBJ_DIR:=$(BUILD_DIR)/tmp
TARGET:=$(BUILD_DIR)/main

CC := clang
CXX := clang++
CFLAGS := -Wall -g -O0 
CXXFLAGS := -Wall -g -O0

SRCS := $(shell cd $(SRC_DIR) && find . -name "*.cpp" -o -name "*.c")
DEPS := $(shell find . -name "*.h")
# In SRC:(this) equal to (build/obj/this.o)
OBJS := $(SRCS:%=$(OBJ_DIR)/%.o)

LDFLAGS := $(shell pkg-config --libs sdl3 sdl3-ttf)
# INC_FLAGS := -I./include

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(OBJS) -o $(TARGET) $(LDFLAGS)

$(OBJ_DIR)/%.cpp.o: $(SRC_DIR)/%.cpp $(DEPS)
	mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@ $(INC_FLAGS)

$(OBJ_DIR)/%.c.o: $(SRC_DIR)/%.c $(DEPS)
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@ $(INC_FLAGS)


.PHONY: all clean run

clean:
	rm -r $(BUILD_DIR)

run: $(TARGET)
	$(TARGET)

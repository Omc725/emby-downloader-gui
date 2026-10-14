CXX ?= g++
CORE_DIR ?= ../emby-downloader-core

CXXFLAGS ?= -std=c++17 -Wall -Wextra -O2 -Iinclude -I$(CORE_DIR)/include $(shell pkg-config --cflags gtk+-3.0)
LDFLAGS ?= $(shell pkg-config --libs gtk+-3.0) $(CORE_DIR)/libemby_core.a -lcurl -lpthread

SRCS = src/poster_cache.cpp \
       src/episode_filter.cpp \
       src/episode_dialog.cpp \
       src/server_dialog.cpp \
       src/queue_view.cpp \
       src/search_view.cpp \
       src/app_window.cpp \
       src/main.cpp

OBJS = $(SRCS:.cpp=.o)
TARGET = emby-downloader-gui

.PHONY: all clean core

all: core $(TARGET)

core:
	$(MAKE) -C $(CORE_DIR) static

$(TARGET): $(OBJS) $(CORE_DIR)/libemby_core.a
	$(CXX) -o $@ $(OBJS) $(LDFLAGS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)

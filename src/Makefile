target=file-system
CC=g++
CXXFLAGS += -std=c++17  -g -w

SRCS = file.cpp inode.cpp main.cpp namei.cpp super.cpp sys.cpp truncate.cpp disk.cpp bitmap.cpp printfc.cpp

# Object files
OBJS = $(SRCS:.cpp=.o)

# Default target
all: $(target)

$(target): $(OBJS)
	$(CC) -o $@ $(OBJS) $(LIBS) $(HEADERS)

%.o: %.cpp
	$(CC) -c $< -o $@ $(CXXFLAGS) $(HEADERS)

.PHONY: format
format:
	clang-format -style=google -i $(SRCS)

.PHONY: clean
clean:
	rm -f $(OBJS) $(target)    

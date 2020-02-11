.PHONY: all run test clean

TARGET=$(notdir $(CURDIR)).so
RELEASE ?= 0
CC=g++

ifeq ($(RELEASE), 1)
	CFLAGS=-Iinclude -I../../libpv/include -L../../libpv -Wall -std=c++11 -fPIC -O3
else
	CFLAGS=-Iinclude -I../../libpv/include -L../../libpv -Wall -std=c++11 -fPIC -O0 -g -fsanitize=address
endif

LIBS=-lpv -ltbb

OBJS=$(patsubst src/%.cpp, obj/%.o, $(wildcard src/*.cpp))

all: $(TARGET)

clean:
	rm -rf obj
	rm -f $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $(LIBS) -shared -o $@ $(filter %.o, $^)

obj/%.d : src/%.cpp
	mkdir -p obj; $(CC) $(CFLAGS) -M $< > $@

-include $(patsubst src/%.cpp, obj/%.d, $(wildcard src/*.cpp))  

obj/%.o: src/%.cpp
	mkdir -p obj; $(CC) $(CFLAGS) -c -o $@ $^

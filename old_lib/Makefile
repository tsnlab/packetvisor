.PHONY: all run test clean cleanall

TARGET=libpv
RELEASE ?= 0
CC=g++

ifeq ($(RELEASE), 1)
	CFLAGS=-Iinclude -Wall -std=c++11 -fPIC -O3
else
	CFLAGS=-Iinclude -Wall -std=c++11 -fPIC -O0 -g -fsanitize=address
endif

LIBS=-ldl

OBJS=$(patsubst src/%.cpp, obj/%.o, $(wildcard src/*.cpp))

all: $(TARGET).so $(TARGET).a

clean:
	rm -rf obj
	rm -f $(TARGET).so
	rm -f $(TARGET).a

$(TARGET).so: $(OBJS)
	$(CC) $(CFLAGS) $(LIBS) -shared -o $@ $(filter %.o, $^)

$(TARGET).a: $(OBJS)
	ar rc $@ $(filter %.o, $^)

obj/%.d : src/%.cpp
	mkdir -p obj; $(CC) $(CFLAGS) -M $< > $@

-include $(patsubst src/%.cpp, obj/%.d, $(wildcard src/*.cpp))  

obj/%.o: src/%.cpp
	mkdir -p obj; $(CC) $(CFLAGS) -c -o $@ $^

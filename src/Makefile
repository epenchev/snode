CC=g++
INC_DIR=./include
CPPFLAGS = -I$(INC_DIR) -O0 -g3 -Wall
LDLIBS = -lpthread -lboost_system -lboost_thread #-lboost_iostreams
EXECUTABLE=smkit

all: $(EXECUTABLE)

OBJECTS = $(patsubst %.cpp, %.o, $(wildcard *.cpp))
OBJECT_MODULES = $(wildcard ../modules/*/*.o)

clean:
	rm -f $(OBJECTS) smkit *~

$(EXECUTABLE): $(OBJECTS) $(OBJECT_MODULES)
	$(CC) -O0 -g3 -Wall $(OBJECTS) $(OBJECT_MODULES) -o $@ $(LDLIBS)
	

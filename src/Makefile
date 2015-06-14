CC=g++
INC_DIR=/usr/include/boost
CPPFLAGS = -I$(INC_DIR) -std=c++11 -O0 -g3 -Wall 
LDLIBS = -lpthread -lboost_system -lboost_thread #-lboost_iostreams
EXECUTABLE=smkit
OBJECTS = $(patsubst %.cpp, %.o, $(wildcard *.cpp))

all: $(EXECUTABLE)

clean:
	rm -f $(OBJECTS) $(EXECUTABLE) *~

$(EXECUTABLE): $(OBJECTS) $(OBJECT_MODULES)
	$(CC) $(OBJECTS) -o $@ $(LDLIBS)
	

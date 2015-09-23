CC=g++
# empty for the preprocessor
CPPFLAGS =
CXXFLAGS = -std=c++11 -O0 -g3 -Wall #-Weffc++ 
LDLIBS = -lpthread -lboost_system -lboost_thread #-lboost_iostreams
EXECUTABLE=snode
OBJECTS = $(patsubst %.cpp, %.o, $(wildcard *.cpp))

.PHONY: all clean

all: $(EXECUTABLE)

clean:
	rm -f $(OBJECTS) $(EXECUTABLE) *~

$(EXECUTABLE): $(OBJECTS) $(OBJECT_MODULES)
	$(CC) $(OBJECTS) -o $@ $(LDLIBS)
	

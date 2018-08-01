VNPATH = /home/pi/vnproglib-1.1/cpp/
MCCPATH = /home/pi/libuldaq-1.0.0/
LIBPATH = -L$(VNPATH)/build/bin -L/usr/local/lib
LIBS =  -luldaq -lvncxx -lpthread
INCLUDE = -I$(VNPATH)/include -I$(MCCPATH)/examples
FLAGS = -Wall -g

SOURCE = main
EXEC = getData

all: $(EXEC) extract

$(EXEC) : $(SOURCE).cpp
	g++ $(FLAGS) -o $@ $^ $(LIBPATH) $(LIBS) $(INCLUDE)

extract: extract.cpp
	g++ $(FLAGS) -o $@ $^ $(LIBPATH) $(INCLUDE) $(LIBS)
clean:
	rm -i *.o $(EXEC)

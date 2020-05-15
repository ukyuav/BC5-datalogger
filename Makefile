VNPATH = /home/pi/DATA/vnproglib-1.1.4.0/cpp/
MCCPATH = /home/pi/DATA/libuldaq-1.0.0/
LIBPATH = -L$(VNPATH)/build/bin -L/usr/local/lib
LIBS =  -luldaq -lvncxx -lpthread -lwiringPi
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

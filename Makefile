
# ACHTUNG unbedingt TABS benutzen beim einrücken

CC = g++
ifeq ($(DEBUG), yes)
CFLAGS = -ggdb -pthread -std=c++14
else
CFLAGS = -Wall -O3 -pthread -std=c++14 -ffunction-sections -fdata-sections
endif
LDFLAGS = -Wl,--gc-sections -lpthread -static-libgcc -static-libstdc++
TARGET1 = libsrvlib.a
TARGET2 = ExampleSrv

LIB = -l srvlib
LIB_PATH = -L .

#OBJ = $(patsubst %.cpp,%.o,$(wildcard *.cpp))
OBJ = ServMain.o ExampleSrv.o

all: $(TARGET1) $(TARGET2)

$(TARGET2) : ExampleSrv.o
	$(CC) -o $(TARGET2) ExampleSrv.o $(LIB_PATH) $(LIB) $(LDFLAGS)

$(TARGET1): ServMain.o
	ar rs $@ $^

ExampleSrv.o: ExampleSrv.cpp Service.h
	$(CC) $(CFLAGS) $(INC_PATH) -c $<

ServMain.o: ServMain.cpp Service.h
	$(CC) $(CFLAGS) $(INC_PATH) -c $<

clean:
	rm -f $(TARGET) $(OBJ) *~


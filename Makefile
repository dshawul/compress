CC = g++
CFLAGS = -O3 -g
DEFINES =
LDFLAGS = 

EXE = compress
RM = rm -rf
OBJ = main.o decompress.o codec.o compress.o

$(EXE): $(OBJ)
	$(CC) $(CFLAGS) $(DEFINES) $(LDFLAGS) $(OBJ) -o $(EXE)

%.o: %.cpp
	$(CC) $(CFLAGS) $(DEFINES) -c $<
clean:
	$(RM) $(OBJ)

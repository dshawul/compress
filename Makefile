#CC = g++
#CFLAGS = -O3 -Wno-unused-result

CC = x86_64-w64-mingw32-g++
CFLAGS = -O3 -Wno-unused-result -static

DEFINES =
LDFLAGS = 

EXE = compress
RM = rm -rf
OBJ = main.o decompress.o codec.o compress.o crc32.o

$(EXE): $(OBJ)
	$(CC) $(CFLAGS) $(DEFINES) $(LDFLAGS) $(OBJ) -o $(EXE)

%.o: %.cpp
	$(CC) $(CFLAGS) $(DEFINES) -c $<
clean:
	$(RM) $(OBJ) $(EXE)

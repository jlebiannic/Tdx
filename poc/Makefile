CC=cc -g
CFLAGS=-DEDISERVER_C=3013 -I/data1/home/jlebiannic/lib -I/data1/home/jlebiannic/DEVEL/TX5_2_UNICODE/Linux/lib 
LDFLAGS=-L/data1/home/jlebiannic/lib -L/data1/home/jlebiannic/DEVEL/TX5_2_UNICODE/Linux/lib -lruntime -ledi -lrls -ltdxdao -lpq -llogsystem -lpthread -ldl -lruntime -lm 
EXEC=TstBaseAccess

all: cleanAll $(EXEC)

TstBaseAccess: TstBaseAccess.o 
	$(CC) -o $@ $^ $(LDFLAGS)

TstBaseAccess.o: TstBaseAccess.c
	$(CC) -o TstBaseAccess.o -c TstBaseAccess.c $(CFLAGS)

clean:
	rm -rf *.o

cleanAll: clean
	rm -rf $(EXEC)



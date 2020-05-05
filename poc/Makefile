DBG=-DDEBUG
CC=cc -g
CFLAGS= $(DBG) -DEDISERVER_C=3013 -I/data1/home/jlebiannic/lib -I/data1/home/jlebiannic/DEVEL/TX5_2_UNICODE/Linux/lib 
LDFLAGS=-L/usr/pgsql-12/lib -L. -L/data1/home/jlebiannic/lib -L/data1/home/jlebiannic/DEVEL/TX5_2_UNICODE/Linux/lib -lruntime -ledi -lrls -ltdxdao -lpq -llogsystem -lpthread -ldl -lruntime -lm 
EXEC=TstBaseAccess TstPerf pgedisend pginterm

all: $(EXEC)

TstBaseAccess: TstBaseAccess.o libtdxdao.so
	$(CC) -o $@ $^ $(LDFLAGS)

TstBaseAccess.o: TstBaseAccess.c
	$(CC) -o TstBaseAccess.o -c TstBaseAccess.c $(CFLAGS)

testperf: TstPerf
	./TstPerf

TstPerf.c: TstPerf.rte 
	mktr -c $^

TstPerf: TstPerf.o  libtdxdao.so
	$(CC) -o $@ $^ $(LDFLAGS)

TstPerf.o: TstPerf.c
	$(CC) -o TstPerf.o -c TstPerf.c $(CFLAGS)

pgedisend.c: pgedisend.rte 
	mktr -c $^

pgedisend: pgedisend.o  libtdxdao.so
	$(CC) -o $@ $^ $(LDFLAGS)

pgedisend.o: pgedisend.c
	$(CC) -o $@ -c $^ $(CFLAGS)

pginterm.c: pginterm.rte 
	mktr -c $^

pginterm: pginterm.o  libtdxdao.so
	$(CC) -o $@ $^ $(LDFLAGS)

pginterm.o: pginterm.c
	$(CC) -o $@ -c $^ $(CFLAGS)

simple.c: simple.rte 
	mktr -c $^

simple: simple.o  libtdxdao.so
	$(CC) -o $@ $^ $(LDFLAGS)

simple.o: simple.c
	$(CC) -o $@ -c $^ $(CFLAGS)

clean:
	rm -rf *.o 

cleanAll: clean
	rm -rf $(EXEC) ${EDIHOME}/lib/libtdxdao.so  libtdxdao.so


ROOT=/data1/home/jlebiannic/DEVEL/TX5_2_UNICODE/Linux/src
LOGSYSLIB=$(ROOT)/logsystem/lib
SQLITE=$(ROOT)/3rdParty/sqlite/3_8_11_1/include64
PG=/usr/pgsql-12/include
CFLAGS_B= $(DBG) -I$(ROOT) -I $(EDIHOME)/lib -I$(LOGSYSLIB) -I$(SQLITE) -I$(PG) -O3 -Wall -g -c -fPIC -DEDISERVER_C=3013 
DAOOBJ= commons.o daoFactory.o dao.o pgDao.o

all: libtdxdao.so

libtdxdao.so:  $(DAOOBJ) 
	cc -shared $(DAOOBJ) -o libtdxdao.so

install: ${EDIHOME}/lib/libtdxdao.so 

${EDIHOME}/lib/libtdxdao.so: libtdxdao.so
	cp libtdxdao.so ${EDIHOME}/lib

pgDao.o: ../data-access/dao/pg/pgDao.c 
	$(CC) $(CFLAGS_B) -o $@ $^  
dao.o: ../data-access/dao/dao.c 
	$(CC) $(CFLAGS_B) -o $@ $^  
commons.o: ../data-access/commons/commons.c 
	$(CC) $(CFLAGS_B) -o $@ $^  
daoFactory.o: ../data-access/commons/daoFactory.c 
	$(CC) $(CFLAGS_B) -o $@ $^  



CFLAGS_C= $(DBG) -I$(ROOT) -I $(EDIHOME)/lib -I$(PG) -O3 -Wall -g -fPIC 
LDFLAGS_C=-L/usr/pgsql-12/lib -L. -lpq 

testlo: testlo.c
	$(CC) $(CFLAGS_C) $(LDFLAGS_C) -o $@ $^

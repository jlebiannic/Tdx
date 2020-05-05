ROOT="/data1/home/jlebiannic/DEVEL/TX5_2_UNICODE/Linux/src"
LOGSYSLIB="$ROOT/logsystem/lib"
SQLITE="$ROOT/3rdParty/sqlite/3_8_11_1/include64"
PG="/usr/pgsql-12/include"

cc -DDEBUG -I$ROOT -I$LOGSYSLIB -I$SQLITE -I$PG -O3 -Wall -g -c -fPIC -DEDISERVER_C=3013 -o "pgDao.o" "/data1/home/jlebiannic/DEVEL/TX5_2_UNICODE/Linux/src/data-access/dao/pg/pgDao.c"
cc -DDEBUG -I$ROOT -I$LOGSYSLIB -I$SQLITE -I$PG -O3 -Wall -g -c -fPIC -DEDISERVER_C=3013 -o "dao.o" "/data1/home/jlebiannic/DEVEL/TX5_2_UNICODE/Linux/src/data-access/dao/dao.c"
cc -DDEBUG -I$ROOT -I$LOGSYSLIB -I$SQLITE -I$PG -O3 -Wall -g -c -fPIC -DEDISERVER_C=3013 -o "commons.o" "/data1/home/jlebiannic/DEVEL/TX5_2_UNICODE/Linux/src/data-access/commons/commons.c"
cc -DDEBUG -I$ROOT -I$LOGSYSLIB -I$SQLITE -I$PG -O3 -Wall -g -c -fPIC -DEDISERVER_C=3013 -o "daoFactory.o" "/data1/home/jlebiannic/DEVEL/TX5_2_UNICODE/Linux/src/data-access/commons/daoFactory.c"

cc -DDEBUG -I$ROOT -I$LOGSYSLIB -I$SQLITE -O3 -Wall -g -c -fPIC -DEDISERVER_C=3013 -o "CoManager.o" "/data1/home/jlebiannic/DEVEL/TX5_2_UNICODE/Linux/src/data-access/CoManager.c"

cc -shared commons.o daoFactory.o dao.o pgDao.o -o libtdxdao.so
cp libtdxdao.so $EDIHOME/lib

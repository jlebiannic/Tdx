OID=${1:-16455}
#echo exporting OID $OID ..
#./testlo -D postgres -O ${OID} -a "/tmp/outfile01.txt" &
#./testlo -D postgres -O ${OID} -a "/tmp/outfile02.txt" &
#./testlo -D postgres -O ${OID} -a "/tmp/outfile03.txt" &
#./testlo -D postgres -O ${OID} -a "/tmp/outfile04.txt" &
#./testlo -D postgres -O ${OID} -a "/tmp/outfile05.txt" &
#./testlo -D postgres -O ${OID} -a "/tmp/outfile06.txt" &
#./testlo -D postgres -O ${OID} -a "/tmp/outfile07.txt" &
#./testlo -D postgres -O ${OID} -a "/tmp/outfile08.txt" &
#./testlo -D postgres -O ${OID} -a "/tmp/outfile09.txt" &
#./testlo -D postgres -O ${OID} -a "/tmp/outfile10.txt" &
(
./pgedisend &
./pgedisend &
./pgedisend &
./pgedisend &
./pgedisend &
./pgedisend &
./pgedisend &
./pgedisend &
./pgedisend &
./pgedisend &
./pgedisend &
./pgedisend &
./pgedisend &
./pgedisend &
./pgedisend &
./pgedisend &
./pgedisend &
./pgedisend &
./pgedisend &
./pgedisend &
./pgedisend &
./pgedisend &
./pgedisend &
./pgedisend &
./pgedisend &
./pgedisend &
./pgedisend &
./pgedisend &
./pgedisend &
./pgedisend &
./pgedisend &
./pgedisend &
./pgedisend &
./pgedisend &
./pgedisend &
) | grep "Creating INDEX"
wait

gcc -o ../bin/kill_ccu.o -lzmq ../scripts/kill_ccu.c
gcc -o ../bin/dcu_ref.o -lzmq ../dcu/dcu_ref.c
gcc -o ../bin/dcu_arduino.o -lzmq ../dcu/dcu_arduino.c
gcc -o ../bin/dcu_ard_ref.o -lzmq ../dcu/dcu_ard_ref.c
gcc -o ../bin/ccu-ru.o -lzmq ../ccu/ccu-ru.c
gcc -o ../bin/ccu.o -lzmq ../ccu/ccu.c
gcc -o ../bin/ccu-db.o ../ccu/ccu-db.c -lpq -std=c99

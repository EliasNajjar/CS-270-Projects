CFLAGS = -g

all:  driver

driver:	driver.o scanner.o reduce.o amap.o auxiliary.o
	gcc $(CFLAGS) -o $@ $^


# Makefile
objs = msr.o bitops.o cpuid.o sample.o

sample: Makefile $(objs)
	cc -Wall -o sample $(objs) -lpthread

sample.o: sample.c
	cc -Wall -c sample.c

pmc.o: msr.c msr.h
	cc -Wall -c msr.c

cpuid.o: cpuid.c
	cc -Wall -c cpuid.c

bitops.o: bitops.c bitops.h
	cc -Wall -c bitops.c

clean:
	rm -f *.o *~ *.csv sample

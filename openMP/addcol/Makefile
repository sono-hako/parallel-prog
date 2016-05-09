CC = mpicc
PROG = mpi_addcol

all:	$(PROG)

mpi_addcol:	mpi_addcol.o
	$(CC) $< -o $@ -lm

mpi_addcol.o:	mpi_addcol.c
	$(CC) -c $< -o $@ -lm

clean:
	rm -f $(PROG) *.o *~ core
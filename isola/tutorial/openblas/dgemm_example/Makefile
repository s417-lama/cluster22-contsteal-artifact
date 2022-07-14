VHOME     := $(or $(ISOLA_HOME),$(HOME))
BLAS_PATH := $(VHOME)/opt/openblas

dgemm: dgemm.c
	$(CC) -o dgemm dgemm.c -I$(BLAS_PATH)/include -L$(BLAS_PATH)/lib -Wl,-R$(BLAS_PATH)/lib -lopenblas

clean:
	rm -f dgemm

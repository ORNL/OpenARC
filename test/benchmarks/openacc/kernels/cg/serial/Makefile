NX ?= 8
NY ?= 8
NZ ?= 8
PRINT ?= 0

cg_serial: cg_serial.c
	gcc -O3 -D_Nx=$(NX) -D_Ny=$(NY) -D_Nz=$(NZ) -DDEBUG_PRINT=$(PRINT) -lm $< -o $@ 

clean:
	rm -rf cg_serial

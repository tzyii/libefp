# C compiler
CC= mpicc

# Fortran compiler
FC= ifort

# install prefix
PREFIX= /usr/local

# fragment library path
FRAGLIB= $(PREFIX)/share/libefp/fraglib

# additional link libraries
MYLIBS= -lifcore

# additional linker flags
MYLDFLAGS=

# additional C flags
MYCFLAGS= -DWITH_MPI -std=c99 -O3 -g -mkl=sequential -fopenmp

# additional Fortran flags
MYFFLAGS= -g

# ar command
AR= ar rcu

# ranlib command
RANLIB= ranlib

# rm command
RM= rm -f

# tags program
CTAGS= ctags

# tags file
TAGS= tags

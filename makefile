
PYINCDIR=/usr/include/python3.6m

TGTS += mount_clnt_ex
TGTS += $(if $(PYINCDIR),PyNfsDebug.so)
TGTS += nfs_write_tst

CFLAGS+= -O2 -g
LDFLAGS+= -g
CPPFLAGS+= $(addprefix -I,$(PYINCDIR) .)
CPPFLAGS+= -DGITVERSION=\"$(shell git describe --always)\"

CPP=$(CROSS)cpp
CXX=$(CROSS)g++
CC=$(CROSS)gcc
LD=$(CROSS)ld
CYTHON=cython

MNT_GEN_SRCS = mount_prot_clnt.c proto/mount_prot.h mount_prot_xdr.c
NFS_GEN_SRCS = nfs_prot_clnt.c   proto/nfs_prot.h nfs_prot_xdr.c
PY_GEN_SRCS  = PyNfsDebug.cc

GEN_SRCS+=$(MNT_GEN_SRCS)
GEN_SRCS+=$(NFS_GEN_SRCS)
GEN_SRCS+=$(PY_GEN_SRCS)

SRCS+=NfsDebug.h
SRCS+=NfsDebug.cc
SRCS+=$(GEN_SRCS)

NFSOBJS+=NfsDebug
NFSOBJS+=mount_prot_clnt
NFSOBJS+=mount_prot_xdr
NFSOBJS+=nfs_prot_clnt
NFSOBJS+=nfs_prot_xdr

PROTS+=proto/mount_prot.x
PROTS+=proto/nfs_prot.x

PYXS+=PyNfsDebug.pyx

all: targets

mount_prot_clnt.o: $(MNT_GEN_SRCS)
nfs_prot_clnt.o:   $(NFS_GEN_SRCS)

mount_clnt_ex.o: proto/mount_prot.h
mount_clnt_ex: mount_clnt_ex.o mount_prot_clnt.o mount_prot_xdr.o

%.c: proto/mount_prot.h proto/nfs_prot.h

%.cc: proto/mount_prot.h proto/nfs_prot.h

%.o: %.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

%.o: %.cc proto/mount_prot.h proto/nfs_prot.h
	$(CXX) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

nfs_write_tst: nfs_write_tst.o $(addsuffix .o,$(NFSOBJS))
	$(CXX) $(LDFLAGS) $^ -o $@

proto/%_prot.h: proto/%_prot.x
	rpcgen -h $< >$@

%_prot_clnt.c: proto/%_prot.x
	rpcgen -l $< >$@

%_prot_xdr.c: proto/%_prot.x
	rpcgen -c $< >$@

%-pic.o: %.c proto/mount_prot.h proto/nfs_prot.h
	$(CC) -fPIC $(CPPFLAGS) $(CFLAGS) -c $< -o $@

%-pic.o: %.cc proto/mount_prot.h proto/nfs_prot.h
	$(CXX) -fPIC $(CPPFLAGS) $(CFLAGS) -c $< -o $@

PyNfsDebug.cc: PyNfsDebug.pyx NfsDebug.h
	$(CYTHON) --cplus $< -o $@

PyNfsDebug.so: PyNfsDebug-pic.o $(addsuffix -pic.o,$(NFSOBJS))
	$(CXX) -shared -o$@ $^

dist: $(SRCS) $(PROTS) $(PYXS)

clean:
	$(RM) proto/mount_prot.h mount_prot_clnt.c mount_prot_xdr.c
	$(RM) proto/nfs_prot.h nfs_prot_clnt.c nfs_prot_xdr.c
	$(RM) *.o
	$(RM) $(TGTS)
	$(RM) PyNfsDebug.cc


-include local.mak

.SECONDEXPANSION:

targets: $(TGTS)

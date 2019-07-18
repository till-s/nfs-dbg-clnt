all: mount_clnt_ex PyNfsDebug.so

CFLAGS+= -O2 -g
LDFLAGS+= -g
CPPFLAGS+= -I/usr/include/python3.6m

mount_prot_clnt.o: mount_prot_clnt.c proto/mount_prot.h
nfs_prot_clnt.o: nfs_prot_clnt.c proto/nfs_prot.h

mount_clnt_ex.o: proto/mount_prot.h
mount_clnt_ex: mount_clnt_ex.o mount_prot_clnt.o mount_prot_xdr.o

%.o: proto/mount_prot.h proto/nfs_prot.h

%-pic.o: proto/mount_prot.h proto/nfs_prot.h


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
	cython3 --cplus $< -o $@

PyNfsDebug.so: PyNfsDebug-pic.o NfsDebug-pic.o mount_prot_clnt-pic.o mount_prot_xdr-pic.o nfs_prot_clnt-pic.o nfs_prot_xdr-pic.o
	$(CXX) -shared -o$@ $^

clean:
	$(RM) proto/mount_prot.h mount_prot_clnt.c mount_prot_xdr.c
	$(RM) proto/nfs_prot.h nfs_prot_clnt.c nfs_prot_xdr.c
	$(RM) *.o
	$(RM) mount_clnt_ex
	$(RM) PyNfsDebug.so
	$(RM) PyNfsDebug.cc

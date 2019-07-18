all: mount_clnt_ex

CFLAGS+= -O2

mount_prot_clnt.o: mount_prot_clnt.c proto/mount_prot.h

mount_clnt_ex: mount_clnt_ex.o mount_prot_clnt.o mount_prot_xdr.o

proto/%_prot.h: proto/%_prot.x
	rpcgen -h $< >$@

%_prot_clnt.c: proto/%_prot.x
	rpcgen -l $< >$@

%_prot_xdr.c: proto/%_prot.x
	rpcgen -c $< >$@

clean:
	$(RM) proto/mount_prot.h mount_prot_clnt.c mount_prot_xdr.c
	$(RM) *.o
	$(RM) mount_clnt_ex

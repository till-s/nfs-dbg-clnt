#ifndef NFS_DEBUG_H
#define NFS_DEBUG_H
#include "proto/mount_prot.h"
#include "proto/nfs_prot.h"
#include <rpc/rpc.h>
#include <string>
#include <string.h>
#include <stdint.h>

using std::string;

template <typename B> class StructWrap : public B {
public:
	StructWrap(const B *bp)
	: B( *bp )
	{
	}

	StructWrap()
	{
	}

	B *get()
	{
		return this;
	}
};

class FH : public StructWrap<nfs_fh> {
public:
	typedef char RawBytes[NFS_FHSIZE];

	FH(const nfs_fh *pfh)
    : StructWrap<nfs_fh>( pfh )
	{
	}

	void set(diropargs *ap)
	{
		ap->dir = *this;
	}
};

class DH : public StructWrap<nfscookie> {
public:
	DH(const nfscookie *c)
	: StructWrap<nfscookie>( c )
	{
	}

	DH()
	{
		memset( data, 0, sizeof(data) );
	}
};

class Clnt {
private:
	CLIENT *c_;
    int     s_;
public:
	Clnt(const char *cred = 0, const char *srv = 0, unsigned long prog = 0, unsigned long vers = 0, unsigned short locPort = 0, bool isUdp = true );

	CLIENT *get() { return c_; }

	virtual ~Clnt();
};

class Name {
public:
	char *name_;
	Name(const char *x = 0)
	{
		name_ = x ? ::strdup( x ) : 0;
	}

	~Name()
	{
		free( name_ );
	}
};

class NfsDebug {
private:
	Clnt    mntClnt_;
	Clnt    nfsClnt_;
	Name    m_;
	nfs_fh  f_;

private:
	virtual int  lkup1(diropargs *arg, fattr *f);

public:
	NfsDebug(const char *srv, const char *mnt, const char *nfscred = 0, unsigned short locNfsPort = 0, const char *mntcred = 0, unsigned short locMntPort = 0, bool useUdp = true);

	NfsDebug(const char *srv, nfs_fh     *mnt, const char *nfscred = 0, unsigned short locNfsPort = 0, bool useUdp = true);

	virtual int  lkup(diropargs *arg, fattr *res_attr = 0);
	virtual void dumpMounts();

	virtual int  read (nfs_fh *fh, u_int off, u_int count, void *buf = 0);
	virtual int  write(nfs_fh *fh, u_int off, u_int count, void *buf);
    virtual int  setattr(nfs_fh *fh, sattr *attrs);
    virtual int  getattr(nfs_fh *fh, fattr *attrs);

	virtual const nfs_fh *root() { return &f_; }

	virtual uint32_t getNfsXid();
	virtual void     setNfsXid(uint32_t xid);

	virtual void     rm(diropargs *arg);

    virtual void     sattrDefaults(sattr *attrs);
    virtual void     sattrEmpty(sattr *attrs);

	virtual int      creat(diropargs *arg, nfs_fh *newfh = 0, sattr *attrs = 0);

	// returned dirlist must be xdr_free'ed!
	virtual entry   *ls(nfs_fh *fh, unsigned count = 1000, nfscookie *poscookie = 0);

	virtual ~NfsDebug();
};

void FreeEntry(entry *);
#endif

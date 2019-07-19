#ifndef NFS_DEBUG_H
#define NFS_DEBUG_H
#include "proto/mount_prot.h"
#include "proto/nfs_prot.h"
#include <rpc/rpc.h>
#include <string>
#include <string.h>
#include <stdint.h>

using std::string;

class FH : public nfs_fh {
public:
	FH(const nfs_fh *pfh)
    : nfs_fh( *pfh )
	{
	}

	void set(diropargs *ap)
	{
		ap->dir = *this;
	}

	nfs_fh *get()
	{
		return this;
	}
};

class Clnt {
private:
	CLIENT *c_;
    int     s_;
public:
	Clnt( const char *srv, unsigned long prog, unsigned long vers, unsigned short locPort );

	CLIENT *get() { return c_; }

	virtual ~Clnt();
};

class Name {
public:
	char *name_;
	Name(const char *x)
	{
		name_ = ::strdup( x );
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
	virtual int  lkup1(diropargs *arg);

public:
	NfsDebug(const char *srv, const char *mnt, unsigned short locNfsPort = 0);

	virtual int  lkup(diropargs *arg);
	virtual void dumpMounts();

	virtual int  read (nfs_fh *fh, u_int off, u_int count, void *buf = 0);
	virtual int  write(nfs_fh *fh, u_int off, u_int count, void *buf);

	virtual const nfs_fh *root() { return &f_; }

	virtual uint32_t getNfsXid();
	virtual void     setNfsXid(uint32_t xid);

	virtual void     rm(diropargs *arg);

	virtual int      creat(diropargs *arg, nfs_fh *newfh = 0, sattr *attrs = 0);

	virtual ~NfsDebug();
};
#endif

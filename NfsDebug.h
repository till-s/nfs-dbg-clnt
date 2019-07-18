#ifndef NFS_DEBUG_H
#define NFS_DEBUG_H
#include "proto/mount_prot.h"
#include "proto/nfs_prot.h"
#include <rpc/rpc.h>
#include <string>
#include <string.h>

using std::string;

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
	fhandle f_;

public:
	NfsDebug(const char *srv, const char *mnt, unsigned short locNfsPort = 0);

	virtual void dumpMounts();

	virtual ~NfsDebug();
};
#endif

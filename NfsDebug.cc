#include "NfsDebug.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>

Clnt::Clnt( const char *srvn, unsigned long prog, unsigned long vers, unsigned short locPort )
 : s_( -1 )
{
struct timeval wai;
int            sd = RPC_ANYSOCK;
struct sockaddr_in me;
struct sockaddr_in srv;

	wai.tv_sec  = 2;
	wai.tv_usec = 0;

	srv.sin_family      = AF_INET;
	srv.sin_addr.s_addr = inet_addr( srvn );
	srv.sin_port        = htons( 0 );

	if ( locPort ) {
		sd = socket( AF_INET, SOCK_DGRAM, 0 );
		if ( sd < 0 ) {
			throw "Unable to create Socket";
		}
		me.sin_family      = AF_INET;
		me.sin_addr.s_addr = htonl( INADDR_ANY );
		me.sin_port        = htons( locPort    );
		if ( bind( sd, (struct sockaddr*) &me, sizeof( me ) ) ) {
			close( sd );
			throw "Unable to bind socket";
		}
		s_ = sd;
	}
	
	if ( ! ( c_ = clntudp_create( &srv, prog, vers, wai, &sd ) ) ) {
		if ( s_ >= 0 ) {
			close( s_ );
		}
		clnt_pcreateerror( inet_ntoa( srv.sin_addr ) );
		throw "Unable to create client";
	}
}

Clnt::~Clnt()
{
	clnt_destroy( c_ );
	if ( s_ >= 0 ) {
		close( s_ );
	}
}

NfsDebug::NfsDebug(const char *host, const char *mnt, unsigned short locNfsPort)
 : mntClnt_( host, MOUNTPROG  , MOUNTVERS  , 0          ),
   nfsClnt_( host, NFS_PROGRAM, NFS_VERSION, locNfsPort ),
   m_      ( mnt                                        )
{
fhstatus *res;
dirpath   exprt = m_.name_;

	res = mountproc_mnt_1( &exprt, mntClnt_.get() );

	if ( ! res ) {
		clnt_perror( mntClnt_.get(), "call failed" );
		throw "Unable to mount";
	}

	if ( res->fhs_status ) {
		fprintf(stderr,"Unable to mount %s:%s -- %s\n", host, mnt, strerror( res->fhs_status ) );
		throw "Mount rejected";
	}

	memcpy( &f_.data, res->fhstatus_u.fhs_fhandle, sizeof( f_.data ) );
}

int
NfsDebug::lkup1(diropargs *arg)
{
diropres *res;
int       st;
	res = nfsproc_lookup_2( arg, nfsClnt_.get() );
	if ( ! res ) {
		clnt_perror( nfsClnt_.get(), "nfsproc_lookup -- call failed" );
		return -1;
	} 

	if ( res->status ) {
		fprintf( stderr, "nfsprook_lookup error: %s\n", strerror( res->status ) ) ;
		st = res->status;
	} else {
		memcpy( &arg->dir, &res->diropres_u.diropres.file, sizeof(arg->dir) );
		st = 0;
	}

	return st;
}

int
NfsDebug::lkup(diropargs *arg)
{
char      *slp, *path = arg->name;
char       ch;
int        st;

	while ( ( slp = strchr( arg->name, '/' ) ) ) {
		ch   = *slp;
		*slp = 0;

		st = lkup1( arg );

		*slp = ch;
		arg->name = slp + 1;

		if ( st ) {
			goto bail;
		}
	}

	st = lkup1( arg );

bail:
	arg->name = path;

	return st;
}

int
NfsDebug::read(nfs_fh *fh, u_int off, u_int count, void *buf)
{
readargs arg;
readres  res;
int      rval;
struct timeval tout;

	tout.tv_sec    = 5;
	tout.tv_usec   = 0;

	arg.file       = *fh;
	arg.offset     = off;
	arg.count      = count;
	arg.totalcount = count;

	memset( &res, 0, sizeof(res) );

	res.readres_u.reply.data.data_val = (caddr_t)buf;

	if ( clnt_call( nfsClnt_.get(), NFSPROC_READ,
	                (xdrproc_t) xdr_readargs, (caddr_t)&arg,
	                (xdrproc_t) xdr_readres , (caddr_t)&res,
	                tout ) != RPC_SUCCESS ) {
		clnt_perror( nfsClnt_.get(), "nfs read call failed" );
		return -1;
	}

	if ( ! res.status ) {
		rval = res.readres_u.reply.data.data_len;
	} else {
		rval = -res.status;
	}

	if ( ! buf ) {
		xdr_free( (xdrproc_t)xdr_readres, (caddr_t) &res );
	}
	return rval;
		

	
}

void
NfsDebug::dumpMounts()
{
void *argdummy;
mountlist   ml;
mountlist  *res;

	res = mountproc_dump_1( (void*)&argdummy, mntClnt_.get() );
	if ( ! res ) {
		clnt_perror( mntClnt_.get(), "mountproc_dump call failed" );
		return;
	}
	for ( ml = *res; ml; ml = ml->ml_next ) {
		printf( "%20s:%s\n", ml->ml_hostname, ml->ml_directory );
	}
	xdr_free( (xdrproc_t)xdr_mountlist, (caddr_t)res );
}

NfsDebug::~NfsDebug()
{
void *res;
	mountproc_umnt_1( &m_.name_, mntClnt_.get() );
	fprintf( stderr, "Leaving NfsDebug destructor\n" );
}

#include "NfsDebug.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/time.h>
#include <errno.h>
#include <stdio.h>

Clnt::Clnt( const char *srvn, unsigned long prog, unsigned long vers, unsigned short locPort )
 : s_( -1 )
{
struct timeval wai;
int            sd = RPC_ANYSOCK;
struct sockaddr_in me;
struct sockaddr_in srv;
const  char   *at, *host;
unsigned       uid, gid;
char           myname[256];

	wai.tv_sec  = 2;
	wai.tv_usec = 0;

	host = (at = ::strchr( srvn, '@' )) ? at + 1 : srvn;

	uid = getuid();
	gid = getgid();

	if ( gethostname( myname, sizeof(myname) ) ) {
		perror("gethostname failed");
		throw "gethostname failed";
	}

	if ( at ) {
		switch ( sscanf( srvn, "%d.%d@", &uid, &gid ) ) {
			case 2:
			case 1:
				break;
			default:
				switch ( sscanf( srvn, ".%d@", &gid ) ) {
					case '1':
						break;
					case '0':
						if ( at == srvn ) {
							break;
						}
						/* else fall through */
					default:
						fprintf( stderr, "Unable to parse server arg; expected: [[<uid>]['.'<gid>]'@']<host_ip>\n" );
						throw "Unable to parse server arg";
				}
				break;
		}
	}

	srv.sin_family      = AF_INET;
	srv.sin_addr.s_addr = inet_addr( host );
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

	c_->cl_auth = authunix_create( myname, uid, gid, 0, 0 );
}

Clnt::~Clnt()
{
	auth_destroy( c_->cl_auth );
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
		fprintf( stderr, "nfsproc_read returned error: %s\n", strerror( res.status ) );
		rval = -res.status;
	}

	if ( ! buf ) {
		xdr_free( (xdrproc_t)xdr_readres, (caddr_t) &res );
	}
	return rval;
		

	
}

int
NfsDebug::write(nfs_fh *fh, u_int off, u_int count, void *buf)
{
writeargs arg;
attrstat *res;

	arg.file          = *fh;
	arg.beginoffset   = 0; //ignored
	arg.offset        = off;
	arg.totalcount    = 0;
	arg.data.data_val = (caddr_t)buf;
	arg.data.data_len = count;


	res = nfsproc_write_2( &arg, nfsClnt_.get() );

	if ( ! res ) {
		clnt_perror( nfsClnt_.get(), "nfsproc_write call failed" );
		return -2;
	}
	if ( res->status ) {
		fprintf( stderr, "nfsproc_write returned error: %s\n", strerror( res->status ) );
		return -res->status;
	}
	return res->attrstat_u.attributes.size;
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

uint32_t
NfsDebug::getNfsXid()
{
uint32_t      rval;
unsigned long tmp;
	if ( ! clnt_control( nfsClnt_.get(), CLGET_XID, (caddr_t)&tmp ) ) {
		fprintf( stderr, "clnt_control(CLGET_XID) failed" );
		return 0;
	}
	rval = (uint32_t)tmp;
	return rval;
}

void
NfsDebug::setNfsXid(uint32_t xid)
{
unsigned long tmp = xid;
	if ( ! clnt_control( nfsClnt_.get(), CLSET_XID, (caddr_t)&tmp ) ) {
		fprintf( stderr, "clnt_control(CLSET_XID) failed" );
	}
}

void
NfsDebug::rm(diropargs *arg)
{
nfsstat *res;

	res = nfsproc_remove_2( arg, nfsClnt_.get() );
	if ( ! res ) {
		clnt_perror( nfsClnt_.get(), "nfsproc_remove call failed" );
		return;
	}
	if ( *res ) {
		fprintf( stderr, "nfsproc_remove returned error: %s\n", strerror( *res ) );
	}
}
	
int
NfsDebug::creat(diropargs *where, nfs_fh *newfh, sattr *attrs)
{
createargs     arg;
struct timeval now;
nfstime        nfsnow;
diropres      *res;

	arg.where = *where;
	if ( attrs ) {
		arg.attributes = *attrs;
	} else {
		gettimeofday( &now, 0 );
		memset      ( &arg.attributes, 0, sizeof( arg.attributes ) );
		nfsnow.seconds       = now.tv_sec;
		nfsnow.useconds      = now.tv_usec;
		arg.attributes.mode  = 0664; 
		arg.attributes.uid   = getuid();
		arg.attributes.gid   = getgid();
        arg.attributes.size  = 0;
        arg.attributes.atime = nfsnow;
        arg.attributes.mtime = nfsnow;
	}

	res = nfsproc_create_2( &arg, nfsClnt_.get() );
	if ( ! res ) {
		clnt_perror( nfsClnt_.get(), "nfsproc_create call failed" );
		return -1;
	}
	if ( res->status ) {
		fprintf( stderr, "nfsproc_create failed; error: %s\n", strerror( res->status ) );
		return res->status;
	}
	if ( newfh ) {
		memcpy( newfh, &res->diropres_u.diropres.file, sizeof( *newfh ) );
	}
	return 0;
}

entry *
NfsDebug::ls(nfs_fh *fh, unsigned count, nfscookie *poscookie)
{
readdirargs arg;
readdirres *res;
	arg.dir = *fh;
	if ( poscookie ) {
		arg.cookie = *poscookie;
	} else {
		memset( &arg.cookie, 0, sizeof( arg.cookie ) );
	}
	arg.count = count;
	res = nfsproc_readdir_2( &arg, nfsClnt_.get() );
	if ( ! res ) {
		clnt_perror( nfsClnt_.get(), "nfsproc_readdir call failed" );
		throw "nfsproc_readdir call failed";
	}
	if ( res->status ) {
		fprintf( stderr, "nfsproc_readdir failed; error: %s\n", strerror( res->status ) );
		throw "nfsproc_readdir failed with error";
	}
	if ( res->readdirres_u.reply.eof ) {
		printf( "EOF, %p\n", res->readdirres_u.reply.entries );
	}
	return res->readdirres_u.reply.entries;
}

void FreeEntry(entry *e)
{
	xdr_free( (xdrproc_t)xdr_entry, (caddr_t)e );
}

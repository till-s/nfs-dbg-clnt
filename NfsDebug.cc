#include "NfsDebug.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/time.h>
#include <errno.h>
#include <stdio.h>

Clnt::Clnt( const char *cred, const char *srvn, unsigned long prog, unsigned long vers, unsigned short locPort )
 : c_(  0 ),
   s_( -1 )
{
struct timeval wai;
int            sd = RPC_ANYSOCK;
struct sockaddr_in me;
struct sockaddr_in srv;
unsigned       uid, gid;
char           myname[256];

    if ( ! srvn ) {
        return;
    }

	wai.tv_sec  = 2;
	wai.tv_usec = 0;

	uid = getuid();
	gid = getgid();

	if ( gethostname( myname, sizeof(myname) ) ) {
		perror("gethostname failed");
		throw "gethostname failed";
	}

	if ( cred ) {
		switch ( sscanf( cred, "%d.%d", &uid, &gid ) ) {
			case 2:
			case 1:
				break;
			default:
				switch ( sscanf( srvn, ".%d", &gid ) ) {
					case '1':
						break;
					default:
						fprintf( stderr, "Unable to parse server arg; expected: [[<uid>]['.'<gid>]'@']<host_ip>\n" );
						throw "Unable to parse server arg";
				}
				break;
		}
	}

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

	printf("Creating credentials %u.%u\n", uid, gid );

	c_->cl_auth = authunix_create( myname, uid, gid, 0, 0 );
}

Clnt::~Clnt()
{
    if ( c_ ) {
        auth_destroy( c_->cl_auth );
        clnt_destroy( c_ );
    }
	if ( s_ >= 0 ) {
		close( s_ );
	}
}

NfsDebug::NfsDebug(const char *srv, const char *mnt, const char *nfscred, unsigned short locNfsPort, const  char *mntcred, unsigned short locMntPort)
 : mntClnt_( mntcred, srv, MOUNTPROG  , MOUNTVERS  , locMntPort ),
   nfsClnt_( nfscred, srv, NFS_PROGRAM, NFS_VERSION, locNfsPort ),
   m_      ( mnt                                                )
{
fhstatus *res;
dirpath   exprt = m_.name_;

	res = mountproc_mnt_1( &exprt, mntClnt_.get() );

	if ( ! res ) {
		clnt_perror( mntClnt_.get(), "call failed" );
		throw "Unable to mount";
	}

	if ( res->fhs_status ) {
		fprintf(stderr,"Unable to mount %s%c%s:%s -- %s\n", mntcred ? mntcred : "", mntcred ? '@' : ' ', srv, mnt, strerror( res->fhs_status ) );
		throw "Mount rejected";
	}

	memcpy( &f_.data, res->fhstatus_u.fhs_fhandle, sizeof( f_.data ) );
}

NfsDebug::NfsDebug(const char *srv, nfs_fh     *mnt, const char *nfscred, unsigned short locNfsPort)
 : nfsClnt_( nfscred, srv, NFS_PROGRAM, NFS_VERSION, locNfsPort )
{
    f_ = *mnt;
}

int
NfsDebug::lkup1(diropargs *arg, fattr *fa)
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
        if ( fa ) {
            *fa = res->diropres_u.diropres.attributes;
        }
		st = 0;
	}

	return st;
}

int
NfsDebug::lkup(diropargs *arg, fattr *fa)
{
char      *slp, *path = arg->name;
char       ch;
int        st;

	while ( ( slp = strchr( arg->name, '/' ) ) ) {
		ch   = *slp;
		*slp = 0;

		st = lkup1( arg, fa );

		*slp = ch;

		if ( st ) {
			goto bail;
		}
		arg->name = slp + 1;
	}

	st = *arg->name ? lkup1( arg, fa ) : 0;

	if ( 0 == st )
		arg->name = 0;

bail:

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
	arg.totalcount    = 0; //ignored
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

    if ( ! mntClnt_.get() ) {
        return;
    }

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
    if ( mntClnt_.get() ) {
        mountproc_umnt_1( &m_.name_, mntClnt_.get() );
    }
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

void
NfsDebug::sattrDefaults(sattr *attrs)
{
struct timeval now;
nfstime        nfsnow;

    gettimeofday( &now, 0 );
    memset      ( attrs, 0, sizeof( *attrs ) );
    nfsnow.seconds       = now.tv_sec;
    nfsnow.useconds      = now.tv_usec;
    attrs->mode  = 0664; 
    attrs->uid   = getuid();
    attrs->gid   = getgid();
    attrs->size  = 0;
    attrs->atime = nfsnow;
    attrs->mtime = nfsnow;
}

void
NfsDebug::sattrEmpty(sattr *attrs)
{
nfstime never;

    never.seconds  = -1;
    never.useconds = -1;

    attrs->mode  = -1;
    attrs->uid   = -1;
    attrs->gid   = -1;
    attrs->size  = -1;
    attrs->atime = never;
    attrs->mtime = never;
}

int
NfsDebug::setattr(nfs_fh *fh, sattr *attrs)
{
sattrargs      arg;
attrstat      *res;

    arg.file       = *fh;
    arg.attributes = *attrs;

    res = nfsproc_setattr_2( &arg, nfsClnt_.get() );

	if ( ! res ) {
		clnt_perror( nfsClnt_.get(), "nfsproc_setattr call failed" );
		return -2;
	}
	if ( res->status ) {
		fprintf( stderr, "nfsproc_setattr returned error: %s\n", strerror( res->status ) );
		return -res->status;
	}
    return 0;
}
	
int
NfsDebug::creat(diropargs *where, nfs_fh *newfh, sattr *attrs)
{
createargs     arg;
diropres      *res;

	arg.where = *where;
	if ( attrs ) {
		arg.attributes = *attrs;
	} else {
        sattrDefaults( &arg.attributes );
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

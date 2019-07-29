#include "NfsDebug.h"
#include <getopt.h>
#include <string.h>

#ifndef GITVERSION
#error "Makefile should define GITVERSION by passing -D option"
#endif

class S {
private:
	char *s_;
public:
	S(const char *s)
	: s_( strdup( s ) )
	{
	}

	char *getp()
	{
		return s_;
	}

	unsigned len()
	{ return strlen( s_ );
	}

	~S()
	{
		free( s_ );
	}
};

static void usage(const char *nm)
{
	fprintf( stderr, "Usage: %s [-hv] [-m mountport] [-n nfsport] [-M mount_creds] [-N nfs_creds] [-c fnam] [-x xid] -s server_ip -e export [message]\n", nm ); 
	fprintf( stderr, "      -h            : this message\n");
	fprintf( stderr, "      -v            : print git description ('version')\n");
	fprintf( stderr, "      -m mountport  : local port from where to send mount requests (defaults to any)\n");
	fprintf( stderr, "      -n nfsport    : local port from where to send nfs   requests (defaults to any)\n");
	fprintf( stderr, "      -M mountcreds : credentials ('[uid][.gid]') to use for mount requests (defaults to getuid/getgid)\n");
	fprintf( stderr, "      -N nfscreds   : credentials ('[uid][.gid]') to use for nfs requests (defaults to getuid/getgid)\n");
	fprintf( stderr, "      -s serverip   : IP (in dot notation) of NFS server\n");
	fprintf( stderr, "      -e path       : remote directory we want to mount\n");
	fprintf( stderr, "      -c filename   : name of file to create (optional)\n");
	fprintf( stderr, "      -x xid        : XID (seed) to use for first NFS operation (optional)\n");
	fprintf( stderr, "      message       : if given, written to filename (if -c present; ignored otherwise)\n");
}

static void listdir(NfsDebug *p, nfs_fh *fhp)
{
entry     *e, *ep, *pp;
DH         cookie;
	while ( ( e = p->ls( fhp, 2048, cookie.get() ) ) ) {
		ep = e;
		// at least executed once; pp is always defined
		while ( ep ) {
			printf( "%s\n", ep->name );
			pp = ep;
			ep = ep->nextentry;
		}
		// save cookie before releasing entries
		cookie = DH( &pp->cookie );
		FreeEntry( e );
	}
}


int
main(int argc, char **argv)
{
unsigned short nfsport = 0;
unsigned short mntport = 0;
const char    *nfscred = 0;
const char    *mntcred = 0;
const char    *exp     = 0;
const char    *srv     = 0;
int            rval    = 1;
unsigned short *s_p;
unsigned       *u_p;
int            opt;
const char    *fnam    = 0;
const char    *msg     = "HELLO\n";
unsigned       xid     = 0;

	while ( (opt = getopt(argc, argv, "s:N:M:e:n:m:hc:x:v")) > 0 ) {

		s_p = 0;
		u_p = 0;

		switch ( opt ) {
			case 'h': rval = 0;
				/* fall thru */
			default:  usage(argv[0]);
				return rval;

			case 's': srv = optarg;     break;

			case 'e': exp = optarg;     break;

			case 'N': nfscred = optarg; break;

			case 'M': mntcred = optarg; break;

			case 'c': fnam    = optarg; break;

			case 'm': s_p = &mntport;   break;

			case 'n': s_p = &nfsport;   break;

			case 'x': u_p = &xid;       break;

			case 'v':
				printf( "Git Version: %s\n", GITVERSION );
				return 0;
		}

		if ( s_p && 1 != sscanf( optarg, "%hu", s_p ) ) {
			fprintf( stderr, "Unable to scan arg to option -%c\n", opt );
			return 1;
		}
		if ( u_p && 1 != sscanf( optarg, "%u", u_p ) ) {
			fprintf( stderr, "Unable to scan arg to option -%c\n", opt );
			return 1;
		}
	}

	if ( ! srv || ! exp ) {
		fprintf(stderr, "%s: need '-s' and '-e' options!\n", argv[0]);
		usage( argv[0] );
		return 1;
	}

	if ( optind < argc ) {
		msg = argv[optind];
	}

try {

NfsDebug c(srv, exp, nfscred, nfsport, mntcred, mntport);

	if ( fnam ) {
		diropargs a;
		nfs_fh    fh;
		int       st;
		S         path( fnam );

		a.dir  = *c.root();
		a.name = path.getp();
		st = c.lkup( &a );

		if ( xid ) {
			c.setNfsXid( xid );
		}

		if ( 0 == st ) {
			if ( '/' == fnam[strlen(fnam) - 1] ) {
				listdir( &c, &a.dir );
				msg = 0;
			} else {
				fprintf(stderr,"Warning: File exists already (not recreating)\n");
				fh = a.dir;
			}
		} else {
			if ( strchr( a.name, '/' ) ) {
				fprintf(stderr,"Directory lookup failed (%s)!\n", strerror(st));
				return 1;
			} else {
				if ( c.creat( &a, &fh ) ) {
					fprintf(stderr, "Unable to create file: %s\n", strerror(st));
					return 1;
				}
			}
		}

		if ( msg ) {
			S msgrw( msg );

			st = c.write(&fh, 0, msgrw.len(), msgrw.getp());
			if ( st < 0 ) {
				fprintf( stderr, "Unable to write file: %s\n", strerror(-st) );
			}
			printf( "%d chars written\n", st );
		}
	}

} catch ( const char *e ) {
	fprintf( stderr, "ERROR: %s\n", e );
	return 1;
}


	return 0;
}

#include "NfsDebug.h"
#include <getopt.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#ifndef GITVERSION
#error "Makefile should define GITVERSION by passing -D option"
#endif

static void dumpFH(FILE *f, const nfs_fh *fh)
{
unsigned u;
    for ( u = 0; u < sizeof(fh->data)/sizeof(fh->data[0]); u++ ) {
        fprintf(f, "%02x", (unsigned char)fh->data[u]);
    }
    fprintf(f, "\n");
}

static int
writeAndVerify(NfsDebug *p, nfs_fh *fh, uint64_t attempt)
{
int      st;
uint64_t rbv;

    st = p->write( fh, 0, sizeof(attempt), &attempt );
    if ( st < 0 )
        return st;
    st = p->read( fh, 0, sizeof(rbv), &rbv );
    if ( st < 0 )
        return st;
    if ( rbv != attempt ) {
        fprintf(stderr,"Verification mismatch; expected %llu but read back %llu\n",
            (unsigned long long)attempt,
            (unsigned long long)rbv
        );
        return 1;
    }
    return 0;
}

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

template <typename T>
class PH {
private:
    T *p_;
public:
    PH(T *p) : p_( p ) {            }
    PH()     : p_( 0 ) {            }
    T *operator->()    { return p_; }
    T *operator&()     { return p_; }
    PH & operator=(T* p)
    {
        p_ = p;
        return *this;
    }
    ~PH()              { delete p_; }
};

static void usage(const char *nm)
{
	fprintf( stderr, "Usage: %s [-hdvFRtTW] [-m mountport] [-n nfsport] [-M mount_creds] [-N nfs_creds] [-f fnam] [-x xid] [-S seed] -s server_ip -e export | -r root_fh [message]\n", nm ); 
	fprintf( stderr, "      -h            : this message\n");
	fprintf( stderr, "      -v            : print git description ('version')\n");
	fprintf( stderr, "      -m mountport  : local port from where to send mount requests (defaults to any)\n");
	fprintf( stderr, "      -n nfsport    : local port from where to send nfs   requests (defaults to any)\n");
	fprintf( stderr, "      -M mountcreds : credentials ('[uid][.gid]') to use for mount requests (defaults to getuid/getgid)\n");
	fprintf( stderr, "      -N nfscreds   : credentials ('[uid][.gid]') to use for nfs requests (defaults to getuid/getgid)\n");
	fprintf( stderr, "      -s serverip   : IP (in dot notation) of NFS server\n");
	fprintf( stderr, "      -e path       : remote directory we want to mount\n");
	fprintf( stderr, "      -f filename   : name of file to create (optional)\n");
	fprintf( stderr, "      -x xid        : XID (seed) to use for first NFS operation (optional)\n");
	fprintf( stderr, "      -t            : Truncate file when writing\n" );
	fprintf( stderr, "      -d            : Dump mounts (server info)\n" );
	fprintf( stderr, "      -R            : Dump root handle\n" );
	fprintf( stderr, "      -F            : Dump file handle\n" );
	fprintf( stderr, "      -r fh_ascii   : Use root file-handle\n");
	fprintf( stderr, "      -S seed       : 'Seed' the server cache by performing 'seed'\n");
	fprintf( stderr, "      -T            : Use TCP instead of UDP for transport\n" );
	fprintf( stderr, "      -W            : Repeated writes with readback verification\n");
	fprintf( stderr, "                      using random XID\n");
	fprintf( stderr, "                      writes with different XID\n");
	fprintf( stderr, "      message       : if given, written to filename (if -f present; ignored otherwise)\n" );
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

static void
randomXidTest(NfsDebug *p, nfs_fh *fh)
{
uint32_t xid;
uint64_t attempt =  (uint64_t)-1LL;
int      st;

    do {
        ++attempt;
        xid = random();
        p->setNfsXid( xid );
    } while ( 0 == ( st = writeAndVerify( p, fh, attempt ) ) );
    fprintf(stderr,"writeAndVerify FAILED on attempt %llu due to ", (unsigned long long)attempt);
    if ( st > 0 ) {
        fprintf(stderr,"readback mismatch!\n");
    } else {
        fprintf(stderr,"error: %s\n", strerror( -st ) );
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
const char    *msg     = 0;
const char    *rootH   = 0;
unsigned       xid     = 0;
int            setXid  = 0;
int            trunc   = 0;
int            dumpM   = 0;
int            dumpR   = 0;
int            dumpF   = 0;
int            randTst = 0;
unsigned       u;
unsigned       seed    = 0;
char           rbuf[512];

	while ( (opt = getopt(argc, argv, "de:Ff:hM:m:N:n:Rr:S:s:TtvWx:")) > 0 ) {

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

			case 'f': fnam    = optarg; break;

			case 'm': s_p = &mntport;   break;

			case 'n': s_p = &nfsport;   break;

			case 't': trunc = 1;        break;
			case 'd': dumpM = 1;        break;
			case 'R': dumpR = 1;        break;
			case 'F': dumpF = 1;        break;

			case 'r': rootH = optarg;   break;

			case 'S': u_p   = &seed;    break;

			case 'W': randTst= 1;       break;

			case 'x': u_p    = &xid; 
                      setXid = 1;       break;

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

	if ( ! srv || (! exp && !rootH) ) {
		fprintf(stderr, "%s: need '-s' and '-e' or '-R' options!\n", argv[0]);
		usage( argv[0] );
		return 1;
	}

	if ( optind < argc ) {
		msg = argv[optind];
	}

    if ( seed && !msg ) {
        msg = "";
    }

try {
PH<NfsDebug> c;

    if ( rootH ) {
        nfs_fh root_fh;
        if ( 2*NFS_FHSIZE != strlen(rootH) ) {
            fprintf( stderr, "%s: arg to -R must be exactly %d (hex) chars\n", argv[0], 2*NFS_FHSIZE );
            return 1;
        }
        for ( u=0; u<NFS_FHSIZE; u++ ) {
            if ( 1 != sscanf( rootH + 2*u, "%02hhx", (unsigned char*)&root_fh.data[u] ) ) {
                fprintf( stderr, "%s: -R arg conversion to hex failed\n", argv[0] );
                return 1;
            }
        }
        c = new NfsDebug(srv, &root_fh, nfscred, nfsport);
    } else {
        c = new NfsDebug(srv, exp, nfscred, nfsport, mntcred, mntport);
    }

    if ( dumpR ) {
        dumpFH( stdout, c->root() );
    }

    if ( dumpM ) {
        c->dumpMounts();
    }

	if ( fnam ) {
		diropargs a;
		nfs_fh    fh;
        fattr     atts;
		int       st;
		S         path( fnam );

		a.dir  = *c->root();
		a.name = path.getp();
		st = c->lkup( &a, &atts );

		if ( setXid ) {
			c->setNfsXid( xid );
            srandom( xid );
		}

		if ( 0 == st ) {
            fh = a.dir;
            if ( dumpF ) {
                printf("FileHandle: ");
                dumpFH( stdout, &fh );
            }
			if ( '/' == fnam[strlen(fnam) - 1] ) {
				listdir( &c, &a.dir );
				msg = 0;
			} else if ( randTst ) {
                randomXidTest( &c, &fh );
                msg = 0;
			} else {
                if ( msg ) {
                    fprintf(stderr,"Warning: File exists already (not recreating)\n");
                    if ( trunc ) {
                        sattr newatts;
                        c->sattrEmpty( &newatts );
                        newatts.size = 0;
                        if ( (st = c->setattr( &fh, &newatts )) ) {
                            fprintf(stderr,"Truncating file failed (%s)\n", strerror(-st));
                            return 1;
                        }
                    }
                } else {
                    if ( (st = c->read( &fh, 0, sizeof(rbuf), rbuf )) < 0 ) {
                        fprintf(stderr, "Reading file failed (%s)\n", strerror( - st ) );
                        return 1;
                    }
                    fwrite(rbuf, sizeof(rbuf[0]), st, stdout);
                }
			}
		} else {
			if ( strchr( a.name, '/' ) ) {
				fprintf(stderr,"Directory lookup failed (%s)!\n", strerror(-st));
				return 1;
			} else {
                if ( msg ) {
                    if ( (st = c->creat( &a, &fh )) ) {
                        fprintf(stderr, "Unable to create file: %s\n", strerror(-st));
                        return 1;
                    }
                } else {
                    fprintf(stderr, "File does not exist: %s\n", fnam);
                    return 1;
                }
			}
		}

		if ( msg ) {
			S        msgrw( msg );
            const unsigned len = msgrw.len();
            fattr    attrs;
            uint64_t attempt, succ;

            if ( seed ) {

                for ( attempt = succ = 0; succ < seed; attempt++ ) {
                    if ( 0 == writeAndVerify( &c, &fh, attempt ) ) {
                        succ++;
                    }
                }
                printf("Seeding took %llu attempts\n", (unsigned long long)attempt);

            } else {
                st = c->write( &fh, 0, len, msgrw.getp() );
                if ( st < 0 ) {
                    fprintf( stderr,"Error writing file: %s\n", strerror( -st ) );
                } else {
                    printf( "%d chars written\n", st );
                }
            }
		}
	}

} catch ( const char *e ) {
	fprintf( stderr, "ERROR: %s\n", e );
	return 1;
}


	return 0;
}

/*
 * This is sample code generated by rpcgen.
 * These are only templates and you can use them
 * as a guideline for developing your own functions.
 */

#include "proto/mount_prot.h"


static int
rescheck(const char *nm, u_int st)
{
	if ( st ) {
		fprintf(stderr,"mountproc_%s failure: %s\n", nm, strerror(st));
		return 1;
	}
	return 0;
}

static void pex(exports e)
{
groups g;
	while ( e ) {
		printf("%s ->", e->ex_dir);
		for ( g = e->ex_groups; g; g = g->gr_next ) {
			printf(" %s", g->gr_name);
		}
		printf("\n");
		e = e->ex_next;
	}
}

void
mountprog_1(char *host, char *expo)
{
	CLIENT *clnt;
	void  *result_1;
	char *mountproc_null_1_arg;
	fhstatus  *result_2;
	dirpath  mountproc_mnt_1_arg = expo;
	mountlist  *result_3;
	char *mountproc_dump_1_arg;
	void  *result_4;
	dirpath  mountproc_umnt_1_arg = expo;
	void  *result_5;
	char *mountproc_umntall_1_arg;
	exports  *result_6;
	char *mountproc_export_1_arg;
	exports  *result_7;
	char *mountproc_exportall_1_arg;

#ifndef	DEBUG
	clnt = clnt_create (host, MOUNTPROG, MOUNTVERS, "udp");
	if (clnt == NULL) {
		clnt_pcreateerror (host);
		exit (1);
	}
#endif	/* DEBUG */

	result_1 = mountproc_null_1((void*)&mountproc_null_1_arg, clnt);
	if (result_1 == (void *) NULL) {
		clnt_perror (clnt, "call failed");
		return;
	}
	result_2 = mountproc_mnt_1(&mountproc_mnt_1_arg, clnt);
	if (result_2 == (fhstatus *) NULL) {
		clnt_perror (clnt, "call failed");
		return;
	}
	if ( rescheck( "mnt", result_2->fhs_status ) ) {
		return;
	}
	result_3 = mountproc_dump_1((void*)&mountproc_dump_1_arg, clnt);
	if (result_3 == (mountlist *) NULL) {
		clnt_perror (clnt, "call failed");
		return;
	}
	{
	mountlist ml;
		for ( ml = *result_3; ml; ml = ml->ml_next ) {
			printf("%s:%s\n", ml->ml_hostname, ml->ml_directory);
		}
		xdr_free( (xdrproc_t)xdr_mountlist, (caddr_t)result_3 );
	}
	result_4 = mountproc_umnt_1(&mountproc_umnt_1_arg, clnt);
	if (result_4 == (void *) NULL) {
		clnt_perror (clnt, "call failed");
		return;
	}
	result_5 = mountproc_umntall_1((void*)&mountproc_umntall_1_arg, clnt);
	if (result_5 == (void *) NULL) {
		clnt_perror (clnt, "call failed");
		return;
	}
	result_6 = mountproc_export_1((void*)&mountproc_export_1_arg, clnt);
	if (result_6 == (exports *) NULL) {
		clnt_perror (clnt, "call failed");
		return;
	}
	pex( *result_6 );
	result_7 = mountproc_exportall_1((void*)&mountproc_exportall_1_arg, clnt);
	if (result_7 == (exports *) NULL) {
		clnt_perror (clnt, "call failed");
		return;
	}
	pex( *result_7 );
#ifndef	DEBUG
	clnt_destroy (clnt);
#endif	 /* DEBUG */
}


int
main (int argc, char *argv[])
{
	char *host, *expo;

	if (argc < 3) {
		printf ("usage: %s server_host export\n", argv[0]);
		exit (1);
	}
	host = argv[1];
	expo = argv[2];
	mountprog_1 (host, expo);
exit (0);
}

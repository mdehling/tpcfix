/*
 * TPCFIX.C - Convert TPC file from 512b fixed to variable record size.
 *
 * Copying a TPC file from a UNIX system to VMS will usually leave you with
 * a file of 512b fixed record size.  This program can fix such a file and
 * convert it to variable record size so it can be written to tape with
 * VMSTPC.
 *
 * To build TPCFIX, run the BUILD.COM command procedure as follows:
 *
 *     $ @BUILD
 *     Compiling...
 *     Linking...
 *
 * Next, define a foreign command for TPCFIX:
 *
 *     $ TPCFIX :== $$DISK:[LOCATION]TPCFIX.EXE
 *
 * You may now run TPCFIX as follows:
 *
 *     $ TPCFIX input.tpc output.tpc
 *
 * Given a 512b fixed record size file `input.tpc`, this should produce
 * a file `output.tpc` of variable record size.
 *
 * TPCFIX was compiled and tested on a VAXstation 2000 with TK50Z tape drive
 * running MicroVMS v4.7 with VAX-C v2.3.
 */

#include fab
#include rab
#include rmsdef
#include ssdef
#include stdio


/* The maximum record size according to the RMS Reference Manual AA-Z503B-TE
 * is 32767b.  We need to be able to read 32767b in 512b blocks, so a buffer
 * of at least 64*512b = 32768b.
 */
#define BUFLEN		32768

/* TPC file structure contains 2b length in little endian. */
struct record {
	unsigned short int length;
	char data[];
};


int main(int argc, char **argv)
{
	int status;

	struct FAB fab_in  = cc$rms_fab,
	           fab_out = cc$rms_fab;
	struct RAB rab_in  = cc$rms_rab,
	           rab_out = cc$rms_rab;

	char buffer[BUFLEN];
	struct record *rec = &buffer;
	int n = 0;

	int eof = 1, file_no = 0, file_bs;

	if (argc != 3) {
		printf("Usage: %s <input.tpc> <output.tpc>\n", argv[0]);
		sys$exit(SS$_ABORT);
	};

	fab_in.fab$b_fac = FAB$M_GET;
	fab_in.fab$l_fna = argv[1];
	fab_in.fab$b_fns = strlen(argv[1]);

	if ( (status = sys$open(&fab_in)) != RMS$_NORMAL ) {
		printf("SYS$OPEN failed for input file '%s'.\n", argv[1]);
		sys$exit(status);
	};

	if ( (fab_in.fab$b_rfm != FAB$C_FIX) || (fab_in.fab$w_mrs != 512) ) {
		printf("Expected input file of fixed 512b record size.\n");
		status = SS$_ABORT;
		goto close_input;
	};

	fab_out.fab$b_fac = FAB$M_PUT;
	fab_out.fab$l_fna = argv[2];
	fab_out.fab$b_fns = strlen(argv[2]);
	fab_out.fab$b_rfm = FAB$C_VAR;

	if ( (status = sys$create(&fab_out)) != RMS$_NORMAL ) {
		printf("SYS$CREATE failed for output file '%s'.\n", argv[2]);
		goto close_input;
	};

	rab_in.rab$l_fab = &fab_in;

	if ( (status = sys$connect(&rab_in)) != RMS$_NORMAL ) {
		printf("SYS$CONNECT failed for input file '%s'.\n", argv[1]);
		goto close_both;
	};

	rab_out.rab$l_fab = &fab_out;

	if ( (status = sys$connect(&rab_out)) != RMS$_NORMAL ) {
		printf("SYS$CONNECT failed for output file '%s'.\n", argv[2]);
		goto disconnect_input;
	};

	while (1) {

		while (n < rec->length+2) {
			rab_in.rab$l_ubf = &buffer[n];
			rab_in.rab$w_usz = sizeof(buffer) - n;

			status = sys$get(&rab_in);

			if (status == RMS$_EOF) {
				printf("Unexpected End of File.\n");
				status = SS$_ABORT;
				goto disconnect_both;
			} else if ( status != RMS$_NORMAL ) {
				printf("SYS$GET failed.\n");
				status = SS$_ABORT;
				goto disconnect_both;
			};

			n += rab_in.rab$w_rsz;
		};

		if (rec->length > 0) {
			if (eof >= 1) {
				/* first block in file */
				file_no++;
				file_bs = rec->length;
				printf("File %d of block size 0x%04x",
					file_no, file_bs);
			} else if (rec->length != file_bs) {
				printf("!\nBogus block of size 0x%04x.\n",
					rec->length);
				status = SS$_ABORT;
				break;
			};
			putchar('.');
			eof = 0;
		} else {
			/* record of length 0 means EOF */
			eof++;
			if (eof >= 2) {
				printf("EOF\n");
				status = SS$_NORMAL;
				break;
			};
			printf(" EOF\n");
		};

		rab_out.rab$l_rbf = &rec->data;
		rab_out.rab$w_rsz = rec->length;

		if ( (status = sys$put(&rab_out)) != RMS$_NORMAL ) {
			printf("\nSYS$PUT failed.\n");
			status = SS$_ABORT;
			goto disconnect_both;
		};

		n -= rec->length + 2;
		memmove(&buffer, &rec->data[rec->length], n);
	};

disconnect_both:
	sys$disconnect(&rab_out);

disconnect_input:
	sys$disconnect(&rab_in);

close_both:
	sys$close(&fab_out);

close_input:
	sys$close(&fab_in);

	sys$exit(status);
}

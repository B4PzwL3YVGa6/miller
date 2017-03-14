#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "lib/mlr_globals.h"
#include "lib/mlr_arch.h"
#include "lib/mlrutil.h"

// ----------------------------------------------------------------
static int aux_list_main(int argc, char** argv);
static int    lecat_main(int argc, char** argv);
static int  termcvt_main(int argc, char** argv);
static int      hex_main(int argc, char** argv);
static int    unhex_main(int argc, char** argv);

static int lecat_stream(FILE* input_stream, int do_color);
static void hex_dump_fp(FILE *in_fp, FILE *out_fp, int do_raw);
static void    unhex_fp(FILE *in_fp, FILE *out_fp);

static void aux_list_usage(char* argv0, char* argv1, FILE* o, int exit_code);
static void    lecat_usage(char* argv0, char* argv1, FILE* o, int exit_code);
static void  termcvt_usage(char* argv0, char* argv1, FILE* o, int exit_code);
static void      hex_usage(char* argv0, char* argv1, FILE* o, int exit_code);
static void    unhex_usage(char* argv0, char* argv1, FILE* o, int exit_code);

// ----------------------------------------------------------------
typedef int aux_main_t(int argc, char**argv);
typedef void aux_usage_t( char* argv0, char* argv1, FILE* o, int exit_code);
typedef struct _aux_lookup_entry_t {
	char* name;
	aux_main_t* pmain;
	aux_usage_t* pusage;
} aux_lookup_entry_t;

static aux_lookup_entry_t aux_lookup_table[] = {

	{ "aux-list", aux_list_main, aux_list_usage },
	{ "lecat",    lecat_main,    lecat_usage    },
	{ "termcvt",  termcvt_main,  termcvt_usage  },
	{ "hex",      hex_main,      hex_usage      },
	{ "unhex",    unhex_main,    unhex_usage    },

};

static int aux_lookup_table_size = sizeof(aux_lookup_table) / sizeof(aux_lookup_table[0]);

// ================================================================
void do_aux_entries(int argc, char** argv) {
	if (argc < 2) {
		return;
	}

	for (int i = 0; i < aux_lookup_table_size; i++) {
		if (streq(argv[1], aux_lookup_table[i].name)) {
			exit(aux_lookup_table[i].pmain(argc, argv));
		}
	}
	// else return to mlrmain for the rest of Miller.
}

// ----------------------------------------------------------------
static void aux_list_usage(char* argv0, char* argv1, FILE* o, int exit_code) {
	fprintf(o, "Usage: %s %s [options]\n", argv0, argv1);
	fprintf(o, "Options:\n");
	fprintf(o, "-h or --help: print this message\n");
	exit(exit_code);
}

static int aux_list_main(int argc, char** argv) {
	printf("Available subcommands:\n");
	for (int i = 0; i < aux_lookup_table_size; i++) {
		printf("  %s\n", aux_lookup_table[i].name);
	}
	printf("For more information, please invoke %s {subcommand} --help\n", MLR_GLOBALS.bargv0);
	return 0;
}

// ----------------------------------------------------------------
static void lecat_usage(char* argv0, char* argv1, FILE* o, int exit_code) {
	fprintf(o, "Usage: %s %s [options] {zero or more file names}\n", argv0, argv1);
	fprintf(o, "Simply echoes input, but flags CR characters in red and LF characters in green.\n");
	fprintf(o, "If zero file names are supplied, standard input is read.\n");
	fprintf(o, "Options:\n");
	fprintf(o, "-h or --help: print this message\n");
	exit(exit_code);
}

static int lecat_main(int argc, char** argv) {
	int ok = 1;
	int do_color = TRUE;

	if (argc >= 3) {
		if (streq(argv[2], "-h") || streq(argv[2], "--help")) {
			lecat_usage(argv[0], argv[1], stdout, 0);
		}
	}

	// 'mlr' and 'lecat' are already argv[0] and argv[1].
	int argb = 2;
	if (argc >= 3 && argv[argb][0] == '-') {
		if (streq(argv[argb], "--mono")) {
			do_color = FALSE;
			argb++;
		} else {
			fprintf(stderr, "%s %s: unrecognized option \"%s\".\n",
				argv[0], argv[1], argv[argb]);
			return 1;
		}
	}

	if (argb == argc) {
		ok = ok && lecat_stream(stdin, do_color);
	} else {
		for (int argi = argb; argi < argc; argi++) {
			char* file_name = argv[argi];
			FILE* input_stream = fopen(file_name, "r");
			if (input_stream == NULL) {
				perror(file_name);
				exit(1);
			}
			ok = lecat_stream(input_stream, do_color);
			fclose(input_stream);
		}
	}
	return ok ? 0 : 1;
}

static int lecat_stream(FILE* input_stream, int do_color) {
	while (1) {
		int c = fgetc(input_stream);
		if (c == EOF)
			break;
		if (c == '\r') {
			if (do_color)
				printf("\033[31;01m"); // xterm red
			printf("[CR]");
			if (do_color)
				printf("\033[0m");
		} else if (c == '\n') {
			if (do_color)
				printf("\033[32;01m"); // xterm green
			printf("[LF]\n");
			if (do_color)
				printf("\033[0m");
		} else {
			putchar(c);
		}
	}
	return 1;
}

// ================================================================
typedef void line_cvt_func_t(char* line, ssize_t linelen, FILE* output_stream);

static void cr_to_crlf(char* line,  ssize_t linelen, FILE* output_stream) {
	if (linelen == 1) {
		if (line[0] == '\r') {
			fputc('\r', output_stream);
			fputc('\n', output_stream);
		} else {
			fputc(line[0], output_stream);
		}
	} else {
		if (line[linelen-2] == '\r' && line[linelen-1] == '\n') {
			fputs(line, output_stream);
		} else if (line[linelen-1] == '\r') {
			fputs(line, output_stream);
			fputc('\n', output_stream);
		} else {
			fputs(line, output_stream);
		}
	}
}

static void lf_to_crlf(char* line,  ssize_t linelen, FILE* output_stream) {
	if (linelen == 1) {
		if (line[0] == '\n') {
			fputc('\r', output_stream);
			fputc('\n', output_stream);
		} else {
			fputc(line[0], output_stream);
		}
	} else {
		if (line[linelen-2] == '\r' && line[linelen-1] == '\n') {
			fputs(line, output_stream);
		} else if (line[linelen-1] == '\n') {
			line[linelen-1] = '\r';
			fputs(line, output_stream);
			fputc('\n', output_stream);
		} else {
			fputs(line, output_stream);
		}
	}
}

static void crlf_to_cr(char* line,  ssize_t linelen, FILE* output_stream) {
	if (linelen >= 2 && line[linelen-2] == '\r' && line[linelen-1] == '\n') {
		line[linelen-2] = '\r';
		line[linelen-1] = '\0';
	}
	fputs(line, output_stream);
}

static void crlf_to_lf(char* line,  ssize_t linelen, FILE* output_stream) {
	if (linelen >= 2 && line[linelen-2] == '\r' && line[linelen-1] == '\n') {
		line[linelen-2] = '\n';
		line[linelen-1] = '\0';
	}
	fputs(line, output_stream);
}

static void cr_to_lf(char* line,  ssize_t linelen, FILE* output_stream) {
	if (linelen >= 1 && line[linelen-1] == '\r') {
		line[linelen-1] = '\n';
	}
	fputs(line, output_stream);
}

static void lf_to_cr(char* line,  ssize_t linelen, FILE* output_stream) {
	if (linelen >= 1 && line[linelen-1] == '\n') {
		line[linelen-1] = '\r';
	}
	fputs(line, output_stream);
}

// ----------------------------------------------------------------
static int termcvt_stream(FILE* input_stream, FILE* output_stream, char inend, line_cvt_func_t* pcvt_func) {
	while (1) {
		char* line = NULL;
		size_t linecap = 0;
		ssize_t linelen = mlr_arch_getdelim(&line, &linecap, inend, input_stream);
		if (linelen <= 0) {
			break;
		}

		pcvt_func(line, linelen, output_stream);

		free(line);
	}
	return 1;
}

// ----------------------------------------------------------------
static void termcvt_usage(char* argv0, char* argv1, FILE* o, int exit_code) {
	fprintf(o, "Usage: %s %s [option] {zero or more file names}\n", argv0, argv1);
	fprintf(o, "Option (exactly one is required):\n");
	fprintf(o, "--cr2crlf\n");
	fprintf(o, "--lf2crlf\n");
	fprintf(o, "--crlf2cr\n");
	fprintf(o, "--crlf2lf\n");
	fprintf(o, "--cr2lf\n");
	fprintf(o, "--lf2cr\n");
	fprintf(o, "-I in-place processing (default is to write to stdout)\n");
	fprintf(o, "-h or --help: print this message\n");
	fprintf(o, "Zero file names means read from standard input.\n");
	fprintf(o, "Output is always to standard output; files are not written in-place.\n");
	exit(exit_code);
}

// ----------------------------------------------------------------
static int termcvt_main(int argc, char** argv) {
	int ok = 1;
	char inend = '\n';
	int do_in_place = FALSE;
	line_cvt_func_t* pcvt_func = lf_to_crlf;

	// argv[0] is 'mlr'
	// argv[1] is 'termcvt'
	// argv[2] is '--some-option'
	// argv[3] and above are filenames
	if (argc < 2)
		termcvt_usage(argv[0], argv[1], stderr, 1);
	int argi;
	for (argi = 2; argi < argc; argi++) {
		char* opt = argv[argi];

		if (opt[0] != '-')
			break;

		if (streq(opt, "-h") || streq(opt, "--help")) {
			termcvt_usage(argv[0], argv[1], stdout, 0);
		} else if (streq(opt, "-I")) {
			do_in_place = TRUE;
		} else if (streq(opt, "--cr2crlf")) {
			pcvt_func = cr_to_crlf;
			inend = '\r';
		} else if (streq(opt, "--lf2crlf")) {
			pcvt_func = lf_to_crlf;
			inend = '\n';
		} else if (streq(opt, "--crlf2cr")) {
			pcvt_func = crlf_to_cr;
			inend = '\n';
		} else if (streq(opt, "--crlf2lf")) {
			pcvt_func = crlf_to_lf;
			inend = '\n';
		} else if (streq(opt, "--cr2lf")) {
			pcvt_func = cr_to_lf;
			inend = '\r';
		} else if (streq(opt, "--lf2cr")) {
			pcvt_func = lf_to_cr;
			inend = '\n';
		} else {
			termcvt_usage(argv[0], argv[1], stdout, 0);
		}
	}

	int nfiles = argc - argi;
	if (nfiles == 0) {
		ok = ok && termcvt_stream(stdin, stdout, inend, pcvt_func);

	} else if (do_in_place) {
		for (; argi < argc; argi++) {
			char* file_name = argv[argi];
			char* temp_name = alloc_suffixed_temp_file_name(file_name);
			FILE* input_stream = fopen(file_name, "r");
			FILE* output_stream = fopen(temp_name, "wb");

			if (input_stream == NULL) {
				perror("fopen");
				fprintf(stderr, "%s: Could not open \"%s\" for read.\n",
					MLR_GLOBALS.bargv0, file_name);
				exit(1);
			}
			if (output_stream == NULL) {
				perror("fopen");
				fprintf(stderr, "%s: Could not open \"%s\" for write.\n",
					MLR_GLOBALS.bargv0, temp_name);
				exit(1);
			}

			ok = termcvt_stream(input_stream, output_stream, inend, pcvt_func);

			fclose(input_stream);
			fclose(output_stream);

			int rc = rename(temp_name, file_name);
			if (rc != 0) {
				perror("rename");
				fprintf(stderr, "%s: Could not rename \"%s\" to \"%s\".\n",
					MLR_GLOBALS.bargv0, temp_name, file_name);
				exit(1);
			}
			free(temp_name);
		}

	} else {
		for (; argi < argc; argi++) {
			char* file_name = argv[argi];
			FILE* input_stream = fopen(file_name, "r");
			if (input_stream == NULL) {
				perror(file_name);
				exit(1);
			}
			ok = termcvt_stream(input_stream, stdout, inend, pcvt_func);
			fclose(input_stream);
		}

	}
	return ok ? 0 : 1;
}

// ================================================================
// Copyright (c) 1998 John Kerl.
// ================================================================
// This is a simple hex dump with hex offsets to the left, hex data in the
// middle, and ASCII at the right.  This is a subset of the functionality of
// Unix od; I wrote it in my NT days.
//
// Example:
//
// $ d2h $(jot 0 128) | unhex | hex
// 00000000: 00 01 02 03  04 05 06 07  08 09 0a 0b  0c 0d 0e 0f |................|
// 00000010: 10 11 12 13  14 15 16 17  18 19 1a 1b  1c 1d 1e 1f |................|
// 00000020: 20 21 22 23  24 25 26 27  28 29 2a 2b  2c 2d 2e 2f | !"#$%&'()*+,-./|
// 00000030: 30 31 32 33  34 35 36 37  38 39 3a 3b  3c 3d 3e 3f |0123456789:;<=>?|
// 00000040: 40 41 42 43  44 45 46 47  48 49 4a 4b  4c 4d 4e 4f |@ABCDEFGHIJKLMNO|
// 00000050: 50 51 52 53  54 55 56 57  58 59 5a 5b  5c 5d 5e 5f |PQRSTUVWXYZ[\]^_|
// 00000060: 60 61 62 63  64 65 66 67  68 69 6a 6b  6c 6d 6e 6f |`abcdefghijklmno|
// 00000070: 70 71 72 73  74 75 76 77  78 79 7a 7b  7c 7d 7e 7f |pqrstuvwxyz{|}~.|
// ================================================================

#define LINE_LENGTH_MAX 8192

// ----------------------------------------------------------------
static void hex_usage(char* argv0, char* argv1, FILE* o, int exit_code) {
	fprintf(o, "Usage: %s %s [options] {zero or more file names}\n", argv0, argv1);
	fprintf(o, "Simple hex-dump.\n");
	fprintf(o, "If zero file names are supplied, standard input is read.\n");
	fprintf(o, "Options:\n");
	fprintf(o, "-r: print only raw hex without leading offset indicators or trailing ASCII dump.\n");
	fprintf(o, "-h or --help: print this message\n");
	exit(exit_code);
}

//----------------------------------------------------------------------
// 'mlr' and 'hex' are already argv[0] and argv[1].
static int hex_main(int argc, char **argv) {
	char * filename;
	FILE * in_fp;
	FILE * out_fp;
	int do_raw = 0;
	int argi = 2;

	if (argc >= 3) {
		if (streq(argv[2], "-r")) {
			do_raw = 1;
			argi++;
		} else if (streq(argv[2], "-h") || streq(argv[2], "--help")) {
			hex_usage(argv[0], argv[1], stdout, 0);
		}
	}

	int num_file_names = argc - argi;

	if (num_file_names == 0) {
#ifdef WINDOWS
		setmode(fileno(stdin), O_BINARY);
#endif //WINDOWS
		hex_dump_fp(stdin, stdout, do_raw);
	} else {
		for ( ; argi < argc; argi++) {
			if (!do_raw) {
				if (num_file_names > 1)
					printf("%s:\n", argv[argi]);
			}
			filename = argv[argi];
			in_fp    = fopen(filename, "rb");
			out_fp   = stdout;
			if (in_fp == NULL) {
				fprintf(stderr, "Couldn't open \"%s\"; skipping.\n",
					filename);
			}
			else {
				hex_dump_fp(in_fp, out_fp, do_raw);
				fclose(in_fp);
				if (out_fp != stdout)
					fclose(out_fp);

			}
			if (!do_raw) {
				if (num_file_names > 1)
					printf("\n");
			}
		}
	}

	return 0;
}

// ----------------------------------------------------------------
static void hex_dump_fp(FILE *in_fp, FILE *out_fp, int do_raw) {
	const int bytes_per_clump = 4;
	const int clumps_per_line = 4;
	const int buffer_size = bytes_per_clump * clumps_per_line;
	unsigned char buf[buffer_size];
	long num_bytes_read;
	long num_bytes_total = 0;
	int byteno;

	while ((num_bytes_read=fread(buf, sizeof(unsigned char),
		buffer_size, in_fp)) > 0)
	{
		if (!do_raw) {
			printf("%08lx: ", num_bytes_total);
		}

		for (byteno = 0; byteno < num_bytes_read; byteno++) {
			unsigned int temp = buf[byteno];
			printf("%02x ", temp);
			if ((byteno % bytes_per_clump) ==
			(bytes_per_clump - 1))
			{
				if ((byteno > 0) && (byteno < buffer_size-1))
					printf(" ");
			}
		}
		for (byteno = num_bytes_read; byteno < buffer_size; byteno++) {
			printf("   ");
			if ((byteno % bytes_per_clump) ==
			(bytes_per_clump - 1))
			{
				if ((byteno > 0) && (byteno < buffer_size-1))
					printf(" ");
			}
		}

		if (!do_raw) {
			printf("|");
			for (byteno = 0; byteno < num_bytes_read; byteno++) {
				unsigned char temp = buf[byteno];
				if (!isprint(temp))
					temp = '.';
				printf("%c", temp);
			}

			printf("|");
		}

		printf("\n");
		num_bytes_total += num_bytes_read;
	}
}

// ----------------------------------------------------------------
static void unhex_usage(char* argv0, char* argv1, FILE* o, int exit_code) {
	fprintf(o, "Usage: %s %s [option] {zero or more file names}\n", argv0, argv1);
	fprintf(o, "Options:\n");
	fprintf(o, "-h or --help: print this message\n");
	fprintf(o, "Zero file names means read from standard input.\n");
	fprintf(o, "Output is always to standard output; files are not written in-place.\n");
	exit(exit_code);
}

// ----------------------------------------------------------------
int unhex_main(int argc, char ** argv) {
	// 'mlr' and 'unhex' are already argv[0] and argv[1].
	if (argc >= 3) {
		if (streq(argv[2], "-h") || streq(argv[2], "--help")) {
			unhex_usage(argv[0], argv[1], stdout, 0);
		}
	}

	int exit_code = 0;
	if (argc == 2) {
		unhex_fp(stdin, stdout);
	} else {
		for (int argi = 2; argi < argc; argi++) {
			char* filename = argv[argi];
			FILE* infp = fopen(filename, "rb");
			if (infp == NULL) {
				fprintf(stderr, "%s %s: Couldn't open \"%s\"; skipping.\n",
					argv[0], argv[1], filename);
				exit_code = 1;
			} else {
				unhex_fp(infp, stdout);
				fclose(infp);
			}
		}
	}

	return exit_code;
}

// ----------------------------------------------------------------
static void unhex_fp(FILE *infp, FILE *outfp) {
	unsigned char byte;
	unsigned temp;
	int count;
	while ((count=fscanf(infp, "%x", &temp)) > 0) {
		byte = temp;
		fwrite (&byte, sizeof(byte), 1, outfp);
	}
}

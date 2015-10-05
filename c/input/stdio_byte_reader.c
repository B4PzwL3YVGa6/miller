#include <stdio.h>
#include <string.h>
#include "input/byte_readers.h"
#include "lib/mlr_globals.h"
#include "lib/mlrutil.h"

typedef struct _stdio_byte_reader_state_t {
	char* filename;
	FILE* fp;
} stdio_byte_reader_state_t;

static int stdio_byte_reader_open_func(struct _byte_reader_t* pbr, char* filename);
static int stdio_byte_reader_read_func(struct _byte_reader_t* pbr);
static void stdio_byte_reader_close_func(struct _byte_reader_t* pbr);

// ----------------------------------------------------------------
byte_reader_t* stdio_byte_reader_alloc() {
	byte_reader_t* pbr = mlr_malloc_or_die(sizeof(byte_reader_t));

	pbr->pvstate     = NULL;
	pbr->popen_func  = stdio_byte_reader_open_func;
	pbr->pread_func  = stdio_byte_reader_read_func;
	pbr->pclose_func = stdio_byte_reader_close_func;

	return pbr;
}

void stdio_byte_reader_free(byte_reader_t* pbr) {
	stdio_byte_reader_state_t* pstate = pbr->pvstate;
	if (pstate != NULL) {
		free(pstate->filename); // null-ok semantics
	}
	free(pbr);
}

// ----------------------------------------------------------------
static int stdio_byte_reader_open_func(struct _byte_reader_t* pbr, char* filename) {
	stdio_byte_reader_state_t* pstate = mlr_malloc_or_die(sizeof(stdio_byte_reader_state_t));
	pstate->filename = mlr_strdup_or_die(filename);
	if (streq(pstate->filename, "-")) {
		pstate->fp = stdin;
	} else {
		pstate->fp = fopen(filename, "r");
		if (pstate->fp == NULL) {
			perror("fopen");
			fprintf(stderr, "%s: Couldn't fopen \"%s\" for read.\n", MLR_GLOBALS.argv0, filename);
			exit(1);
		}
	}
	pbr->pvstate = pstate;
	return TRUE;
}

static int stdio_byte_reader_read_func(struct _byte_reader_t* pbr) {
	stdio_byte_reader_state_t* pstate = pbr->pvstate;
	int c = getc_unlocked(pstate->fp);
	if (c == EOF && ferror(pstate->fp)) {
		perror("fread");
		fprintf(stderr, "%s: Read error on file \"%s\".\n", MLR_GLOBALS.argv0, pstate->filename);
		exit(1);
	}
	return c;
}

static void stdio_byte_reader_close_func(struct _byte_reader_t* pbr) {
	stdio_byte_reader_state_t* pstate = pbr->pvstate;
	if (pstate->fp != stdin)
		fclose(pstate->fp);
	free(pstate);
	pbr->pvstate = NULL;
}

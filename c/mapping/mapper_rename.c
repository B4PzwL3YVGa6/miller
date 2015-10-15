#include "lib/mlrutil.h"
#include "containers/lhmss.h"
#include "containers/sllv.h"
#include "mapping/mappers.h"
#include "cli/argparse.h"

typedef struct _mapper_rename_state_t {
	lhmss_t* pold_to_new;
} mapper_rename_state_t;

static sllv_t*   mapper_rename_process(lrec_t* pinrec, context_t* pctx, void* pvstate);
static sllv_t*   mapper_rename_regex_process(lrec_t* pinrec, context_t* pctx, void* pvstate);
static void      mapper_rename_free(void* pvstate);
static mapper_t* mapper_rename_alloc(lhmss_t* pold_to_new, int do_regexes);
static void      mapper_rename_usage(FILE* o, char* argv0, char* verb);
static mapper_t* mapper_rename_parse_cli(int* pargi, int argc, char** argv);

// ----------------------------------------------------------------
mapper_setup_t mapper_rename_setup = {
	.verb = "rename",
	.pusage_func = mapper_rename_usage,
	.pparse_func = mapper_rename_parse_cli
};

// ----------------------------------------------------------------
static void mapper_rename_usage(FILE* o, char* argv0, char* verb) {
	fprintf(o, "Usage: %s %s {old1,new1,old2,new2,...}\n", argv0, verb);
	fprintf(o, "Renames specified fields.\n");
}

static mapper_t* mapper_rename_parse_cli(int* pargi, int argc, char** argv) {
	int do_regexes = FALSE;

	char* verb = argv[(*pargi)++];

	ap_state_t* pstate = ap_alloc();
	ap_define_true_flag(pstate, "-r", &do_regexes);

	if (!ap_parse(pstate, verb, pargi, argc, argv)) {
		mapper_rename_usage(stderr, argv[0], verb);
		return NULL;
	}

	if ((argc - *pargi) < 1) {
		mapper_rename_usage(stderr, argv[0], verb);
		return NULL;
	}

	slls_t* pnames = slls_from_line(argv[*pargi], ',', FALSE);
	if ((pnames->length % 2) != 0) {
		mapper_rename_usage(stderr, argv[0], verb);
		return NULL;
	}
	lhmss_t* pold_to_new = lhmss_alloc();
	for (sllse_t* pe = pnames->phead; pe != NULL; pe = pe->pnext->pnext) {
		lhmss_put(pold_to_new, pe->value, pe->pnext->value);
	}

	*pargi += 1;
	return mapper_rename_alloc(pold_to_new, do_regexes);
}

// ----------------------------------------------------------------
static mapper_t* mapper_rename_alloc(lhmss_t* pold_to_new, int do_regexes) {
	mapper_t* pmapper = mlr_malloc_or_die(sizeof(mapper_t));

	mapper_rename_state_t* pstate = mlr_malloc_or_die(sizeof(mapper_rename_state_t));
	pstate->pold_to_new = pold_to_new;

	pmapper->pvstate = (void*)pstate;
	if (do_regexes)
		pmapper->pprocess_func = mapper_rename_regex_process;
	else
		pmapper->pprocess_func = mapper_rename_process;
	pmapper->pfree_func = mapper_rename_free;

	return pmapper;
}

static void mapper_rename_free(void* pvstate) {
	mapper_rename_state_t* pstate = (mapper_rename_state_t*)pvstate;
	lhmss_free(pstate->pold_to_new);
}

// ----------------------------------------------------------------
static sllv_t* mapper_rename_process(lrec_t* pinrec, context_t* pctx, void* pvstate) {
	if (pinrec != NULL) {
		mapper_rename_state_t* pstate = (mapper_rename_state_t*)pvstate;
		for (lhmsse_t* pe = pstate->pold_to_new->phead; pe != NULL; pe = pe->pnext) {
			char* old_name = pe->key;
			char* new_name = pe->value;
			char* value = lrec_get(pinrec, old_name);
			if (value != NULL) {
				lrec_rename(pinrec, old_name, new_name);
			}
		}
		return sllv_single(pinrec);
	}
	else {
		return sllv_single(NULL);
	}
}

static sllv_t* mapper_rename_regex_process(lrec_t* pinrec, context_t* pctx, void* pvstate) {
	if (pinrec != NULL) {
		mapper_rename_state_t* pstate = (mapper_rename_state_t*)pvstate;
		for (lhmsse_t* pe = pstate->pold_to_new->phead; pe != NULL; pe = pe->pnext) {
			char* old_name = pe->key;
			char* new_name = pe->value;
			char* value = lrec_get(pinrec, old_name);
			if (value != NULL) {
				lrec_rename(pinrec, old_name, new_name);
			}
		}
		return sllv_single(pinrec);
	}
	else {
		return sllv_single(NULL);
	}
}

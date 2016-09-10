#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "lib/mlrutil.h"
#include "containers/sllv.h"
#include "containers/lhmslv.h"
#include "containers/lhmsv.h"
#include "containers/mixutil.h"
#include "mapping/mappers.h"
#include "cli/argparse.h"

typedef struct _mapper_uniq_state_t {
	ap_state_t* pargp;
	slls_t* pgroup_by_field_names;
	int show_counts;
	int show_num_distinct_only;
	lhmslv_t* pcounts_by_group;
} mapper_uniq_state_t;

static void      mapper_uniq_usage(FILE* o, char* argv0, char* verb);
static mapper_t* mapper_uniq_parse_cli(int* pargi, int argc, char** argv,
	cli_reader_opts_t* _, cli_writer_opts_t* __);
static void      mapper_count_distinct_usage(FILE* o, char* argv0, char* verb);
static mapper_t* mapper_count_distinct_parse_cli(int* pargi, int argc, char** argv,
	cli_reader_opts_t* _, cli_writer_opts_t* __);
static mapper_t* mapper_uniq_alloc(ap_state_t* pargp, slls_t* pgroup_by_field_names,
	int show_counts, int show_num_distinct_only);
static void      mapper_uniq_free(mapper_t* pmapper);

static sllv_t* mapper_uniq_process_num_distinct_only(lrec_t* pinrec, context_t* pctx, void* pvstate);
static sllv_t* mapper_uniq_process_with_counts(lrec_t* pinrec, context_t* pctx, void* pvstate);
static sllv_t* mapper_uniq_process_no_counts(lrec_t* pinrec, context_t* pctx, void* pvstate);

// ----------------------------------------------------------------
mapper_setup_t mapper_count_distinct_setup = {
	.verb = "count-distinct",
	.pusage_func = mapper_count_distinct_usage,
	.pparse_func = mapper_count_distinct_parse_cli,
	.ignores_input = FALSE,
};

mapper_setup_t mapper_uniq_setup = {
	.verb = "uniq",
	.pusage_func = mapper_uniq_usage,
	.pparse_func = mapper_uniq_parse_cli,
	.ignores_input = FALSE,
};

// ----------------------------------------------------------------
static void mapper_count_distinct_usage(FILE* o, char* argv0, char* verb) {
	fprintf(o, "Usage: %s %s [options]\n", argv0, verb);
	fprintf(o, "-f {a,b,c}    Field names for distinct count.\n");
	fprintf(o, "-n            Show only the number of distinct values.\n");
	fprintf(o, "Prints number of records having distinct values for specified field names.\n");
	fprintf(o, "Same as uniq -c.\n");
}

// ----------------------------------------------------------------
static mapper_t* mapper_count_distinct_parse_cli(int* pargi, int argc, char** argv,
	cli_reader_opts_t* _, cli_writer_opts_t* __)
{
	slls_t* pfield_names = NULL;
	int     show_num_distinct_only = FALSE;

	char* verb = argv[(*pargi)++];

	ap_state_t* pstate = ap_alloc();
	ap_define_string_list_flag(pstate, "-f", &pfield_names);
	ap_define_true_flag(pstate,        "-n", &show_num_distinct_only);

	if (!ap_parse(pstate, verb, pargi, argc, argv)) {
		mapper_count_distinct_usage(stderr, argv[0], verb);
		return NULL;
	}

	if (pfield_names == NULL) {
		mapper_count_distinct_usage(stderr, argv[0], verb);
		return NULL;
	}

	return mapper_uniq_alloc(pstate, pfield_names, TRUE, show_num_distinct_only);
}

// ----------------------------------------------------------------
static void mapper_uniq_usage(FILE* o, char* argv0, char* verb) {
	fprintf(o, "Usage: %s %s [options]\n", argv0, verb);
	fprintf(o, "-g {d,e,f}    Group-by-field names for uniq counts.\n");
	fprintf(o, "-c            Show repeat counts in addition to unique values.\n");
	fprintf(o, "-n            Show only the number of distinct values.\n");
	fprintf(o, "Prints distinct values for specified field names. With -c, same as\n");
	fprintf(o, "count-distinct. For uniq, -f is a synonym for -g.\n");
}

static mapper_t* mapper_uniq_parse_cli(int* pargi, int argc, char** argv,
	cli_reader_opts_t* _, cli_writer_opts_t* __)
{
	slls_t* pgroup_by_field_names = NULL;
	int     show_counts = FALSE;
	int     show_num_distinct_only = FALSE;

	char* verb = argv[(*pargi)++];

	ap_state_t* pstate = ap_alloc();
	ap_define_string_list_flag(pstate, "-f", &pgroup_by_field_names);
	ap_define_string_list_flag(pstate, "-g", &pgroup_by_field_names);
	ap_define_true_flag(pstate,        "-c", &show_counts);
	ap_define_true_flag(pstate,        "-n", &show_num_distinct_only);

	if (!ap_parse(pstate, verb, pargi, argc, argv)) {
		mapper_uniq_usage(stderr, argv[0], verb);
		return NULL;
	}

	if (pgroup_by_field_names == NULL) {
		mapper_uniq_usage(stderr, argv[0], verb);
		return NULL;
	}

	return mapper_uniq_alloc(pstate, pgroup_by_field_names, show_counts, show_num_distinct_only);
}

// ----------------------------------------------------------------
static mapper_t* mapper_uniq_alloc(ap_state_t* pargp, slls_t* pgroup_by_field_names,
	int show_counts, int show_num_distinct_only)
{
	mapper_t* pmapper = mlr_malloc_or_die(sizeof(mapper_t));

	mapper_uniq_state_t* pstate = mlr_malloc_or_die(sizeof(mapper_uniq_state_t));

	pstate->pargp                  = pargp;
	pstate->pgroup_by_field_names  = pgroup_by_field_names;
	pstate->show_counts            = show_counts;
	pstate->show_num_distinct_only = show_num_distinct_only;
	pstate->pcounts_by_group       = lhmslv_alloc();

	pmapper->pvstate = pstate;
	if (show_num_distinct_only)
		pmapper->pprocess_func = mapper_uniq_process_num_distinct_only;
	else if (show_counts)
		pmapper->pprocess_func = mapper_uniq_process_with_counts;
	else
		pmapper->pprocess_func = mapper_uniq_process_no_counts;
	pmapper->pfree_func = mapper_uniq_free;

	return pmapper;
}

static void mapper_uniq_free(mapper_t* pmapper) {
	mapper_uniq_state_t* pstate = pmapper->pvstate;
	slls_free(pstate->pgroup_by_field_names);
	// lhmslv_free will free the keys: we only need to free the void-star values.
	for (lhmslve_t* pa = pstate->pcounts_by_group->phead; pa != NULL; pa = pa->pnext) {
		unsigned long long* pcount = pa->pvvalue;
		free(pcount);
	}
	lhmslv_free(pstate->pcounts_by_group);
	pstate->pgroup_by_field_names = NULL;
	pstate->pcounts_by_group = NULL;
	ap_free(pstate->pargp);
	free(pstate);
	free(pmapper);
}

// ----------------------------------------------------------------
static sllv_t* mapper_uniq_process_num_distinct_only(lrec_t* pinrec, context_t* pctx, void* pvstate) {
	mapper_uniq_state_t* pstate = pvstate;
	if (pinrec != NULL) {
		slls_t* pgroup_by_field_values = mlr_reference_selected_values_from_record(pinrec, pstate->pgroup_by_field_names);
		if (pgroup_by_field_values != NULL) {
			unsigned long long* pcount = lhmslv_get(pstate->pcounts_by_group, pgroup_by_field_values);
			if (pcount == NULL) {
				pcount = mlr_malloc_or_die(sizeof(unsigned long long));
				*pcount = 1LL;
				lhmslv_put(pstate->pcounts_by_group, slls_copy(pgroup_by_field_values), pcount, FREE_ENTRY_KEY);
			} else {
				(*pcount)++;
			}
			slls_free(pgroup_by_field_values);
		}
		lrec_free(pinrec);
		return NULL;
	}
	else {
		sllv_t* poutrecs = sllv_alloc();

		lrec_t* poutrec = lrec_unbacked_alloc();
		int count = pstate->pcounts_by_group->num_occupied;
		lrec_put(poutrec, "count", mlr_alloc_string_from_int(count), FREE_ENTRY_VALUE);
		sllv_append(poutrecs, poutrec);

		sllv_append(poutrecs, NULL);
		return poutrecs;
	}
}

static sllv_t* mapper_uniq_process_with_counts(lrec_t* pinrec, context_t* pctx, void* pvstate) {
	mapper_uniq_state_t* pstate = pvstate;
	if (pinrec != NULL) {
		slls_t* pgroup_by_field_values = mlr_reference_selected_values_from_record(pinrec, pstate->pgroup_by_field_names);
		if (pgroup_by_field_values != NULL) {
			unsigned long long* pcount = lhmslv_get(pstate->pcounts_by_group, pgroup_by_field_values);
			if (pcount == NULL) {
				pcount = mlr_malloc_or_die(sizeof(unsigned long long));
				*pcount = 1LL;
				lhmslv_put(pstate->pcounts_by_group, slls_copy(pgroup_by_field_values), pcount, FREE_ENTRY_KEY);
			} else {
				(*pcount)++;
			}
			slls_free(pgroup_by_field_values);
		}
		lrec_free(pinrec);
		return NULL;
	} else {
		sllv_t* poutrecs = sllv_alloc();

		for (lhmslve_t* pa = pstate->pcounts_by_group->phead; pa != NULL; pa = pa->pnext) {
			lrec_t* poutrec = lrec_unbacked_alloc();

			slls_t* pgroup_by_field_values = pa->key;

			sllse_t* pb = pstate->pgroup_by_field_names->phead;
			sllse_t* pc =         pgroup_by_field_values->phead;
			for ( ; pb != NULL && pc != NULL; pb = pb->pnext, pc = pc->pnext) {
				lrec_put(poutrec, pb->value, pc->value, NO_FREE);
			}

			if (pstate->show_counts) {
				unsigned long long* pcount = pa->pvvalue;
				lrec_put(poutrec, "count", mlr_alloc_string_from_ull(*pcount), FREE_ENTRY_VALUE);
			}

			sllv_append(poutrecs, poutrec);
		}
		sllv_append(poutrecs, NULL);
		return poutrecs;
	}
}

static sllv_t* mapper_uniq_process_no_counts(lrec_t* pinrec, context_t* pctx, void* pvstate) {
	mapper_uniq_state_t* pstate = pvstate;
	if (pinrec == NULL) {
		return sllv_single(NULL);
	}

	slls_t* pgroup_by_field_values = mlr_reference_selected_values_from_record(pinrec, pstate->pgroup_by_field_names);
	if (pgroup_by_field_values == NULL) {
		lrec_free(pinrec);
		return NULL;
	}

	unsigned long long* pcount = lhmslv_get(pstate->pcounts_by_group, pgroup_by_field_values);
	if (pcount == NULL) {
		pcount = mlr_malloc_or_die(sizeof(unsigned long long));
		*pcount = 1LL;
		slls_t* pcopy = slls_copy(pgroup_by_field_values);
		lhmslv_put(pstate->pcounts_by_group, pcopy, pcount, FREE_ENTRY_KEY);

		lrec_t* poutrec = lrec_unbacked_alloc();

		sllse_t* pb = pstate->pgroup_by_field_names->phead;
		sllse_t* pc = pcopy->phead;
		for ( ; pb != NULL && pc != NULL; pb = pb->pnext, pc = pc->pnext) {
			lrec_put(poutrec, pb->value, pc->value, NO_FREE);
		}

		lrec_free(pinrec);
		slls_free(pgroup_by_field_values);
		return sllv_single(poutrec);
	} else {
		(*pcount)++;
		lrec_free(pinrec);
		slls_free(pgroup_by_field_values);
		return NULL;
	}
}

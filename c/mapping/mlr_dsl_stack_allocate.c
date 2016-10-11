#include <stdlib.h>
#include "lib/mlr_globals.h"
#include "lib/mlrutil.h"
#include "containers/free_flags.h"
#include "containers/lhmsi.h"
#include "mapping/mlr_dsl_blocked_ast.h"
#include "mapping/context_flags.h"

// ================================================================
// This is a two-pass stack allocator for the Miller DSL.
//
// ----------------------------------------------------------------
// CONTEXT:
//
// In the initial Miller implementation of local variables (e.g.  for-loop
// indices or frame-local variables), I used a hashmap from string name to
// mlrval at each level. This was very easy to code and it worked, but it had
// the property that every local-variable read or write involved a hashmap
// lookup to locate each local variable on the stack.  For compute-intensive
// work this resulted in 80% or more of the compute time being used for the
// hashmap accesses. It doesn't make sense to always be asking "where is
// variable 'a'"? at runtime (maybe ten million times) since this can be figured
// out ahead of time.
//
// The Miller DSL allows for recursive functions and subroutines, but within
// those, stack layout is knowable at parse time.
//
// ----------------------------------------------------------------
// EXAMPLE:
//
//                       # ---- FUNC FRAME: defcount 7 {absent-RHS,a,b,c,i,j,y}
//                       # To be noted below: absent-RHS is at slot 0 of top level.
// func f(a, b, c) {     # Args define locals 1,2,3 at current level.
//     local i = 24;     # Explicitly define local 4 at current level.
//     j = 25;           # Implicitly define local 5 at current level.
//                       #
//                       # ---- IF FRAME: defcount 1 {k}
//     if (a == 26) {    # Read local 1, up 1 level.
//         local k = 27; # Explicitly define local 0 at this level.
//         j = 28;       # LHS is local 5 up one level.
//                       #
//                       #
//     } else {          # ---- ELSE FRAME: defcount 1 {n}
//         n = b;        # Implicitly define local 0 at this level.
//     }                 #
//                       #
//     y = 7;            # LHS is local 6 at current level.
//     i = z;            # LHS is local 4 at current level;
//                       #   RHS is unresolved -> slot 0 at current level.
// }                     #
//
// Notes:
//
// * Pass 1 computes frame-relative indices and upstack-level counts,
//   as in the example, for each local variable.
//
// * Pass 2 computes absolute indices for each local variable. These
//   aren't computable in pass 1 due to the example 'y = 7' assignment
//   above: the number of local variables in an upper level can change
//   after the invocation of a child level, so total frame size is not
//   known until all AST nodes in the top-level block have been visited.
//
// * Pass 2 also computes the max depth, counting number of variables, so
//   that for each top-level block we can allocate an array of mlrvals which
//   will be reused on every invocation. (For recursive function calls this will
//   be dynamically allocated.)
//
// * Slot 0 of the top level is reserved for an absent-null for unresolved
//   names on reads.
//
// * The tree-traversal order is done correctly so that if a variable is read
//   before it is defined, then read again after it is defined, then the first
//   read gets absent-null and the second gets the defined value. This also
//   requires the concrete-syntax-tree implementation to initialize the
//   stack to mv_absent on each invocation.
// ================================================================

// ----------------------------------------------------------------
// xxx to do:

// * maybe move ast from containers to mapping?

// * 'semantic analysis': use this to describe CST-build-time checks
// * 'object binding': use this to describe linking func/subr defs and callsites
// * separate verbosity for allocator? and invoke it in UT cases specific to this?
//   -> (note allocation marks in the AST will be printed regardless)

// ================================================================
// Pass-1 stack-frame container: simply a hashmap from name to position on the
// frame relative to the curly-braced statement block (top-level, for-loop,
// if-statement, else-statement, etc.).

typedef struct _stkalc_frame_t {
	long long var_count;
	lhmsi_t* pnames_to_indices;
} stkalc_frame_t;

// ----------------------------------------------------------------
// Pass-1 stack-frame methods

// xxx make a test-and-get API for lhmsi
static      stkalc_frame_t* stkalc_frame_alloc();
static void stkalc_frame_free(stkalc_frame_t* pframe);
static int  stkalc_frame_test_and_get(stkalc_frame_t* pframe, char* name, int* pvalue);
static int  stkalc_frame_get(stkalc_frame_t* pframe, char* name);
static int  stkalc_frame_add(stkalc_frame_t* pframe, char* desc, char* name, int verbose);

// ================================================================
// Pass-1 frame-group container: a linked list with current frame at the head
// and top-level frame at the tail. Within a given top-level block there is a
// tree of curly-braced statement blocks, e.g. a function-definition might have
// an if-statement, else-if, else-if, else, each with its own frame. But during
// pass 1 we only maintain a list from the current frame being analyzed up to
// its parents; sibling branches are not simultaneously stored in this data
// structure.

typedef struct _stkalc_frame_group_t {
	sllv_t* plist;
} stkalc_frame_group_t;

// ----------------------------------------------------------------
// Pass-1 stack-frame-group methods

static      stkalc_frame_group_t* stkalc_frame_group_alloc(stkalc_frame_t* pframe);
static void stkalc_frame_group_free(stkalc_frame_group_t* pframe_group);
static void stkalc_frame_group_push(stkalc_frame_group_t* pframe_group, stkalc_frame_t* pframe);
static stkalc_frame_t* stkalc_frame_group_pop(stkalc_frame_group_t* pframe_group);

// Pass-1 stack-frame-group node-mutator methods: given an AST node containing a
// local-variable usage they assign a frame-relative index and a frame-depth
// counter (how many frames deep into the top-level statement block the node
// is).

static void stkalc_frame_group_mutate_node_for_define(stkalc_frame_group_t* pframe_group,
	mlr_dsl_ast_node_t* pnode, char* desc, int verbose);

static void stkalc_frame_group_mutate_node_for_write(stkalc_frame_group_t* pframe_group,
	mlr_dsl_ast_node_t* pnode, char* desc, int verbose);

static void stkalc_frame_group_mutate_node_for_read(stkalc_frame_group_t* pframe_group,
	mlr_dsl_ast_node_t* pnode, char* desc, int verbose);

// ================================================================
// Pass-1 helper methods for the main entry point to this file.

static void pass_1_for_func_subr_block(mlr_dsl_ast_node_t* pnode);
static void pass_1_for_begin_end_block(mlr_dsl_ast_node_t* pnode);
static void pass_1_for_main_block(mlr_dsl_ast_node_t* pnode);
static void pass_1_for_statement_block(mlr_dsl_ast_node_t* pnode, stkalc_frame_group_t* pframe_group);
static void pass_1_for_node(mlr_dsl_ast_node_t* pnode, stkalc_frame_group_t* pframe_group);

// Pass-2 helper methods for the main entry point to this file.
static void pass_2_for_top_level_block(mlr_dsl_ast_node_t* pnode);
static void pass_2_for_node(mlr_dsl_ast_node_t* pnode,
	int frame_depth, int var_count_below_frame, int var_count_at_frame, int* pmax_var_depth);

// ================================================================
// Main entry point for the bind-stack allocator

void blocked_ast_allocate_locals(blocked_ast_t* paast) {

	for (sllve_t* pe = paast->pfunc_defs->phead; pe != NULL; pe = pe->pnext) {
		pass_1_for_func_subr_block(pe->pvvalue);
	}
	for (sllve_t* pe = paast->psubr_defs->phead; pe != NULL; pe = pe->pnext) {
		pass_1_for_func_subr_block(pe->pvvalue);
	}
	for (sllve_t* pe = paast->pbegin_blocks->phead; pe != NULL; pe = pe->pnext) {
		pass_1_for_begin_end_block(pe->pvvalue);
	}
	{
		pass_1_for_main_block(paast->pmain_block);
	}
	for (sllve_t* pe = paast->pend_blocks->phead; pe != NULL; pe = pe->pnext) {
		pass_1_for_begin_end_block(pe->pvvalue);
	}

	for (sllve_t* pe = paast->pfunc_defs->phead; pe != NULL; pe = pe->pnext) {
		pass_2_for_top_level_block(pe->pvvalue);
	}
	for (sllve_t* pe = paast->psubr_defs->phead; pe != NULL; pe = pe->pnext) {
		pass_2_for_top_level_block(pe->pvvalue);
	}
	for (sllve_t* pe = paast->pbegin_blocks->phead; pe != NULL; pe = pe->pnext) {
		pass_2_for_top_level_block(pe->pvvalue);
	}
	{
		pass_2_for_top_level_block(paast->pmain_block);
	}
	for (sllve_t* pe = paast->pend_blocks->phead; pe != NULL; pe = pe->pnext) {
		pass_2_for_top_level_block(pe->pvvalue);
	}
}

// ----------------------------------------------------------------
static void pass_1_for_func_subr_block(mlr_dsl_ast_node_t* pnode) {
	// xxx make a keystroke-saver, use it here, & use it from the cst builder as well
	if (pnode->type != MD_AST_NODE_TYPE_SUBR_DEF && pnode->type != MD_AST_NODE_TYPE_FUNC_DEF) {
		fprintf(stderr,
			"%s: internal coding error detected in file %s at line %d.\n",
			MLR_GLOBALS.bargv0, __FILE__, __LINE__);
		exit(1);
	}
	//	xxx assert two children of desired type

	stkalc_frame_t* pframe = stkalc_frame_alloc();
	stkalc_frame_group_t* pframe_group = stkalc_frame_group_alloc(pframe);

	printf("\n");
	printf("ALLOCATING LOCALS FOR DEFINITION BLOCK [%s]\n", pnode->text);
	mlr_dsl_ast_node_t* pdef_name_node = pnode->pchildren->phead->pvvalue;
	mlr_dsl_ast_node_t* plist_node = pnode->pchildren->phead->pnext->pvvalue;
	for (sllve_t* pe = pdef_name_node->pchildren->phead; pe != NULL; pe = pe->pnext) {
		mlr_dsl_ast_node_t* pparameter_node = pe->pvvalue;
		stkalc_frame_group_mutate_node_for_define(pframe_group, pparameter_node, "PARAMETER", TRUE/*xxx temp*/);
	}
	pass_1_for_statement_block(plist_node, pframe_group);
	pnode->frame_var_count = pframe->var_count;
	printf("BLK %s frct=%d\n", pnode->text, pnode->frame_var_count);

	stkalc_frame_free(stkalc_frame_group_pop(pframe_group));
	stkalc_frame_group_free(pframe_group);
}

// ----------------------------------------------------------------
static void pass_1_for_begin_end_block(mlr_dsl_ast_node_t* pnode) {
	if (pnode->type != MD_AST_NODE_TYPE_BEGIN && pnode->type != MD_AST_NODE_TYPE_END) {
		fprintf(stderr,
			"%s: internal coding error detected in file %s at line %d.\n",
			MLR_GLOBALS.bargv0, __FILE__, __LINE__);
		exit(1);
	}

	printf("\n");
	printf("ALLOCATING LOCALS FOR %s BLOCK\n", pnode->text);

	stkalc_frame_t* pframe = stkalc_frame_alloc();
	stkalc_frame_group_t* pframe_group = stkalc_frame_group_alloc(pframe);

	pass_1_for_statement_block(pnode->pchildren->phead->pvvalue, pframe_group);
	pnode->frame_var_count = pframe->var_count;
	printf("BLK %s frct=%d\n", pnode->text, pnode->frame_var_count); // xxx fix name

	stkalc_frame_free(stkalc_frame_group_pop(pframe_group));
	stkalc_frame_group_free(pframe_group);
}

// ----------------------------------------------------------------
static void pass_1_for_main_block(mlr_dsl_ast_node_t* pnode) {
	if (pnode->type != MD_AST_NODE_TYPE_STATEMENT_BLOCK) {
		fprintf(stderr,
			"%s: internal coding error detected in file %s at line %d.\n",
			MLR_GLOBALS.bargv0, __FILE__, __LINE__);
		exit(1);
	}

	printf("\n");
	printf("ALLOCATING LOCALS FOR MAIN BLOCK\n");

	stkalc_frame_t* pframe = stkalc_frame_alloc();
	stkalc_frame_group_t* pframe_group = stkalc_frame_group_alloc(pframe);

	pass_1_for_statement_block(pnode, pframe_group);
	pnode->frame_var_count = pframe->var_count;
	printf("BLK %s frct=%d\n", pnode->text, pnode->frame_var_count); // xxx fix name

	stkalc_frame_free(stkalc_frame_group_pop(pframe_group));
	stkalc_frame_group_free(pframe_group);
}

// ----------------------------------------------------------------
static void pass_1_for_statement_block(mlr_dsl_ast_node_t* pnode,
	stkalc_frame_group_t* pframe_group)
{
	if (pnode->type != MD_AST_NODE_TYPE_STATEMENT_BLOCK) {
		fprintf(stderr,
			"%s: internal coding error detected in file %s at line %d.\n",
			MLR_GLOBALS.bargv0, __FILE__, __LINE__);
		exit(1);
	}
	for (sllve_t* pe = pnode->pchildren->phead; pe != NULL; pe = pe->pnext) {
		mlr_dsl_ast_node_t* pchild = pe->pvvalue;
		pass_1_for_node(pchild, pframe_group);
	}
}

// ----------------------------------------------------------------
static void pass_1_for_node(mlr_dsl_ast_node_t* pnode,
	stkalc_frame_group_t* pframe_group)
{
	// xxx make separate functions

	if (pnode->type == MD_AST_NODE_TYPE_LOCAL_DEFINITION) {
		// xxx decide on preorder vs. postorder
		mlr_dsl_ast_node_t* pnamenode = pnode->pchildren->phead->pvvalue;

		stkalc_frame_group_mutate_node_for_define(pframe_group, pnamenode, "DEFINE", TRUE/*xxx temp*/);
		mlr_dsl_ast_node_t* pvaluenode = pnode->pchildren->phead->pnext->pvvalue;
		pass_1_for_node(pvaluenode, pframe_group);

	} else if (pnode->type == MD_AST_NODE_TYPE_LOCAL_ASSIGNMENT) { // xxx rename
		mlr_dsl_ast_node_t* pnamenode = pnode->pchildren->phead->pvvalue;
		stkalc_frame_group_mutate_node_for_write(pframe_group, pnamenode, "WRITE", TRUE/*xxx temp*/);
		mlr_dsl_ast_node_t* pvaluenode = pnode->pchildren->phead->pnext->pvvalue;
		pass_1_for_node(pvaluenode, pframe_group);

	} else if (pnode->type == MD_AST_NODE_TYPE_BOUND_VARIABLE) {
		stkalc_frame_group_mutate_node_for_read(pframe_group, pnode, "READ", TRUE/*xxx temp*/);

	} else if (pnode->type == MD_AST_NODE_TYPE_FOR_SREC) { // xxx comment

		for (int i = 0; i < pframe_group->plist->length; i++) // xxx temp
			printf("::  ");
		printf("PUSH FRAME %s\n", pnode->text);
		stkalc_frame_t* pnext_frame = stkalc_frame_alloc();
		stkalc_frame_group_push(pframe_group, pnext_frame);

		mlr_dsl_ast_node_t* pvarsnode  = pnode->pchildren->phead->pvvalue;
		mlr_dsl_ast_node_t* pblocknode = pnode->pchildren->phead->pnext->pvvalue;

		mlr_dsl_ast_node_t* pknode = pvarsnode->pchildren->phead->pvvalue;
		mlr_dsl_ast_node_t* pvnode = pvarsnode->pchildren->phead->pnext->pvvalue;
		stkalc_frame_group_mutate_node_for_define(pframe_group, pknode, "FOR-BIND", TRUE/*xxx temp*/);
		stkalc_frame_group_mutate_node_for_define(pframe_group, pvnode, "FOR-BIND", TRUE/*xxx temp*/);

		pass_1_for_statement_block(pblocknode, pframe_group);
		pnode->frame_var_count = pnext_frame->var_count;

		stkalc_frame_free(stkalc_frame_group_pop(pframe_group));

		for (int i = 0; i < pframe_group->plist->length; i++)
			printf("::  ");
		printf("POP FRAME %s frct=%d\n", pnode->text, pnode->frame_var_count);

	} else if (pnode->type == MD_AST_NODE_TYPE_FOR_OOSVAR) { // xxx comment

		mlr_dsl_ast_node_t* pvarsnode    = pnode->pchildren->phead->pvvalue;
		mlr_dsl_ast_node_t* pkeylistnode = pnode->pchildren->phead->pnext->pvvalue;
		mlr_dsl_ast_node_t* pblocknode   = pnode->pchildren->phead->pnext->pnext->pvvalue;

		mlr_dsl_ast_node_t* pkeysnode    = pvarsnode->pchildren->phead->pvvalue;
		mlr_dsl_ast_node_t* pvalnode     = pvarsnode->pchildren->phead->pnext->pvvalue;

		// xxx note keylistnode is outside the block binding. in particular if there are any localvar reads
		// in there they shouldn't read from forloop boundvars.
		for (sllve_t* pe = pkeylistnode->pchildren->phead; pe != NULL; pe = pe->pnext) {
			mlr_dsl_ast_node_t* pchild = pe->pvvalue;
			pass_1_for_node(pchild, pframe_group);
		}

		for (int i = 0; i < pframe_group->plist->length; i++) // xxx temp
			printf("::  ");
		printf("PUSH FRAME %s\n", pnode->text);
		stkalc_frame_t* pnext_frame = stkalc_frame_alloc();
		stkalc_frame_group_push(pframe_group, pnext_frame);

		for (sllve_t* pe = pkeysnode->pchildren->phead; pe != NULL; pe = pe->pnext) {
			mlr_dsl_ast_node_t* pkeynode = pe->pvvalue;
			stkalc_frame_group_mutate_node_for_define(pframe_group, pkeynode, "FOR-BIND", TRUE/*xxx temp*/);
		}
		stkalc_frame_group_mutate_node_for_define(pframe_group, pvalnode, "FOR-BIND", TRUE/*xxx temp*/);
		pass_1_for_statement_block(pblocknode, pframe_group);
		// xxx make accessor ...
		pnode->frame_var_count = pnext_frame->var_count;

		stkalc_frame_free(stkalc_frame_group_pop(pframe_group));
		for (int i = 0; i < pframe_group->plist->length; i++)
			printf("::  ");
		printf("POP FRAME %s frct=%d\n", pnode->text, pnode->frame_var_count);

	} else if (pnode->pchildren != NULL) {
		for (sllve_t* pe = pnode->pchildren->phead; pe != NULL; pe = pe->pnext) {
			mlr_dsl_ast_node_t* pchild = pe->pvvalue;

			if (pchild->type == MD_AST_NODE_TYPE_STATEMENT_BLOCK) {

				for (int i = 0; i < pframe_group->plist->length; i++) // xxx temp
					printf("::  ");
				printf("PUSH FRAME %s\n", pchild->text);

				stkalc_frame_t* pnext_frame = stkalc_frame_alloc();
				stkalc_frame_group_push(pframe_group, pnext_frame);

				pass_1_for_statement_block(pchild, pframe_group);
				pchild->frame_var_count = pnext_frame->var_count;

				stkalc_frame_free(stkalc_frame_group_pop(pframe_group));

				for (int i = 0; i < pframe_group->plist->length; i++)
					printf("::  ");
				printf("POP FRAME %s frct=%d\n", pnode->text, pchild->frame_var_count);

			} else {
				pass_1_for_node(pchild, pframe_group);
			}
		}
	}
}

// ================================================================
static stkalc_frame_t* stkalc_frame_alloc() {
	stkalc_frame_t* pframe = mlr_malloc_or_die(sizeof(stkalc_frame_t));
	pframe->var_count = 0;
	pframe->pnames_to_indices = lhmsi_alloc();
	return pframe;
}

static void stkalc_frame_free(stkalc_frame_t* pframe) {
	if (pframe == NULL)
		return;
	lhmsi_free(pframe->pnames_to_indices);
	free(pframe);
}

static int stkalc_frame_test_and_get(stkalc_frame_t* pframe, char* name, int* pvalue) {
	long long llvalue;
	int rc = lhmsi_test_and_get(pframe->pnames_to_indices, name, &llvalue);
	if (rc)
		*pvalue = llvalue;
	return rc;
}

static int stkalc_frame_get(stkalc_frame_t* pframe, char* name) {
	return lhmsi_get(pframe->pnames_to_indices, name);
}

static int stkalc_frame_add(stkalc_frame_t* pframe, char* desc, char* name, int verbose) {
	int rv = pframe->var_count;
	lhmsi_put(pframe->pnames_to_indices, name, pframe->var_count, NO_FREE);
	pframe->var_count++;
	return rv;
}

// ================================================================
static stkalc_frame_group_t* stkalc_frame_group_alloc(stkalc_frame_t* pframe) {
	stkalc_frame_group_t* pframe_group = mlr_malloc_or_die(sizeof(stkalc_frame_group_t));
	pframe_group->plist = sllv_alloc();
	sllv_prepend(pframe_group->plist, pframe);
	stkalc_frame_add(pframe, "FOR_ABSENT", "", /*xxx temp*/TRUE);
	return pframe_group;
}

static void stkalc_frame_group_free(stkalc_frame_group_t* pframe_group) {
	if (pframe_group == NULL)
		return;
	while (pframe_group->plist->phead != NULL) {
		stkalc_frame_free(sllv_pop(pframe_group->plist));
	}
	sllv_free(pframe_group->plist);
	free(pframe_group);
}

static void stkalc_frame_group_push(stkalc_frame_group_t* pframe_group, stkalc_frame_t* pframe) {
	sllv_prepend(pframe_group->plist, pframe);
}

static stkalc_frame_t* stkalc_frame_group_pop(stkalc_frame_group_t* pframe_group) {
	return sllv_pop(pframe_group->plist);
}

static void stkalc_frame_group_mutate_node_for_define(stkalc_frame_group_t* pframe_group, mlr_dsl_ast_node_t* pnode,
	char* desc, int verbose)
{
	char* op = "REUSE";
	stkalc_frame_t* pframe = pframe_group->plist->phead->pvvalue;
	pnode->upstack_frame_count = 0;
	if (!stkalc_frame_test_and_get(pframe, pnode->text, &pnode->frame_relative_index)) {
		pnode->frame_relative_index = stkalc_frame_add(pframe, desc, pnode->text, verbose);
		op = "ADD";
	}
	if (verbose) {
		for (int i = 1; i < pframe_group->plist->length; i++) {
			printf("::  ");
		}
		printf("::  %s %s %s @ %du%d\n", op, desc, pnode->text,
			pnode->frame_relative_index, pnode->upstack_frame_count);
	}
}

static void stkalc_frame_group_mutate_node_for_write(stkalc_frame_group_t* pframe_group, mlr_dsl_ast_node_t* pnode,
	char* desc, int verbose)
{
	char* op = "REUSE";
	int found = FALSE;
	// xxx loop. if not found, fall back to top frame.
	pnode->upstack_frame_count = 0;
	for (sllve_t* pe = pframe_group->plist->phead; pe != NULL; pe = pe->pnext, pnode->upstack_frame_count++) {
		stkalc_frame_t* pframe = pe->pvvalue;
		if (stkalc_frame_test_and_get(pframe, pnode->text, &pnode->frame_relative_index)) {
			found = TRUE; // xxx dup
			break;
		}
	}

	if (!found) {
		pnode->upstack_frame_count = 0;
		stkalc_frame_t* pframe = pframe_group->plist->phead->pvvalue;
		pnode->frame_relative_index = stkalc_frame_add(pframe, desc, pnode->text, verbose);
		// xxx temp
		op = "ADD";
	}

	if (verbose) {
		for (int i = 1; i < pframe_group->plist->length; i++) {
			printf("::  ");
		}
		printf("::  %s %s %s @ %du%d\n", op, desc, pnode->text,
			pnode->frame_relative_index, pnode->upstack_frame_count);
	}
}

// xxx make this very clear in the header somehow ... this is an assumption to be tracked across modules.
static void stkalc_frame_group_mutate_node_for_read(stkalc_frame_group_t* pframe_group, mlr_dsl_ast_node_t* pnode,
	char* desc, int verbose)
{
	char* op = "PRESENT";
	int found = FALSE;
	// xxx loop. if not found, fall back to top frame.
	int upstack_frame_count = 0;
	for (sllve_t* pe = pframe_group->plist->phead; pe != NULL; pe = pe->pnext, upstack_frame_count++) {
		stkalc_frame_t* pframe = pe->pvvalue;
		if (stkalc_frame_test_and_get(pframe, pnode->text, &pnode->frame_relative_index)) {
			found = TRUE; // xxx dup
			break;
		}
	}

	// xxx if not found: go to the tail & use the "" entry
	if (!found) {
		stkalc_frame_t* plast = pframe_group->plist->ptail->pvvalue;
		pnode->frame_relative_index = stkalc_frame_get(plast, "");
		upstack_frame_count = pframe_group->plist->length - 1;
		op = "ABSENT";
	}

	if (verbose) {
		for (int i = 1; i < pframe_group->plist->length; i++) {
			printf("::  ");
		}
		printf("::  %s %s %s @ %du%d\n", desc, pnode->text, op, pnode->frame_relative_index, upstack_frame_count);
	}
	pnode->upstack_frame_count = upstack_frame_count;

}

// ================================================================
static void pass_2_for_top_level_block(mlr_dsl_ast_node_t* pnode) {
	int frame_depth = 0;
	int var_count_below_frame = 0;
	int var_count_at_frame = 0;
	int max_var_depth   = 0;
	printf("\n");
	printf("ABSOLUTIZING LOCALS FOR DEFINITION BLOCK [%s]\n", pnode->text);
	pass_2_for_node(pnode, frame_depth, var_count_below_frame, var_count_at_frame, &max_var_depth);
	pnode->max_var_depth = max_var_depth;
}

static void pass_2_for_node(mlr_dsl_ast_node_t* pnode,
	int frame_depth, int var_count_below_frame, int var_count_at_frame, int* pmax_var_depth)
{
	if (pnode->frame_var_count != MD_UNUSED_INDEX) {
		var_count_below_frame += var_count_at_frame;
		var_count_at_frame = pnode->frame_var_count;
		int depth = var_count_below_frame + var_count_at_frame;
		frame_depth++;
		if (depth > *pmax_var_depth) {
			*pmax_var_depth = depth;
		}
		// xxx funcify
		for (int i = 1; i < frame_depth; i++)
			printf("::  ");
		printf("FRAME [%s] var_count_below=%d var_count_at=%d max_var_depth=%d\n",
			pnode->text, var_count_below_frame, var_count_at_frame, *pmax_var_depth);
	}

	if (pnode->frame_relative_index != MD_UNUSED_INDEX) {
		pnode->absolute_index = var_count_below_frame + pnode->frame_relative_index;
		for (int i = 0; i < frame_depth; i++)
			printf("::  ");
		printf("NODE %s %du%d -> %d\n",
			pnode->text,
			pnode->frame_relative_index,
			pnode->upstack_frame_count,
			pnode->absolute_index);
	}

	if (pnode->pchildren != NULL) {
		for (sllve_t* pe = pnode->pchildren->phead; pe != NULL; pe = pe->pnext) {
			mlr_dsl_ast_node_t* pchild = pe->pvvalue;
			pass_2_for_node(pchild, frame_depth,
				var_count_below_frame, var_count_at_frame, pmax_var_depth);
		}
	}
}
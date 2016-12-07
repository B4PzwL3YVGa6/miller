// ================================================================
// Array-only (open addressing) string-to-void linked hash map with linear
// probing for collisions.
//
// John Kerl 2012-08-13
//
// Notes:
// * null key is not supported.
// * null value is supported.
//
// See also:
// * http://en.wikipedia.org/wiki/Hash_table
// * http://docs.oracle.com/javase/6/docs/api/java/util/Map.html
// ================================================================

#ifndef LHMSV_H
#define LHMSV_H

#include "containers/sllv.h"
#include "lib/free_flags.h"

// ----------------------------------------------------------------
typedef struct _lhmsve_t {
	int   ideal_index;
	char* key;
	void* pvvalue;
	char  free_flags;
	struct _lhmsve_t *pprev;
	struct _lhmsve_t *pnext;
} lhmsve_t;

typedef unsigned char lhmsve_state_t;

typedef struct _lhmsv_t {
	int             num_occupied;
	int             num_freed;
	int             array_length;
	lhmsve_t*       entries;
	lhmsve_state_t* states;
	lhmsve_t*       phead;
	lhmsve_t*       ptail;
} lhmsv_t;

// ----------------------------------------------------------------
lhmsv_t* lhmsv_alloc();
void  lhmsv_free(lhmsv_t* pmap);
void  lhmsv_clear(lhmsv_t* pmap);

void  lhmsv_put(lhmsv_t* pmap, char* key, void* pvvalue, char free_flags);
void* lhmsv_get(lhmsv_t* pmap, char* key);
int   lhmsv_has_key(lhmsv_t* pmap, char* key);

// Unit-test hook
int lhmsv_check_counts(lhmsv_t* pmap);

#endif // LHMSV_H

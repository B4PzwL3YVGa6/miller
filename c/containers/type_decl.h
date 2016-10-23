#ifndef TYPE_DECL_H
#define TYPE_DECL_H

#include "containers/mlrval.h"

// ----------------------------------------------------------------
// These use the MT defines from mlrval.h
#define TYPE_MASK_ERROR   (1 << MT_ERROR)
#define TYPE_MASK_ABSENT  (1 << MT_ABSENT)
#define TYPE_MASK_EMPTY   (1 << MT_EMPTY)
#define TYPE_MASK_STRING  (1 << MT_STRING)
#define TYPE_MASK_INT     (1 << MT_INT)
#define TYPE_MASK_FLOAT   (1 << MT_FLOAT)
#define TYPE_MASK_BOOLEAN (1 << MT_BOOL)
#define TYPE_MASK_MAP     (1 << MT_DIM) // xxx for later. xxx note map-types are outside of mlrval.h.

#define TYPE_MASK_NUMERIC (TYPE_MASK_INT | TYPE_MASK_FLOAT)
#define TYPE_MASK_PRESENT (TYPE_MASK_EMPTY | TYPE_MASK_STRING | TYPE_MASK_INT | TYPE_MASK_FLOAT | TYPE_MASK_BOOLEAN)
#define TYPE_MASK_ANY     (~0)

// ----------------------------------------------------------------
char* type_mask_to_desc(int type_mask);
static inline int type_mask_from_mv(mv_t* pmv) {
	return 1 << pmv->type;
}

#endif // TYPE_DECL_H

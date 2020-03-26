/*========================================================================
	TradeXpress

	File:		tr_array.h
	Author:	Jean-François CLAVIER (JFC)
	Date:		thu Apr 07 2011

	This file defines internal type used in tr_array.c.


	Copyright (c) 2011 GenerixGroup
==========================================================================
	Record all changes here and update the above string accordingly.
	07.04.11/JFC	Created.
	08.04/11/JFC	move ststistics nodes data in compilation option
========================================================================*/

#ifndef _TR_ARRAY_H
#define _TR_ARRAY_H

#ifdef STATISTIC_ON_ARRAY
#define INSTRUMENT(x) x ;
#else
#define INSTRUMENT(x) /* no stat */
#endif

/*
 * Each array line (name=value)  is a node in the binary tree.
 */

typedef struct am_node {
	char *name;
	union am_value {
		char  *txt;
		double num;
	} value;
	struct am_node *left;
	struct am_node *rite;
	struct am_node *linear; /* next entry in sorted list */
} ArrayNode;

#define TR_CONST_ARRAY_STACKSIZE 100

#define ARRAYSTACK_DEF 	int size; /* size in nodes for this stack */ \
	int count;                    /* used elements */ \
	struct ArrayStack_ *next;     /* stack extension */

/* ArrayStack: array nodes stack  used to free the array. Use for the 10 first allocations */ 
typedef struct ArrayStack_ {
	ARRAYSTACK_DEF
	ArrayNode* nodes[TR_CONST_ARRAY_STACKSIZE];  /* nodes stack */
} ArrayStack;

/* ArrayStack: array nodes stack  used to free the array. Use for the 10 next allocations (10 to 19) */ 
typedef struct ArrayStack2_ {
	ARRAYSTACK_DEF
	ArrayNode* nodes[TR_CONST_ARRAY_STACKSIZE*10];  /* nodes stack */
} ArrayStack2;

/* ArrayStack: array nodes stack  used to free the array. Use for the next allocations (20 or above) */ 
typedef struct ArrayStack3_ {
	ARRAYSTACK_DEF
	ArrayNode* nodes[TR_CONST_ARRAY_STACKSIZE*100];  /* nodes stack */
} ArrayStack3;


/*
 *  Each array is defined as a binary tree.
 */
struct am_tab {
	char *name;
	struct am_tab  *left;      /* other tables to the left */
	struct am_tab  *rite;      /* other tables to the right */
	struct am_node *linear;    /* sorted entries in table if not NULL */
	struct am_node *nodes;     /* tree of entries */
	struct am_node *hint;      /* last node used */
	struct am_node *most_left; /* leftmost node in tree */
	struct am_node *most_rite; /* rightmost likewise */
	ArrayStack *stack;         /* stack to push nodes for free */
	enum {
		AM_BAD,
		AM_TEXT,
		AM_NUM,
	} type;

#ifdef STATISTIC_ON_ARRAY
	INSTRUMENT(int lookups)       /* number of getxxx() done  */
	INSTRUMENT(int cache_hits)    /* ... of which hint worked */
	INSTRUMENT(int turns_left)    /* descends to the left     */
	INSTRUMENT(int turns_rite)    /* ... and right            */
	INSTRUMENT(int leftmost_hits) /* hits on leftmost node    */
	INSTRUMENT(int ritemost_hits) /* rightmost, likewise      */
	INSTRUMENT(int walks)         /* rightmost, likewise      */
#endif
};

#endif /* _TR_ARRAY_H */


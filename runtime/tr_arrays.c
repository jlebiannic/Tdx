/*============================================================================
	V E R S I O N   C O N T R O L
============================================================================*/
#include "conf/local_config.h"
MODULE("@(#)TradeXpress $Id: tr_arrays.c 47429 2013-10-29 15:27:44Z cdemory $")
/*LIBRARY(libruntime_version)
*/
/*============================================================================
  Record all changes here and update the above string accordingly.
  3.01 23.05.94/JN	?
       28.11.94/JN	alpha fix with arglist.
  3.02 08.03.95/JN	Some recursions changed to iterations.
  3.03 21.03.95/JN	Optimization to WRITEBUFFER special case (text array).
  3.04 29.03.95/JN	Continuation of the above.
  3.05 18.04.95/JN	? change.
  3.06 13.02.96/JN	recursive walks of arrays now ok (strtok syndrome).
			Speedup also, not when having a reason to change...
			Now textset(EMPTY) puts TR_EMPTY in tree
  Bug 10012	08.04.02/JFC	Add a stack on array to avoid walking throw the tree to free nodes
					move statistics under compilation option to optimize memory cost.


  This (too) has problems with memory management,
  we keep private data in tree and return pointers to it to TCL.
  All too easy to free that data twice...

  Walks can consume ridiculous amounts of stack,
  now little better (one third of previous) but still...

  A good way to walk an array is like this:

    void *p = tr_am_rewind(taTMP);
    char *name, *value;
    while (tr_amtext_getpair(&p, &name, &value)) {
	whatever
    }

  TCL does it little differently,

    char *tNAME;

    void p[2];
    tr_am_rewind2(taTMP, p);
    while (tr_am_getkey2(p, &tNAME)) {
	whatever
    }

  There exists a number of "associative" arrays, they are
  held in a binary tree sorted by the names of the arrays.
  TCL generates the names with a running number.

  It probably would be better to keep them in a single list,
  pulling the table in question to the top of the list
  each time it is accessed. TBD.

  Currently, instead, last table looked at is remembered and
  tried before searching the whole tree of tables.


  Each table is again a binary tree, sorted by name.
  The value can be either a string, or a double; one table
  can contain only strings or doubles, not both.
  TCL generates the names with tr_MakeIndex() and it is
  a zero-padded number in string form (for [1]) or any string
  (for ["hoppa"]). Yet another place for speedup, using true numbers
  for keys whenever possible...

  Each table keeps three hints; last accessed node, the node
  on the furthest left and the node on the furthest right.
  If/when nodes are added in alphabetical order, the tree grows
  on the right side only and degenerates into simple list.

  When the tree is "rewound", it is recursively walked and
  an alphabetical list is created using the member 'linear'.
  This list is remembered as long as nothing is inserted
  into the table, so the walk can be skipped for multiple
  while-loops without intervening insertions.
  When the insertions come strictly in order, we can skip
  the purging of the alphabetical list. This is different
  from previous behavior, where insertions never showed up
  in while-loops !!!

  Rewinding returns a walking-position to the caller,
  and this walkpos is then used to keep track of current location
  between calls to getpair(), getkey() or getkey2().
============================================================================*/

#include <stdio.h>
#include <string.h>

#include "tr_externals.h"
#include "tr_privates.h"
#include "tr_prototypes.h"
#include "tr_arrays.h"
#include "tr_strings.h"

static int table_lookups, table_cache_hits;

static int StackCount[] = {TR_CONST_ARRAY_STACKSIZE, TR_CONST_ARRAY_STACKSIZE*10, TR_CONST_ARRAY_STACKSIZE*100};
static int StackSizes[] = {sizeof(ArrayStack), sizeof(ArrayStack2), sizeof(ArrayStack3)};

/*
 *  All arrays are somewhere below asstabs
 *
 *  For the common case where one walks thru a single array,
 *  to prevent exponential time,
 *  last looked-up table is cached, and all tables cache
 *  the last referenced node.
 */
static struct am_tab *asstabs    = NULL;
static struct am_tab *table_hint = NULL;

/*
 *  This is used to hold the function and generic arg for walker-functions.
 */
struct am_walk_args {
	int (*fcn)();
	void *arg;
};

/*
 *  The pervert placement of register rv
 *  is just to hint for the compiler not to use stackspace for them.
 *  Same reason for using the above struct for arguments.
 */
static int am_walk(struct am_node *np, struct am_walk_args *arg)
{
	if (np->left) {
		register int rv = am_walk(np->left, arg);
		if (rv)
			return (rv);
	}
	{
		register int rv = (*arg->fcn)(np->name, np->value, arg->arg, np);
		if (rv)
			return (rv);
	}
	if (np->rite) {
		register int rv = am_walk(np->rite, arg);
		if (rv)
			return (rv);
	}
	return (0);
}

static int am_walkreversed(struct am_node *np, struct am_walk_args *arg)
{
	if (np->rite) {
		register int rv = am_walkreversed(np->rite, arg);
		if (rv)
			return (rv);
	}
	{
		register int rv = (*arg->fcn)(np->name, np->value, arg->arg, np);
		if (rv)
			return (rv);
	}
	if (np->left) {
		register int rv = am_walkreversed(np->left, arg);
		if (rv)
			return (rv);
	}
	return (0);
}

/*
 * Find the table with given name.
 * Returns the table or NULL.
 */
static struct am_tab *find_table(char *name)
{
	struct am_tab *tp = table_hint;

	if (!name)
		return NULL;
	
	INSTRUMENT(table_lookups++)

	if (tp && !strcmp(tp->name, name)) {
		INSTRUMENT(table_cache_hits++)
		return (tp);
	}
	tp = asstabs;

	while (tp) {
		int cmp = strcmp(tp->name, name);
		if (cmp < 0) {
			tp = tp->rite;
			continue;
		}
		if (cmp > 0) {
			tp = tp->left;
			continue;
		}
		table_hint = tp;
		break;
	}
	return (tp);
}

/*
 * Find (and create as empty if needed)
 * the given table.
 */
static struct am_tab *create_table(char *name, int type)
{
	struct am_tab *tp = table_hint;
	struct am_tab **pos;

	INSTRUMENT(table_lookups++)

	if (tp && !strcmp(tp->name, name)) {
		INSTRUMENT(table_cache_hits++)
		return (tp);
	}
	pos = &asstabs;

	while (tp = *pos) {
		int cmp = strcmp(tp->name, name);
		if (cmp < 0) {
			pos = &tp->rite;
			continue;
		}
		if (cmp > 0) {
			pos = &tp->left;
			continue;
		}
		goto found;
	}
	*pos = tp = tr_zalloc(sizeof(*tp));
	if (!tp) {
		tr_OutOfMemory();  /* exit on error */
	}

	tp->name = tr_strdup(name);
	tp->type = type;
found:
	table_hint = tp;

	return (tp);
}

void tr_am_remove_completely(char *table) {
	struct am_tab *tab, *tp, *l, *r;
	struct am_tab **pos;

	if ((tab = find_table(table)) == NULL)
		return;

	tr_am_remove(table);

	pos = &asstabs;

	if(*pos != tab) {
		while (tp = *pos) {
			int cmp;

			if((tp->left && tp->left == tab) || (tp->rite && tp->rite == tab))
				break;

			cmp = strcmp(tp->name, table);
			if (cmp < 0) {
				pos = &tp->rite;
				continue;
			}
			if (cmp > 0) {
				pos = &tp->left;
				continue;
			}
		}
	} else {
		tp = *pos;
	}

	if(tp->left == tab)
		tp->left = NULL;
	else
		tp->rite = NULL;

	l = tab->left;
	r = tab->rite;

	/*
	 * Remove tab contents
	 */
	tr_free(tab->name);
	tr_free(tab);

	if(l && l->name) {
		/*
		 * Reinsert left site
		 */
		pos = &asstabs;
		while (tp = *pos) {
			int cmp;

			cmp = strcmp(tp->name, l->name);
			if (cmp < 0) {
				pos = &tp->rite;
				continue;
			}
			if (cmp > 0) {
				pos = &tp->left;
				continue;
			}
			break;
		}
		*pos = l;
	}

	if(r && r->name) {
		/*
		 * Reinsert left site
		 */
		pos = &asstabs;
		while (tp = *pos) {
			int cmp;

			cmp = strcmp(tp->name, r->name);
			if (cmp < 0) {
				pos = &tp->rite;
				continue;
			}
			if (cmp > 0) {
				pos = &tp->left;
				continue;
			}
			break;
		}
		*pos = r;
	}

	table_hint = NULL;
}

static struct am_node *find_node(struct am_tab *tab, char *name)
{
	struct am_node *np = tab->hint;

	INSTRUMENT(tab->lookups++)

	if (np && !strcmp(np->name, name)) {
		INSTRUMENT(tab->cache_hits++)
		return (np);
	}
	np = tab->nodes;

	while (np) {
		int cmp = strcmp(np->name, name);
		if (cmp < 0) {
			INSTRUMENT(tab->turns_rite++)
			np = np->rite;
			continue;
		}
		if (cmp > 0) {
			INSTRUMENT(tab->turns_left++)
			np = np->left;
			continue;
		}
		tab->hint = np;
		break;
	}
	return (np);
}

/* push_node: store each node in a stack to avoid walking the tree to free it.
	The stack is first allocated with 100 nodes, if the array grows we allocated
	stack extension. The first 10 stack blocs have a size of 100 nodes, the 10 next 1000
	the next 10000.
*/
static void push_node(struct am_tab *tab, struct am_node *node)
{
	ArrayStack *stack;
	int stackCount = 1, stackSizeIdx = 0;

	stack = tab->stack;
	if (!stack) {
		tab->stack = tr_zalloc(sizeof(ArrayStack));
		if (!tab->stack) {
			tr_OutOfMemory();  /* exit on error */
		}
		tab->stack->size = StackCount[stackSizeIdx];
		stack = tab->stack;
		tr_Trace(3,1, "push_node stack 0x%x size %d nodes 0x%x\n", stack, stack->size, stack->nodes);
	}
	while (stack->count == stack->size) {
		stackCount++;
		/* First 10 stack will contain 100 nodes, next 10 1000 nodes, and next 10000 */
		if (stackCount > 20)
			stackSizeIdx = 2;
		else if (stackCount > 10)
			stackSizeIdx = 1;
		if (!stack->next) {
			stack->next = tr_zalloc(StackSizes[stackSizeIdx]);
			if (!stack->next) {
				tr_OutOfMemory();  /* exit on error */
			}
			stack->next->size = StackCount[stackSizeIdx];
			tr_Trace(3,1, "push_node stack 0x%x size %d nodes 0x%x\n", stack->next, stack->next->size, stack->next->nodes);
		}
		stack = stack->next;
	}
	stack->nodes[stack->count] = node;
	stack->count++;
}

/*
 * Find (and insert) a node to tree starting at *pos.
 * We make a copy of name if this is a new one.
 * Also, when a new one is created,
 * value is initialized to a copy of empty string.
 *
 * This never fails, we create a new one when
 * the search would have failed.
 *
 * Whenever entries are added or removed,
 * the sorted list starting from tab->linear must be zeroed.
 * Then it will be rebuilt before walks.
 */
static struct am_node *create_node(struct am_tab *tab, char *nodename)
{
	struct am_node *np;
	struct am_node **pos;

	INSTRUMENT(tab->lookups++)

	if (tab->hint && !strcmp(tab->hint->name, nodename)) {

		INSTRUMENT(tab->cache_hits++)
		return (tab->hint);

	}
	if (tab->nodes == NULL) {
		/*
		 *  Nothing yet.
		 */
		pos = &tab->nodes;

	} else if (strcmp(nodename, tab->most_rite->name) > 0) {
		/*
		 * Adding beyond right limit,
		 * position over the NULL on rightmost node.
		 */
		INSTRUMENT(tab->ritemost_hits++)
		pos = &tab->most_rite->rite;

	} else if (strcmp(nodename, tab->most_left->name) < 0) {
		/*
		 * Adding beyond left limit.
		 */
		INSTRUMENT(tab->leftmost_hits++)
		pos = &tab->most_left->left;

	} else {
		/*
		 * Somewhere in the middle,
		 * exhaustive search.
		 */
		pos = &tab->nodes;
		while (np = *pos) {
			int cmp = strcmp(np->name, nodename);
			if (cmp < 0) {
				pos = &np->rite;
				INSTRUMENT(tab->turns_rite++)
				continue;
			}
			if (cmp > 0) {
				pos = &np->left;
				INSTRUMENT(tab->turns_left++)
				continue;
			}
			goto found;
		}
	}
	/*
	 * Node was not in the table.
	 * Create a new one, zalloc sets all the pointers to 0
	 */
	np = (struct am_node *)tr_zalloc(sizeof(*np));

	np->name = tr_strdup(nodename);
	push_node(tab, np);

	switch (tab->type) {
	default:
		*(int*)0=0;
	case AM_TEXT:
		np->value.txt = TR_EMPTY;
		break;
	case AM_NUM:
		np->value.num = 0.0;
		break;
	}
	if (tab->nodes == NULL) {
		/*
		 * When adding the first node,
		 * start optimistically building
		 * the linear list.
		 */
		tab->linear    = np;
		tab->most_rite = np;
		tab->most_left = np;

	} else if (pos == &tab->most_rite->rite) {
		/*
		 *  When adding to the right,
		 *  the alphab. list can be expanded,
		 *  if it still holds.
		 */
		if (tab->linear) {
			tab->most_rite->linear = np;
		}
		tab->most_rite = np;

	} else if (pos == &tab->most_left->left) {
		/*
		 *  Adding to the left,
		 *  new start of alphab. list.
		 */
		if (tab->linear) {
			np->linear = tab->linear;
			tab->linear = np;
		}
		tab->most_left = np;

	} else {
		/*
		 *  In the midst.
		 *  Forget alphab. list.
		 */
		tab->linear = NULL;
	}
	*pos = np;
found:
	tab->hint = np;

	return (np);
}

char *tr_am_textget(char *table, char *name)
{
	struct am_tab  *tab;
	struct am_node *node;
	int not_found = 0;

	if ((tab = find_table(table)) == NULL)
		not_found = 1;
	else if ((node = find_node(tab, name)) == NULL)
		not_found = 1;

	/* free parameters if they are in the pool */
	tr_MemPoolFree(table);
	tr_MemPoolFree(name);

	if (not_found)
		return TR_EMPTY;

	return node->value.txt;
}

double tr_am_numget(char *table, char *name)
{
	struct am_tab   *tab;
	struct am_node *node;

	if ((tab = find_table(table)) == NULL)
		return (0.0);
	if ((node = find_node(tab, name)) == NULL)
		return (0.0);

	return (node->value.num);
}

void tr_am_textset(char *table, char *name, char *txt)
{
	struct am_tab  *tab;
	struct am_node *node;

	tab  = create_table(table, AM_TEXT);
	node = create_node(tab, name);

	if (node->value.txt != NULL
	 && node->value.txt != TR_EMPTY)
		tr_free(node->value.txt);

	if (txt != NULL
	 && txt != TR_EMPTY)
		node->value.txt = tr_strdup(txt);
	else
		node->value.txt = TR_EMPTY;

	/* free parameters if they are in the pool */
	tr_MemPoolFree(table);
	tr_MemPoolFree(name);
	tr_MemPoolFree(txt);
}

void tr_am_numset(char *table, char *name, double num)
{
	struct am_tab *tab;
	struct am_node *node;

	tab  = create_table(table, AM_NUM);
	node = create_node(tab, name);
	node->value.num = num;
	/* free parameters if they are in the pool */
	tr_MemPoolFree(table);
	tr_MemPoolFree(name);
}

/*
 * Find (and invent if necessary) the named node in named table.
 * The address of the value is returned.
 * Caller can modify the value if the pointer is also updated
 * and the storage (for text) is allocated with tr_xxxlloc().
 *
 * Using this converts TR_EMPTY into normal (but empty) string.
 * (tr_Put() requires this, currently our only caller)
 */
char **tr_am_text(char *table, char *name)
{
	struct am_tab  *tab;
	struct am_node *node;

	tab  = create_table(table, AM_TEXT);
	node = create_node(tab, name);

	if (node->value.txt == TR_EMPTY)
		node->value.txt = tr_strdup("");

	/* free parameters if they are in the pool */
	tr_MemPoolFree(table);
	tr_MemPoolFree(name);

	return (&node->value.txt);
}

double *tr_am_num(char *table, char *name)
{
	struct am_tab   *tab;
	struct am_node *node;

	tab  = create_table(table, AM_NUM);
	node = create_node(tab, name);

	/* free parameters if they are in the pool */
	tr_MemPoolFree(table);
	tr_MemPoolFree(name);

	return (&node->value.num);
}

static int am_linearize(char *name, union am_value value, struct am_node **walkpos, /* the generic arg */ struct am_node *this)
{
	this->linear = *walkpos;
	*walkpos     = this;
	return (0);
}

void *tr_am_rewind(char *table)
{
	struct am_tab *tab;
	struct am_walk_args warg;

	if ((tab = find_table(table)) == NULL || tab->nodes == NULL)
		return (NULL);

	if (tab->linear == NULL) {
		INSTRUMENT(tab->walks++)
		warg.fcn = am_linearize;
		warg.arg = &tab->linear;
		am_walkreversed(tab->nodes, &warg);
	}
	/* free parameters if they are in the pool */
	tr_MemPoolFree(table);

	return (tab->linear);
}

/*
 *  Variation of the above, saves the walkposition and the table
 *  so that following calls to getkey2 can save the location in table too.
 *  handle should be declared like void *q[4];
 *  (4 just for future expansions, only 2 are used currently).
 */
void tr_am_rewind2(char *table, void **handle)
{
	struct am_tab *tab;
	struct am_walk_args warg;

	if ((tab = find_table(table)) == NULL || tab->nodes == NULL) {
		handle[0] = NULL;
		handle[1] = NULL;
	} else {
		if (tab->linear == NULL) {
			INSTRUMENT(tab->walks++)
			warg.fcn = am_linearize;
			warg.arg = &tab->linear;
			am_walkreversed(tab->nodes, &warg);
		}
		handle[0] = tab->linear;
		handle[1] = tab;
	}
	/* free parameters if they are in the pool */
	tr_MemPoolFree(table);
}

/*
 *  This should also set the hint, but it does not know the table !
 */
int tr_am_getkey(struct am_node **walkpos, char **idxp)
{
	if (walkpos && *walkpos) {
		*idxp    = (*walkpos)->name;
		*walkpos = (*walkpos)->linear;
		return (1);
	}
	*idxp = TR_EMPTY;
	return (0);
}

/*
 *  Because of the above complaint, this function was made.
 *  Save the location in table
 *  for the benefit of imminent calls to textget()
 */
int tr_am_getkey2(void **handle, char **idxp)
{
	struct am_node *walkpos = handle[0];
	struct am_tab  *tab     = handle[1];

	if (walkpos) {
		tr_Assign(idxp, walkpos->name);
		handle[0] = walkpos->linear;
		if (tab)
			tab->hint = walkpos;
		return (1);
	}
	tr_AssignEmpty(idxp);
	return (0);
}

int tr_amtext_getpair(struct am_node **walkpos, char **idxp, char **valp)
{
	if (walkpos && *walkpos) {
		*idxp    = (*walkpos)->name;
		*valp    = (*walkpos)->value.txt;
		*walkpos = (*walkpos)->linear;
		return (1);
	}
	return (0);
}

int tr_amnum_getpair(struct am_node **walkpos, char **idxp, double *valp)
{
	if (walkpos && *walkpos) {
		*idxp    = (*walkpos)->name;
		*valp    = (*walkpos)->value.num;
		*walkpos = (*walkpos)->linear;
		return (1);
	}
	return (0);
}

static void am_freetext(struct am_tab *tab)
{
	ArrayStack *stack, *nextStack;
	struct am_node *node;
	int i;

	tr_Trace(3, 1, "am_freetext tab 0x%x\n", tab);
	stack = tab->stack;
	while (stack) {
		for (i = 0; i < stack->count; i++) {
			node = stack->nodes[i];
			if (node->value.txt && node->value.txt != TR_EMPTY) tr_free(node->value.txt);
			tr_free(node->name);
			tr_free(node);
			stack->nodes[i] = NULL;
		}
		stack->count = 0;
		stack = stack->next;
	}
	stack = tab->stack;
	while (stack) {
		nextStack = stack->next;
		tr_free(stack);
		stack = nextStack;
	}
	tab->stack = NULL;
}

static void am_freedouble(struct am_tab *tab)
{
	ArrayStack *stack, *nextStack;
	struct am_node *node;
	int i;

	tr_Trace(3, 1, "am_freedouble tab 0x%x\n", tab);
	stack = tab->stack;
	while (stack) {
		for (i = 0; i < stack->count; i++) {
			node = stack->nodes[i];
			tr_free(node->name);
			tr_free(node);
			stack->nodes[i] = NULL;
		}
		stack->count = 0;
		stack = stack->next;
	}
	stack = tab->stack;
	while (stack) {
		nextStack = stack->next;
		tr_free(stack);
		stack = nextStack;
	}
	tab->stack = NULL;
}

int tr_am_remove(char *table)
{
	struct am_tab *tab;

	if ((tab = find_table(table)) == NULL) {
		tr_Trace(1, 1, "tr_am_remove table %s not found\n", table ? table : "");
		return (-1);
	}

	tab->linear    = NULL;
	tab->hint      = NULL;
	tab->most_rite = NULL;
	tab->most_left = NULL;

	if (tab->nodes) {
		switch (tab->type) {
		default:
			*(int*)0=0;
		case AM_TEXT:
			am_freetext(tab);
			break;
		case AM_NUM:
			am_freedouble(tab);
			break;
		}
		tab->nodes = NULL;
	}
	else
		tr_Trace(1, 1, "tr_am_remove table %s no nodes found\n", table);
	/* free parameters if they are in the pool */
	tr_MemPoolFree(table);
	return (0);
}

/* return the number of element in the array */
int tr_am_count(char *table)
{
	struct am_tab *tab;

	if ((tab = find_table(table)) == NULL) {
		tr_Trace(1, 1, "tr_am_remove table %s not found\n", table);
		return (-1);
	}

	if (!tab->stack)
		return 0;
	/* free parameters if they are in the pool */
	tr_MemPoolFree(table);
	return tab->stack->count;
}

int tr_am_textremove(char *table)
{
	return (tr_am_remove(table));
}

int tr_am_numremove(char *table)
{
	return (tr_am_remove(table));
}

int tr_am_walknodes(char *table, int (*fcn)(), void *arg)
{
	struct am_tab *tab;
	struct am_walk_args warg;

	if ((tab = find_table(table)) == NULL)
		return (-1);

	if (tab->nodes) {
		INSTRUMENT(tab->walks++)
		warg.fcn = fcn;
		warg.arg = arg;
		return (am_walk(tab->nodes, &warg));
	}
	/* free parameters if they are in the pool */
	tr_MemPoolFree(table);
	return (0);
}

#ifdef STATISTIC_ON_ARRAY
static void tbl_stats(struct am_tab *tp)
{
	if (tp->left)
		tbl_stats(tp->left);

	printf("table %s: %d lookups, %d+%d+%d hits, %d+%d turns, %d walks\n",
		tp->name,
		tp->lookups,
		tp->cache_hits, tp->leftmost_hits, tp->ritemost_hits,
		tp->turns_left, tp->turns_rite,
		tp->walks);

	if (tp->rite)
		tbl_stats(tp->rite);
}

void tr_am_statistics()
{
	if (asstabs)
		tbl_stats(asstabs);
	else
		printf("No arrays were created\n");
	printf("%d table lookups, %d hits\n",
		table_lookups, table_cache_hits);
}
#endif


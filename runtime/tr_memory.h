/*============================================================================
MemIdx_... : override search table functions defined in tr_list.h
define an hashcoding table to index mempool.
We apply a mask to the pointer to get the index of the hast table.
Then the pointer and index are stored in a linked list with 2 root nodes, one for the used node 
and the other for free nodes.

SearchTable:
	tab[count ] : each element of the hashtable point a MainListTab which contains 16 Nodes

	The MainListTab maintains a list of free nodes and a list of used nodes
	When the list is full, a new ListTab is allocated and linked to the MainListTab
============================================================================*/
#define TR_CONST_MEMSLOTS 65536
#define TR_CONST_MEM_HASH_COUNT 4096
#define TR_CONST_MEM_HASH_SIZE 16
#define TR_CONST_MEM_HASH_MASK 0xFFF0

/* MemIdxNode define the index of a pointer */ 
typedef struct MemIdxNode_ {
	void *ptr;
	int index;
	struct MemIdxNode_ *next;
} MemIdxNode;

/* define a table of linked pointers : use to expand the MainListTab */
typedef struct MemIdxListTab_ {
	struct MemIdxListTab_ *prev;
	struct MemIdxListTab_ *next;
	MemIdxNode tab[TR_CONST_MEM_HASH_SIZE];
} MemIdxListTab;

/* define the main table of linked pointers */
typedef struct MemIdxMainListTab_ {
	MemIdxNode *LastUsed;
	MemIdxNode *FirstFree;
	struct MemIdxListTab_ *prev;
	struct MemIdxListTab_ *next;
	MemIdxNode tab[TR_CONST_MEM_HASH_SIZE];
} MemIdxMainListTab;

/* Redefine AbstractTabNode from tr_list.h */
typedef struct MemIdxTabNode_ {
	MemIdxMainListTab *nodes;  /* Pointer to the first list of node */
	int count;                 /* Number of pointer in the list */
} MemIdxTabNode;

#ifdef TR_MEMORY
void *memtabhash = NULL;
#else
extern void *memtabhash;
#endif

void MemIdx_InitList(MemIdxNode List[], MemIdxNode *prevNode);
int MemIdx_CreateNode(void **nodes);
int MemIdx_ClearNodes(void *iTabNode);
void MemIdx_SetInfo(void* ptr, int index, MemIdxNode *info);
int MemIdx_AddNode(void *iTabNode, void *iInfo);
int MemIdx_GetTabIdx(void *iInfo);
void* MemIdx_GetNode(void *iNodes, void *iInfo);
void* MemIdx_GetPrevNode(void *iTabNode, void *iInfo);

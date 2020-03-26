/*============================================================================

	File:		tr_list.h
	Programmer:	Jean-François CLAVIER

	Copyright (c) 1992-2011 Generix-group

	Interface for hash table list
============================================================================*/
/* CreateNodes: Create a table of nodes
   Parameters:
   - &nodes: address to return the allocated nodes.
   return the size of the table.
*/
typedef int (*pfct_CreateNodes)(void**);

/* ClearNodes: Clear a table of nodes
   Parameters:
   - iNodes: nodes table.
   return 0 on error, 1 if ok
*/
typedef int (*pfct_ClearNodes)(void*);

/* GetNodeIdx: get node info
   Parameters:
    - iNodes: nodes table.
    - iInfo: in pointer to look for
    return NULL if not found, or the pointer to the AbstractNode.
*/
typedef void* (*pfct_GetNode)(void*, void*);

/* GetTabIdx: return the hash table index from given info
   Parameters:
   - iInfo: pointer to the data
*/
typedef int (*pfct_GetTabIdx)(void*);

/* AddNode: Add node info in the table
   Parameters:
   - iTabNode: nodes table
   - iInfo: pointer to the data
   return 0 on error, 1 if ok
 */
typedef int (*pfct_AddNode)(void*, void*);

/* RemoveNode: Remove a node info in the table
   Parameters:
   - iTabNode: nodes table
   - iInfo: pointer to the data
   return 1 if successful 0 if not found
*/
typedef int (*pfct_RemoveNode)(void*, void*);

/* AbstractNode abstract information of a pointer */ 
typedef void* AbstractNode;

typedef struct AbstractTabNode_ {
	AbstractNode nodes;     /* Pointer to the first list of node */
	int count;              /* Number of pointer in the list */
} AbstractTabNode;

/* AbstractSearchTab : structure pointing on the hash tables */
typedef struct AbstractSearchTab_ {
	int count;                       /* hash tables number */
	int size;                        /* hash tables size */
	pfct_CreateNodes pCreateNodes;   /* function returning a table of nodes */
	pfct_ClearNodes pClearNodes;     /* function to clear a table of nodes */
	pfct_GetNode pGetNode;           /* function returning the index of a node for a given pointer */
	pfct_GetTabIdx pGetTabIdx;       /* function returning the hash index */
	pfct_AddNode pAddNode;           /* function to add a node */  
	pfct_RemoveNode pRemoveNode;     /* function to remove a node */  
	AbstractTabNode *tab;            /* First allocated tables */
} AbstractSearchTab;

#if !defined(TR_LIST)
void *tr_CreateSearchTable(int count, int size, pfct_CreateNodes pCreateNodes, pfct_ClearNodes pClearNodes, pfct_GetNode pGetNode, pfct_GetTabIdx pGetTabIdx, pfct_AddNode pAddNode, pfct_RemoveNode pRemoveNode);
void tr_SearchTableClear(AbstractSearchTab *hashtab);
void tr_SearchTableFree(AbstractSearchTab *hashtab);
void tr_SearchTableAdd(AbstractSearchTab *hashtab, AbstractNode iInfo);
void* tr_SearchTableFind(AbstractSearchTab *hashtab, AbstractNode iInfo);
int tr_SearchTableRemove(AbstractSearchTab *hashtab, AbstractNode ioInfo);
void tr_SearchDump(AbstractSearchTab *hashtab);
#endif


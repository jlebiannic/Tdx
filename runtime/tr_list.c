/*============================================================================

	File:		tr_list.c
	Programmer:	Jean-François CLAVIER

	Copyright (c) 1992-2011 Generix-group

============================================================================*/
#define TR_LIST
#define TR_MEM /* Do not redefine malloc functions */
#include "conf/local_config.h"
MODULE("@(#)TradeXpress $Id: tr_list.c 55433 2020-03-16 12:37:08Z sodifrance $")
/*============================================================================

	$Log:  $
	Revision 1.2.4.2  2011-05-31 09:43:47  jfranc
	Merge of Bugz 10092 : InitMemoryManager if need for call in C modules

	Revision 1.2.4.1  2011-04-15 13:51:15  jfranc
	Merge of Bugz 1219 : change error string

	Revision 1.2  2011-04-12 06:32:03  jfranc
	Bugz 1219 : Change debuglevel to tracelevel to avoid conflict with logsystem/util

	Revision 1.1  2011-04-11 09:53:02  jfranc
	Bugz 10092 : Add index on mem pool to get pointer quicker for remove.cf. Bugz 1219.

    Jira TX-3143 16.03.2020 - Olivier REMAUD - Passage au 64 bits


============================================================================*/
#include <stdio.h>
#include <string.h>

#include "tr_list.h"
#include "tr_privates.h"

#include <malloc.h>


/* tr_CreateHashTable :
   Allocate a table of "count" HashTabNode. 
   Each HashTabNode will point on a table of HashNode allocated on the first use.
*/
AbstractSearchTab *tr_CreateSearchTable(int count, int size, pfct_CreateNodes pCreateNodes, pfct_ClearNodes pClearNodes, pfct_GetNode pGetNode, pfct_GetTabIdx pGetTabIdx, pfct_AddNode pAddNode, pfct_RemoveNode pRemoveNode)
{
	AbstractSearchTab *result;

	result = malloc(sizeof(AbstractSearchTab));
	if (!result) {
		fprintf(stderr, "tr_CreateSearchTable (%d): *** MEMORY ALLOCATION ERROR! ***\n", __LINE__);
		exit(0);
	}
	result->count = count;
	result->size = size;
	result->pCreateNodes = pCreateNodes;
	result->pClearNodes = pClearNodes;
	result->pGetNode = pGetNode;
	result->pGetTabIdx = pGetTabIdx;
	result->pAddNode = pAddNode;
	result->pRemoveNode = pRemoveNode;
	result->tab = calloc(1, count * sizeof(AbstractTabNode));
	if (!result->tab) {
		fprintf(stderr, "tr_CreateSearchTable (%d): *** MEMORY ALLOCATION ERROR! ***\n", __LINE__);
		exit(0);
	}
	tr_Trace(3, 1, "(tr_CreateSearchTable %d) hashtab 0x%x count %d\n", __LINE__, result, result->count);
	return result;
}

void tr_SearchDump(AbstractSearchTab *hashtab)
{
	int i, j;
	AbstractTabNode *tabNode;
	char buf[256];

	if (!hashtab) {
		fprintf(stderr, "*** ERROR hashtab is null, missing init memory manager  ***\n");
		exit(1);
	}
	tr_Trace(1, 1, "*** Hash table memory used ***\n");
	buf[0] = '\0';
	j = 0;
	tr_Trace(1, 1, "hashtab 0x%x count %d\n", hashtab, hashtab->count);
	for (i = 0; i < hashtab->count; i++)
	{
		tabNode = &hashtab->tab[i];
		if (tracelevel >= 5)
			sprintf(buf,"%s\t%p", buf, tabNode);
		sprintf(buf,"%s\t%d", buf, tabNode->count);
		j++;
		if (j == 16) {
			j = 0;
			tr_Trace(1, 1, "%s\n", buf);
			buf[0] = '\0';
		}
	}
	if (strlen(buf))
		tr_Trace(1, 1, "%s\n", buf);
}

void tr_SearchTableClear(AbstractSearchTab *hashtab)
{
	int i;
	AbstractTabNode *tabNode;

	if (!hashtab) {
		fprintf(stderr, "*** ERROR hashtab is null, missing init memory manager  ***\n");
		exit(1);
	}
	if (tracelevel >= 1) 
		tr_SearchDump(hashtab);

	tr_Trace(1, 1, "tr_SearchTableClear hashtab 0x%x count %d\n", hashtab, hashtab->count);
	for (i = 0; i < hashtab->count; i++)
	{
		tabNode = &hashtab->tab[i];
		tr_Trace(3, 1, "tr_SearchTableClear tab[%d] 0x%x count %d nodes 0x%x\n", i, tabNode, tabNode->count, tabNode->nodes);
		if (tabNode->nodes) {
			if (!hashtab->pClearNodes(tabNode)) {
				fprintf(stderr, "*** ERROR could not clear hash table[%d] %p ***\n",  i, hashtab);
			}
		}
	}
}

void tr_SearchTableFree(AbstractSearchTab *hashtab)
{
	/* To Do : remove the object */
}

void tr_SearchTableAdd(AbstractSearchTab *hashtab, AbstractNode iInfo)
{
	int index;
	AbstractNode nodes;
	AbstractTabNode *tabNode;

	if (!hashtab) {
		fprintf(stderr, "*** ERROR hashtab is null, missing init memory manager  ***\n");
		exit(1);
	}
	tr_Trace(3, 1, "(tr_SearchTableAdd %d) hashtab 0x%x \n", __LINE__, hashtab);
	index = hashtab->pGetTabIdx(iInfo);
	tabNode = &hashtab->tab[index];
	nodes = tabNode->nodes;
	/* First allocation for this index : we create the table */
	if (!nodes) {
		hashtab->pCreateNodes(&tabNode->nodes);
		nodes = tabNode->nodes;
	}
	if (!nodes) {
		fprintf(stderr, "tr_SearchTableAdd (%d): *** MEMORY ALLOCATION ERROR! ***\n", __LINE__);
		if (tracelevel >= 1) {
			tr_Trace(1, 1, "(tr_SearchTableAdd %d) tabNode 0x%x count %d nodes 0x%x index %d\n", __LINE__, tabNode, tabNode->count, nodes, index);
			tr_SearchDump(hashtab);
		}
		exit(0);
	}
	tr_Trace(4, 1, "(tr_SearchTableAdd %d) tabNode 0x%x count %d nodes 0x%x index %d\n", __LINE__, tabNode, tabNode->count, nodes, index);
	if (hashtab->pAddNode(tabNode, iInfo) == 0) {
		/* memory allocation error */
		if (tracelevel >= 1) {
			tr_Trace(1, 1, "(tr_SearchTableAdd %d) tabNode 0x%x count %d nodes 0x%x index %d\n", __LINE__, tabNode, tabNode->count, nodes, index);
			tr_SearchDump(hashtab);
		}
		exit(0);
	}
}

void* tr_SearchTableFind(AbstractSearchTab *hashtab, AbstractNode iInfo)
{
	int index;
	AbstractNode found = NULL;
	AbstractTabNode *tabNode;

	if (!hashtab) {
		fprintf(stderr, "*** ERROR hashtab is null, missing init memory manager  ***\n");
		exit(1);
	}
	tr_Trace(3, 1, "(tr_SearchTableFind %d) hashtab 0x%x ", __LINE__, hashtab);
	index = hashtab->pGetTabIdx(iInfo);
	tabNode = &hashtab->tab[index];
	tr_Trace(4, 1, "(tr_SearchTableFind %d) tabNode 0x%x tabIdx %d\n", __LINE__, tabNode, index);
	if (tabNode->nodes) {
		found = hashtab->pGetNode(tabNode, iInfo);
	}
	return found;
}

int tr_SearchTableRemove(AbstractSearchTab *hashtab, AbstractNode ioInfo)
{
	int index, result;
	AbstractTabNode *tabNode;

	if (!hashtab) {
		fprintf(stderr, "*** ERROR hashtab is null, missing init memory manager  ***\n");
		exit(1);
	}
	index = hashtab->pGetTabIdx(ioInfo);
	tabNode = &hashtab->tab[index];
	if (tabNode && tabNode->nodes) {
		result = hashtab->pRemoveNode(tabNode, ioInfo);
	}
	else
		result = 0;
	tr_Trace(3, 1, "(tr_SearchTableRemove %d) hashtab 0x%x result %d\n", __LINE__, hashtab, result);
	return result;
}


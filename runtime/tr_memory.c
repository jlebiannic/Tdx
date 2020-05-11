/*============================================================================
	E D I S E R V E R   T R A N S L A T O R

	File:		tr_memory.c
	Programmer:	Mikko Valimaki (MV)
	Date:		Sat Oct 10 10:17:53 EET 1992

	Copyright (c) 1992 Telecom Finland/EDI Operations
==============================================================================
	V E R S I O N   C O N T R O L
============================================================================*/
#define TR_MEMORY
#include "conf/local_config.h"
MODULE("@(#)TradeXpress $Id: tr_memory.c 55484 2020-05-05 09:18:01Z sodifrance $")
/*LIBRARY(libruntime_version)
*/
/*============================================================================
  Record all changes here and update the above string accordingly.
  3.01 25.01.93/MV	tr_FreeElemList initializes all elements to TR_EMPTY.
  3.02 01.02.93/MV	Applied strdup for tr_ElemAssign in cases where the
			value does not come from the mempool.
  3.03 16.03.93/JN	Changed memory-allocators to use the tr_-allocation.
			Check against tTMP := tTMP; in tr_Assign().
  3.04 06.07.94/JN	Hardened ismalloced(), touched freexxx() functions.
			Removed excessive tests, tr_xxalloc never return NULL.
  3.05 06.02.95/JN	Removed excess #includes, tr_AdjustDS into other file.
  3.06 08.05.95/JN	tr_MemPoolPass() added.
  3.07 06.02.96/JN	ismalloced for windog.
  Bug 1219 08.04.11/JFC	remove ismalloced usage.
					new mempool management to avoid memory leak.
  28.08.2013/TCE(CG) Jira TX-2441 Pointer directly copied into another pointer
  20.09.2016/SCH(CG) Jira TX-2899 Cancellation of previous TX-2441 modification.
  Jira TX-3143 19.11.2019 - Olivier REMAUD - Passage au 64 bits
  Jira TX-3143 16.03.2020 - Olivier REMAUD - Passage au 64 bits
============================================================================*/

#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include <errno.h>
#ifndef MACHINE_WNT
#include <unistd.h>
#endif

#include "tr_externals.h"
#include "tr_list.h"
#include "tr_privates.h"
#include "tr_memory.h"

#include "tr_prototypes.h"

#ifdef MACHINE_WNT
#define vsnprintf _vsnprintf
#endif

#ifndef FINNISH_LANGUAGE

struct tr_errtable tr_errs[] =
{
/*  0 */ { "Too much data in the element",
									TRE_EXCESS_COMPONENTS },
/*  1 */ { "Truncated segment",
									TRE_TRUNCATED_SEGMENT },
/*  2 */ { "Too short: %d minimum",
									TRE_TOO_SHORT },
/*  3 */ { "Too many %s groups (%d), %d maximum",
									TRE_TOO_MANY_GROUPS },
/*  4 */ { "Too many %s segments (%d), %d maximum",
									TRE_TOO_MANY_SEGMENTS },
/*  5 */ { "Too long: %d maximum",
									TRE_TOO_LONG },
/*  6 */ { "Too few groups %s (%d), %d minimum",
									TRE_TOO_FEW_GROUPS },
/*  7 */ { "Too few segments %s (%d), %d minimum",
									TRE_TOO_FEW_SEGMENTS },
/*  8 */ { "Cannot find character set %s.",
									TRE_CHARSET_NOT_FOUND },
/*  9 */ { "Out of memory: attempted to allocate %d bytes, already %d bytes previously allocated",
									0 },
/* 10 */ { "Unexpected end of file",
									TRE_UNEXPECTED_EOF },
/* 11 */ { "Missing element",
									TRE_MISSING_ELEMENT },
/* 12 */ { "Missing component",
									TRE_MISSING_COMPONENT },
/* 13 */ { "Missing mandatory element",
									TRE_MISSING_ELEMENT },
/* 14 */ { "Missing mandatory component",
									TRE_MISSING_COMPONENT },
/* 15 */ { "Too little data requested",
									TRE_REQUEST_UNDERRUN },
/* 16 */ { "Out of messages",
									0 },
/* 17 */ { "Segment '%s' does not belong to the message",
									TRE_NO_PLACE_FOR_SEGMENT },
/* 18 */ { "Excess data in the segment",
									TRE_EXCESS_DATA_IN_SEGMENT },
/* 19 */ { "[This space intentionally left blank]",
									0 },
/* 20 */ { "Cannot output data",
									TRE_IO_ERROR },
/* 21 */ { "Unknown data type: '%c' (\\0%o)",
									TRE_UNKNOWN_DATATYPE },
/* 22 */ { "Corrupted character set %s",
									TRE_CORRUPT_CHARSET },
/* 23 */ { "Message contains errors",
									TRE_MESSAGE_WITH_ERROR },
/* 24 */ { "Segment contains errors",
									TRE_SEGMENT_WITH_ERROR },
/* 25 */ { "Invalid type %s field%s",
									TRE_ELEMENT_WITH_ERROR },
/* 26 */ { "Invalid type %s field: '%c' (\\0%o)",
									TRE_ELEMENT_WITH_ERROR },
/* 27 */ { "Error opening '%s': %d",
									TRE_IO_ERROR },
/* 28 */ { "Error reading '%s': %d",
									TRE_IO_ERROR },
/* 29 */ { "Extraneous data before message thrown away",
									TRE_JUNK_OUTSIDE_MESSAGE },
/* 30 */ { "Segment %s inside the message",
									TRE_JUNK_INSIDE_MESSAGE },
/* 31 */ { "Segment %s overflowed",
									TRE_SEGMENT_OVERFLOW },
/* 32 */ { "Group %s overflowed",
									TRE_GROUP_OVERFLOW },
/* 33 */ { "%s",
									TRE_XERCES_MESSAGE }
};

#else

struct tr_errtable tr_errs[] =
{
/*  0 */ "Elementissa liikaa dataa",
									TRE_EXCESS_COMPONENTS,
/*  1 */ "Keskenjaanyt segmentti",
									TRE_TRUNCATED_SEGMENT,
/*  2 */ "Liian lyhyt: %d minimi",
									TRE_TOO_SHORT,
/*  3 */ "Liian monta %s ryhmaa (%d), %d maksimi",
									TRE_TOO_MANY_GROUPS,
/*  4 */ "Liian monta %s segmenttia (%d), %d maksimi",
									TRE_TOO_MANY_SEGMENTS,
/*  5 */ "Liian pitka: %d maksimi",
									TRE_TOO_LONG,
/*  6 */ "Liian vahan ryhmia %s (%d), %d minimi",
									TRE_TOO_FEW_GROUPS,
/*  7 */ "Liian vahan segmentteja %s (%d), %d minimi",
									TRE_TOO_FEW_SEGMENTS,
/*  8 */ "Merkistoa %s ei loytynyt.",
									TRE_CHARSET_NOT_FOUND,
/*  9 */ "Muisti loppui: yritettiin allokoida %d tavua, allokoituna oli jo %d tavua",
									0,
/* 10 */ "Odottamaton tiedoston loppu",
									TRE_UNEXPECTED_EOF,
/* 11 */ "Puuttuva elementti",
									TRE_MISSING_ELEMENT,
/* 12 */ "Puuttuva komponentti",
									TRE_MISSING_COMPONENT,
/* 13 */ "Puuttuva pakollinen elementti",
									TRE_MISSING_ELEMENT,
/* 14 */ "Puuttuva pakollinen komponentti",
									TRE_MISSING_COMPONENT,
/* 15 */ "Pyydettiin vaillinainen maara dataa",
									TRE_REQUEST_UNDERRUN,
/* 16 */ "Sanomat loppuivat",
									0,
/* 17 */ "Segmentille '%s' ei paikkaa",
									TRE_NO_PLACE_FOR_SEGMENT,
/* 18 */ "Segmentissa liikaa dataa",
									TRE_EXCESS_DATA_IN_SEGMENT,
/* 19 */ "[This space intentionally left blank]",
									0,
/* 20 */ "Tulostus ei onnistu",
									TRE_IO_ERROR,
/* 21 */ "Tuntematon datatyyppi: '%c' (\\0%o)",
									TRE_UNKNOWN_DATATYPE,
/* 22 */ "Vaarankokoinen merkisto %s",
									TRE_CORRUPT_CHARSET,
/* 23 */ "Viallinen sanoma",
									TRE_MESSAGE_WITH_ERROR,
/* 24 */ "Viallinen segmentti",
									TRE_SEGMENT_WITH_ERROR,
/* 25 */ "Viallinen tyypin %s kentta%s",
									TRE_ELEMENT_WITH_ERROR,
/* 26 */ "Viallinen tyypin %s kentta: '%c' (\\0%o)",
									TRE_ELEMENT_WITH_ERROR,
/* 27 */ "Virhe avattaessa '%s': %d",
									TRE_IO_ERROR,
/* 28 */ "Virhe luettaessa '%s': %d",
									TRE_IO_ERROR,
/* 29 */ "Ylimaarainen tieto ennen sanomaa hylatty",
									TRE_JUNK_OUTSIDE_MESSAGE,
/* 30 */ "Segmentti %s sanoman sisalla",
									TRE_JUNK_INSIDE_MESSAGE,
/* 31 */ "Segmentti %s ei mahtunut",
									TRE_SEGMENT_OVERFLOW,
/* 32 */ "Ryhma %s ei mahtunut",
									TRE_GROUP_OVERFLOW,
/* 33 */ "%s",
									TRE_XERCES_MESSAGE,
};
#endif

#if defined(DEBUG) && MEM_DEBUG==1
char *TDXMemDebug_tr_strdup(char *s, int line_strdup, char *file_strdup);
void *TDXMemDebug_tr_malloc(int size, int line_malloc, char *file_malloc);
void *TDXMemDebug_tr_calloc(int n, int size, int line_malloc, char *file_malloc);
void TDXMemDebug_tr_free(void *p, int line_free, char *file_free);
void *TDXMemDebug_tr_realloc(void *p, int size, int line_realloc, char *file_realloc);
char *TDXMemDebug_tr_strndup(char *s, int len, int line_strdup, char *file_strdup);
void *TDXMemDebug_tr_zalloc(int size, int line_malloc, char *file_malloc);
#endif


static char **memtab = NULL;
static int  memptr = 0;
static int  memslots = 0;

/* MemIdx_InitList :
   Initialise a bloc for memory allocation
*/
void MemIdx_InitList(MemIdxNode List[], MemIdxNode *prevNode)
{
	size_t i;

	tr_Trace(1, 1, "*** Init memory bloc %d *** List %p\n", TR_CONST_MEM_HASH_SIZE, List);
	for (i = 0; i < TR_CONST_MEM_HASH_SIZE-1; i++) {
		List[i].ptr = NULL;
		List[i].next = &List[i+1];
	}
	List[i].ptr = NULL;
	List[i].next = NULL;
	if (prevNode){
		prevNode->next = &List[0];
	}
}

/* MemIdx_dump: dump list */
void MemIdx_dump(void *iTabNode)
{
	MemIdxTabNode *tabNode = (MemIdxTabNode*)iTabNode;
	MemIdxNode *node;

	if (tracelevel >= 4) {
		tr_Trace(4, 1, "*** dump of used addresses *** Count %d\n\n", tabNode->count);
		node = tabNode->nodes->LastUsed;
		while (node) {
			tr_Trace(4, 1, "node %p ptr %p next %p Index %d\n", node, node->ptr, node->next, node->index);
			node = node->next;
		}
	}
	if (tracelevel >= 5) {
		tr_Trace(5, 1, "*** dump of free addresses *** Count %d\n\n", tabNode->count);
		node = tabNode->nodes->FirstFree;
		while (node) {
			tr_Trace(5, 1, "node %p alloc %p next %p Index %d\n", node, node->ptr, node->next, node->index);
			node = node->next;
		}
	}
}

/* MemIdx_CreateNode: create a table of nodes. content of a hash table */
int MemIdx_CreateNode(void **nodes)
{
	MemIdxMainListTab *nodesTab;

	*nodes = calloc(1, sizeof(MemIdxMainListTab));
	nodesTab = *nodes;
	tr_Trace(3, 1, "(MemIdx_CreateNode %d) nodes %p size %d prev %p next %p\n", __LINE__, 
				*nodes, sizeof(MemIdxMainListTab), &nodesTab->prev, &nodesTab->next);
	MemIdx_InitList(&nodesTab->tab[0], NULL);
	nodesTab->FirstFree = &nodesTab->tab[0];
	tr_Trace(3, 1, "(MemIdx_CreateNode %d) nodes %p FirstFree %p\n", __LINE__, *nodes, nodesTab->FirstFree);
	return sizeof(MemIdxMainListTab);
}

/* MemIdx_SetInfo: Set node info */
void MemIdx_SetInfo(void* ptr, int index, MemIdxNode *info)
{
	info->ptr = ptr;
	info->index = index;
}

void MemIdx_CopyInfo(MemIdxNode *iInfo, MemIdxNode *oInfo)
{
	oInfo->ptr = iInfo->ptr;
	oInfo->index = iInfo->index;
}

/* MemIdx_ClearNodes: Clear a list of node */
int MemIdx_ClearNodes(void *iTabNode)
{
	MemIdxTabNode *tabNode = (MemIdxTabNode*)iTabNode;
	MemIdxMainListTab *nodesTab = tabNode->nodes;

	tr_Trace(3, 1, "(MemIdx_ClearNodes %d) count %d\n", __LINE__, tabNode->count);
	if (nodesTab) {
		MemIdxNode *node = nodesTab->LastUsed;
		if (node) {
			while (node->next) {
				node = node->next;
			}
			node->next = nodesTab->FirstFree;
			nodesTab->FirstFree = nodesTab->LastUsed;
			nodesTab->LastUsed = NULL;
		}
		return 1;
	}
	else {
		return 0;
	}
}

/* MemIdx_AddNode: Add and set a node in a hash table */
int MemIdx_AddNode(void *iTabNode, void *iInfo)
{
	MemIdxTabNode *tabNode = (MemIdxTabNode*)iTabNode;
	MemIdxNode *info = (MemIdxNode*)iInfo, *result;
	MemIdxListTab *rootTab = NULL, *newTab;

	tr_Trace(3, 1, "(MemIdx_AddNode %d) count %d ptr %p memIndex %d nodes %p FirstFree %p\n", __LINE__, 
			tabNode->count, info->ptr, info->index, tabNode->nodes, tabNode->nodes->FirstFree);
	if (!tabNode || !tabNode->nodes) {
		fprintf(stderr, "*** ERROR memtabnode is null, missing call to CreateNodes  ***\n");
		return 0;
	}
	MemIdx_dump(tabNode);
	if (tabNode->nodes->FirstFree == NULL) {
		tr_Trace(3, 1, "(MemIdx_AddNode %d) New bloc, count %d ptr %p memIndex %d\n", __LINE__, tabNode->count, info->ptr, info->index);
		rootTab = (MemIdxListTab *)&tabNode->nodes->prev;  /* rootTab point the first list of nodes */
		newTab = tr_calloc(1, sizeof(MemIdxListTab));
		if (!newTab) {
			fprintf(stderr,"*** MEMORY ALLOCATION ERROR! ***\n");
			return 0;
		}
		if (rootTab->prev) {
			rootTab->prev->next = newTab;  /* Linked the previous last extension to the new one */
			newTab->prev = rootTab->prev;  /* Linked the new extension to the last one */
		}
		else {                             /* first extension : the root next & prev are linked to the extension */
			rootTab->next = newTab;
			newTab->prev = rootTab;
		}
		rootTab->prev = newTab;
		MemIdx_InitList(&newTab->tab[0], NULL);
		tabNode->nodes->FirstFree = &newTab->tab[0];
	}

	tabNode->count++;
	result = tabNode->nodes->FirstFree;
	result->ptr = info->ptr;
	result->index = info->index;
	tabNode->nodes->FirstFree = result->next;
	result->next = tabNode->nodes->LastUsed;
	tabNode->nodes->LastUsed = result;
	tr_Trace(3, 1, "(MemIdx_AddNode %d) count %d ptr %p memIndex %d FirstFree %p\n", __LINE__, 
			 tabNode->count, info->ptr, info->index, tabNode->nodes->FirstFree);
	MemIdx_dump(tabNode);
	return 1;
}

/* MemIdx_RemoveNode: Remove a node from the list */
int MemIdx_RemoveNode(void *iTabNode, void *ioInfo)
{
	MemIdxTabNode *tabNode = (MemIdxTabNode*)iTabNode;
	MemIdxNode *info = (MemIdxNode*)ioInfo;
	MemIdxNode *node, *result = NULL;

	tr_Trace(3, 1, "MemIdx_RemoveNode %d ptr %p last used  %p\n", __LINE__, info->ptr, 
			tabNode->nodes->LastUsed ? tabNode->nodes->LastUsed->ptr : NULL);
	if (!tabNode->nodes->LastUsed){
		return 0;  /* No node used */
	}
	if (tabNode->nodes->LastUsed->ptr == info->ptr) {  /* we remove the last used node */
		result = tabNode->nodes->LastUsed;
		tabNode->nodes->LastUsed = result->next;
	}
	else {
		node = MemIdx_GetPrevNode(tabNode, info);
		if (node) {
			tr_Trace(3, 1, "MemIdx_RemoveNode %d ptr %p memindex %d \n", __LINE__, info->ptr, info->index);
			result = node->next;
			node->next = node->next->next;  /* point to the next used node */
		}
	}
	if (result) {
		MemIdx_CopyInfo(result, info);
		/* add the node to the list of free pointer */
		result->ptr = NULL;
		result->index = -1;
		result->next = tabNode->nodes->FirstFree;
		tabNode->nodes->FirstFree = result;
		tabNode->count--;
		return 1;
	}
	tr_Trace(3, 1, "MemIdx_RemoveNode %d ptr %p not found \n", __LINE__, info->ptr);
	return 0;
}

/* MemIdx_GetTabIdx: return the hash table to use using the pointer and a hash key */
int MemIdx_GetTabIdx(void *iInfo)
{
	MemIdxNode *info = (MemIdxNode*)iInfo;

	int result = (int)((long)info->ptr & (long)TR_CONST_MEM_HASH_MASK) / 16;
	tr_Trace(3, 1, "MemIdx_GetTabIdx %d ptr %p mask 0x%x result  %d\n", __LINE__, info->ptr, TR_CONST_MEM_HASH_MASK, result);
	return result;
}

/* MemIdx_GetNode: return the node in the list */
void* MemIdx_GetNode(void *iTabNode, void *iInfo)
{
	MemIdxTabNode *tabNode = (MemIdxTabNode*)iTabNode;
	MemIdxNode *info = (MemIdxNode*)iInfo;
	MemIdxNode *result = NULL;

	if (info && tabNode->nodes) {
		for (result = tabNode->nodes->LastUsed; result && result->ptr != info->ptr; result = result->next){;}
	}
	tr_Trace(3, 1, "MemIdx_GetNode %d result %p\n", __LINE__, result);
	return result;
}

/* MemIdx_GetPrevNode: return the previous node in the list */
void* MemIdx_GetPrevNode(void *iTabNode, void *iInfo)
{
	MemIdxTabNode *tabNode = (MemIdxTabNode*)iTabNode;
	MemIdxNode *info = (MemIdxNode*)iInfo;
	MemIdxNode *result = NULL, *next = NULL;

	if (info && tabNode->nodes) {
		for (next = tabNode->nodes->LastUsed; next && next->ptr != info->ptr; next = next->next) {
			result = next;
		}
	}
	tr_Trace(3, 1, "MemIdx_GetPrevNode %d result %p\n", __LINE__, next ? result : NULL);
	if (next){
		return result;
	}
	return NULL;
}

/*============================================================================
	This is called in the very beginning...

	Create an initial list for the mempool,
	and initialize heap and stack region addresses.

	We get an address of some stack variable from main.
	This initializes the stackbase.
	Stack limit is updated every time in ismalloced().
============================================================================*/

void tr_InitMemoryManager (void)
{
	tr_Inittrace();

	memslots = TR_CONST_MEMSLOTS;
	memtab = (char **) tr_malloc (memslots * sizeof (char *));
	memtabhash = tr_CreateSearchTable(TR_CONST_MEM_HASH_COUNT,
										  TR_CONST_MEM_HASH_SIZE,
										  MemIdx_CreateNode,
										  MemIdx_ClearNodes,
										  MemIdx_GetNode,
										  MemIdx_GetTabIdx,
										  MemIdx_AddNode,
										  MemIdx_RemoveNode);
	tr_Trace(2, 1, "tr_InitMemoryManager RTEline %d memtabhash %p\n", tr_sourceLine, memtabhash);
	memptr = 0;
}

/*============================================================================
  Make a copy of string and return it.
  Save the copy in our private list, these saved strings are
  freed in garbagecollect.
============================================================================*/
char *tr_MemPool (char *s)
{
	MemIdxNode info;

	if (!memtab) {
		fprintf(stderr, "*** WARNING memtab is null, missing call to tr_InitMemoryManager ***\n");
		tr_InitMemoryManager();
	}
	if ((s == NULL) || (s == TR_EMPTY)){
		return TR_EMPTY;
	}

	if (memptr == memslots) {
		/*
		**  Have to allocate more memory slots.
		*/
		memslots += TR_CONST_MEMSLOTS;
#if defined(DEBUG) && MEM_DEBUG==1
		memtab = (char **) TDXMemDebug_tr_realloc (memtab, memslots * sizeof (char *),__LINE__,__FILE__);
#else
		memtab = (char **) tr_realloc (memtab, memslots * sizeof (char *));
#endif
	}

#if defined(DEBUG) && MEM_DEBUG==1
	memtab[memptr] = TDXMemDebug_tr_strdup(s,__LINE__,__FILE__);
#else
	memtab[memptr] = tr_strdup (s);
#endif
	MemIdx_SetInfo(memtab[memptr], memptr, &info);
	tr_SearchTableAdd(memtabhash, &info);
	tr_Trace(2, 1, "tr_MemPool RTEline %d ptr %p result[%d] %p\n", tr_sourceLine, s, memptr, memtab[memptr]);

	return memtab[memptr++];
}

/*============================================================================
  Remove a pointer from the memory pool and free it.
============================================================================*/
int tr_MemPoolRemove(char *s, int withFree)
{
	int i;
	MemIdxNode info;

	if (!memtab) {
		fprintf(stderr, "*** WARNING memtab is null, missing call to tr_InitMemoryManager ***\n");
		tr_InitMemoryManager();
	}
	if (!s) {
		if (withFree || (tr_errorLevel > 2)){
			fprintf(stderr, "*** WARNING line %d: trying to remove a NULL pointer from memory pool ***\n", tr_sourceLine);
		}
		return 0;
	}
	if (s == TR_EMPTY) {
		return 0;
	}
	/* check if the pointer is in the pool */
	info.ptr = s;
	i = tr_SearchTableRemove(memtabhash, &info);
	tr_Trace(2, 1, "tr_MemPoolRemove RTEline %d memptr %p memindex %d with free %d\n", tr_sourceLine, s, i ? info.index : -1, withFree);
	if (i > 0) {
		i = info.index;
		if (withFree) {
			tr_free (memtab[i]);
		}
		memtab[i] = NULL;
		while (memptr > 0 && memtab[memptr-1] == NULL){
			memptr--;
		}
		return 1;
	}
	return 0;
}
/*============================================================================
  Remove a pointer from the memory pool and free it.
============================================================================*/
int tr_MemPoolFree(char *s)
{
	return tr_MemPoolRemove(s, 1);
}

/*============================================================================
  tr_MemPoolPass: Like tr_MemPool function, but given string is already strdupped.
============================================================================*/
char *tr_MemPoolPass (char *s)
{
	MemIdxNode info;

	if (!memtab) {
		fprintf(stderr, "*** WARNING memtab is null, missing call to tr_InitMemoryManager ***\n");
		tr_InitMemoryManager();
	}
	if ((s == NULL) || (s == TR_EMPTY)){
		return TR_EMPTY;
	}

	if (memptr == memslots) {
		/*
		**  Have to allocate more memory slots.
		*/
		memslots += TR_CONST_MEMSLOTS;
#if defined(DEBUG) && MEM_DEBUG==1
		memtab = (char **) TDXMemDebug_tr_realloc (memtab, memslots * sizeof (char *),__LINE__,__FILE__);
#else
		memtab = (char **) tr_realloc (memtab, memslots * sizeof (char *));
#endif
	}

	tr_Trace(2, 1, "tr_MemPoolPass RTEline %d memptr %p\n", tr_sourceLine, s);
	MemIdx_SetInfo(s, memptr, &info);
	tr_SearchTableAdd(memtabhash, &info);
	memtab[memptr++] = s;

	return s;
}
/*============================================================================
	Release the unused memory blocks.
============================================================================*/
void tr_GarbageCollection (void)
{
	int i;

	tr_Trace(2, 1, "tr_GarbageCollection RTEline %d \n", tr_sourceLine);

	if (!memtab) {
		fprintf(stderr, "*** WARNING memtab is null, missing call to tr_InitMemoryManager ***\n");
		tr_InitMemoryManager();
	}
	if (tracelevel >= 1) {
		tr_SearchDump(memtabhash);
	}
	for (i = 0; i < memptr; ++i) {

		if (memtab[i]) {
			tr_Trace(2, 1, "tr_GarbageCollection RTEline %d memptr %p\n", tr_sourceLine, memtab[i]);
			tr_free (memtab[i]);
			memtab[i] = NULL;
		}
	}
	tr_SearchTableClear(memtabhash);
	memptr = 0;
}

/*============================================================================
  tr_LastMemPoolValue:  return the last value in the memory pool
============================================================================*/
char *tr_LastMemPoolValue (void) {
	if (memptr){
		return memtab[memptr-1];
	}
	else{
		return NULL;
	}
}

/*============================================================================
  tr_GetFunctionMemPtr:  return the index of the last value in the memory pool
============================================================================*/
int tr_GetFunctionMemPtr(void) {
	return memptr;
}

/*============================================================================
  tr_CollectFunctionGarbage: release pointer in mempool from the given index
  this is used in RTE functions to release blocs allocated by the function
============================================================================*/
void tr_CollectFunctionGarbage (int from)
{
	int i;
	int f;
	MemIdxNode info;

	f = 0;

	tr_Trace(2, 1, "tr_CollectFunctionGarbage RTEline %d from %d memptr %d\n", tr_sourceLine, from, memptr);

	if (!memtab) {
		fprintf(stderr, "*** WARNING memtab is null, missing call to tr_InitMemoryManager ***\n");
		tr_InitMemoryManager();
	}
	if(from < 0 || from >= memptr){
		return;
	}

	for (i = from; i < memptr; ++i) {

		if (memtab[i]) {
			info.ptr = memtab[i];
			tr_SearchTableRemove(memtabhash, &info);
			tr_Trace(2, 1, "tr_CollectFunctionGarbage RTEline %d memptr %p\n", tr_sourceLine, memtab[i]);
			tr_free (memtab[i]);
			memtab[i] = NULL;
		} else {
			f = 1;	/* why ? */
		}
	}
	if(f == 0 && i == memptr){
		memptr = from;
	}
}

/*============================================================================
============================================================================*/
char * tr_ShowSep (unsigned char sepchar)
{
	char tmp[2];

	if (!sepchar){
		return TR_EMPTY;
	}
	
	tmp[0] = sepchar;
	tmp[1] = '\0';
	return tr_MemPool (tmp);
}

/*============================================================================
	tr_Assign: Assign a memory bloc to a variable.

	In RTE, value are allways allocated with strdup.
	The only exception si TR_EMPTY used at initialization or by tr_AssignEmpty.

	Do nothing if we are assigning a variable into itself.

	If the previous value was set, then release it.

	If the current value comes from the memory pool,
	then mark it to be NULL there, so that it will not
	be released in the next garbage collection.
	Otherwise make a copy and assign it.
============================================================================*/
void tr_Assign (char **var, char *value)
{
	if (*var != value) {
		tr_Trace(2, 1, "tr_Assign %p '%s' %p '%s'\n", *var, *var ? *var : "", value, value ? value : "null");
		if (*var && !(*var == TR_EMPTY)) {
#if defined(DEBUG) && MEM_DEBUG==1
			TDXMemDebug_tr_free(*var,__LINE__,__FILE__);
#else
			tr_free (*var);
#endif
			*var = NULL;
		}

		if (value == tr_LastMemPoolValue()) {
			*var = value;
			tr_MemPoolRemove(value, 0);
		} else {
#if defined(DEBUG) && MEM_DEBUG==1
			*var = TDXMemDebug_tr_strdup(value,__LINE__,__FILE__);
#else
			*var = tr_strdup (value);
#endif
		}
	}
#if defined(DEBUG) && MEM_DEBUG==1
	else
		tr_Trace(2, 1, "tr_Assign %p '%s' %p '%s'\n", *var, *var ? *var : "null", value, value ? value : "null");
#endif
}

/*============================================================================
	tr_AssignEmpty : used to free local variable in RTE functions.
============================================================================*/
void tr_AssignEmpty (char **var)
{
	char *old = *var;

	*var = TR_EMPTY;

	tr_Trace(2, 1, "tr_AssignEmpty RTEline %d memptr &%p %p\n", tr_sourceLine, var, var ? *var : 0);
	if (old && !(old == TR_EMPTY)) {
#if defined(DEBUG) && MEM_DEBUG==1
		TDXMemDebug_tr_free(old,__LINE__,__FILE__);
#else
		tr_free (old);
#endif
		old = NULL;
	}
}

/*============================================================================
	tr_ElemAssign : assign a value to an element in a segment structure.
	if this value was allocated in the pool, it is removed from the pool.
============================================================================*/
void tr_ElemAssign (char **var, char *value, int content)
{
	char *p;

	switch (content) {
	case 'N':
	case 'R':
		/* if Decimal separator used in application file is defined and 
			  present in the input string                               */
		if (*tr_ADS && (p = strchr (value, *tr_ADS))) {
			/* of Primary Decimal separator used in EDI-msg is defined  */
			if (*tr_MDS){
				/* Replace tr_ADS by tr_MDS in the input string         */
				/* TX-2899 SCH(CG) Cancellation of TX-2441 modification.*/
				p[0] = tr_MDS[0];
				/*END TX-2899*/
				}
			else{
				tr_Log (TR_WARN_NO_MDS_SPECIFIED);
			}
		}
		/* Flow through here for the assignment */
	default:
		if (value == tr_LastMemPoolValue()) {
#if defined(DEBUG) && MEM_DEBUG==1
			tr_Trace(1, 1, "tr_ElemAssign RTEline %d memptr %d %p, %s\n", tr_sourceLine, tr_GetFunctionMemPtr(), 
					tr_LastMemPoolValue(), value ? value : "null");
#endif
			*var = value;
			tr_MemPoolRemove(value, 0);
		} else {
#if defined(DEBUG) && MEM_DEBUG==1
			*var = TDXMemDebug_tr_strdup(value,__LINE__,__FILE__);
#else
			*var = tr_strdup (value);
#endif
		}
	}
}

/*======================================================================
tr_FreeElemList : clear the elements in a segment structure.
======================================================================*/
void tr_FreeElemList (char **list, int count)
{
	/*
	 * Subtract one from the count,
	 * for some reason.
	 *
	 * Then, check list and free dynamic stuff from it.
	 */
	--count;

	while (--count >= 0) {
		if (*list && *list != TR_EMPTY) {
#if defined(DEBUG) && MEM_DEBUG==1
			TDXMemDebug_tr_free(*list,__LINE__,__FILE__);
#else
			tr_free (*list);
#endif
		}

		*list++ = TR_EMPTY;
	}
}

char * tr_strdup(char *s)
{
#if defined(DEBUG) && MEM_DEBUG==1
	return TDXMemDebug_tr_strdup(s, __LINE__, __FILE__);
#else
	void *result;

	if (s == TR_EMPTY){
		return TR_EMPTY;
	}
	result = (void *) strdup(s);
	if (errno == ENOMEM) {
		tr_OutOfMemory();  /* log error and exit */
	}
	return result;
#endif
}

#if defined(DEBUG) && MEM_DEBUG==1
char *TDXMemDebug_tr_strdup(char *s, int line_strdup, char *file_strdup)
{
	if (s == TR_EMPTY){
		return TR_EMPTY;
	}
	char *result = strdup(s);

	if (tracelevel >= 1){
		fprintf(stderr,"%30s L%d (TDXMemDebug_tr_strdup L%d) %p -> %p\n",file_strdup,line_strdup, __LINE__, s, result);
	}
	return result;
}
#endif

char * tr_strndup(char *s, int len)
{
#if defined(DEBUG) && MEM_DEBUG==1
	if (s == TR_EMPTY){
		return TR_EMPTY;
	}
	return TDXMemDebug_tr_strndup(s, len, __LINE__, __FILE__);
#else
	void *p;

	if (s == TR_EMPTY){
		return TR_EMPTY;
	}

	p = (void *) tr_zalloc(len + 1);
	if (p){
		memcpy(p, s, len);
	}
	return (p);
#endif
}

#if defined(DEBUG) && MEM_DEBUG==1
char *TDXMemDebug_tr_strndup(char *s, int len, int line_strdup, char *file_strdup)
{
	if (s == TR_EMPTY){
		return TR_EMPTY;
	}
	char *result = strndup(s, len);
	if (tracelevel >= 1){
		fprintf(stderr,"%30s L%d (TDXMemDebug_tr_strndup L%d) %p -> %p\n",file_strdup,line_strdup, __LINE__, s, result);
	}
	return result;
}
#endif

char *tr_strvadup(char *s, ...)
{
	va_list ap;
	char *p;
	int len = 1;

	for (va_start(ap, s); s; s = va_arg(ap, char *)){
		len += strlen(s);
	}
	va_end(ap);
#if defined(DEBUG) && MEM_DEBUG==1
	p = TDXMemDebug_tr_malloc(len,__LINE__,__FILE__);
#else
	p = tr_malloc(len);
#endif
	p[0] = 0;
	for (va_start(ap, s); s; s = va_arg(ap, char *)){
		strcat(p, s);
	}
	va_end(ap);
	return p;
}

void * tr_malloc(int size)
{
#if defined(DEBUG) && MEM_DEBUG==1
	return TDXMemDebug_tr_malloc(size, __LINE__, __FILE__);
#else
	void *result = (void *) malloc(size);
	if (errno == ENOMEM) {
		tr_OutOfMemory();  /* log error and exit */
	}
	return result;
#endif
}

#if defined(DEBUG) && MEM_DEBUG==1
void *TDXMemDebug_tr_malloc(int size, int line_malloc, char *file_malloc)
{
	void *result = malloc(size);
	if (tracelevel >= 1)
		fprintf(stderr,"%30s L%d (TDXMemDebug_tr_malloc L%d size %d) RTE %d %p\n", file_malloc, line_malloc, __LINE__, size, tr_sourceLine, result);
	return result;
}
#endif

void * tr_zalloc(int size)
{
#if defined(DEBUG) && MEM_DEBUG==1
	return TDXMemDebug_tr_zalloc(size, __LINE__, __FILE__);
#else
	void *result = calloc(1, size);
	if (errno == ENOMEM) {
		tr_OutOfMemory();  /* log error and exit */
	}
	return result;
#endif
}

#if defined(DEBUG) && MEM_DEBUG==1
void *TDXMemDebug_tr_zalloc(int size, int line_malloc, char *file_malloc)
{
	void *result = calloc(1, size);
    if (errno == ENOMEM) {
        tr_OutOfMemory();  /* log error and exit */
    }
	if (tracelevel >= 1)
		fprintf(stderr,"%30s L%d (TDXMemDebug_tr_zalloc L%d size %d) RTE %d %p\n", file_malloc, line_malloc, __LINE__, size, tr_sourceLine, result);
	return result;
}
#endif

void * tr_calloc(int n, int size)
{
#if defined(DEBUG) && MEM_DEBUG==1
	return TDXMemDebug_tr_zalloc(n * size, __LINE__, __FILE__);
#else
	void *result = (void *) calloc(n, size);
	if (errno == ENOMEM) {
		tr_OutOfMemory();  /* log error and exit */
	}
	return result;
#endif
}

#if defined(DEBUG) && MEM_DEBUG==1
void *TDXMemDebug_tr_calloc(int n, int size, int line_malloc, char *file_malloc)
{
	void *result = calloc(n, size);
	if (tracelevel >= 1)
		fprintf(stderr,"%30s L%d (TDXMemDebug_tr_calloc L%d size %d) RTE %d %p\n", file_malloc, line_malloc, __LINE__, n*size, tr_sourceLine, result);
	return result;
}
#endif

void * tr_realloc(void *p, int size)
{
#if defined(DEBUG) && MEM_DEBUG==1
	return TDXMemDebug_tr_realloc(p, size, __LINE__, __FILE__);
#else
	void *result = p ? (void *) realloc(p, size) : (void *) malloc(size);
	if (errno == ENOMEM) {
		tr_OutOfMemory();  /* log error and exit */
	}

	return result;
#endif
}

#if defined(DEBUG) && MEM_DEBUG==1
void *TDXMemDebug_tr_realloc(void *p, int size, int line_realloc, char *file_realloc)
{
	void *result = NULL;
	if (p != NULL) {
		result = realloc(p, size);  
		if (tracelevel >= 1)
			fprintf(stderr,"%30s L%d (TDXMemDebug_tr_realloc (realloc) L%d size %d) RTE %d %p\n", file_realloc, line_realloc, __LINE__, size, tr_sourceLine, result);
	} else {
		result = malloc(size);
		if (tracelevel >= 1)
			fprintf(stderr,"%30s L%d (TDXMemDebug_tr_realloc (malloc) L%d size %d) RTE %d %p\n", file_realloc, line_realloc, __LINE__, size, tr_sourceLine, result);
	}
	return result;
}
#endif

void tr_free(void *p)
{
	if (p == TR_EMPTY){
		return;
	}
#if defined(DEBUG) && MEM_DEBUG==1
	TDXMemDebug_tr_free(p, __LINE__, __FILE__);
#else /* not DEBUG */
	tr_Trace(1, 1, "tr_free RTE %d %p\n", tr_sourceLine, p);
	if (!p){
		return;
	}
	free(p);
#endif /* not DEBUG */
}

#if defined(DEBUG) && MEM_DEBUG==1
void TDXMemDebug_tr_free(void *p, int line_free, char *file_free)
{
	if (p == TR_EMPTY){
		return;
	}
	if (tracelevel >= 1) {
		fprintf(stderr,"%30s L%d (TDXMemDebug_tr_free L%d) RTE %d %p\n", file_free, line_free, __LINE__, tr_sourceLine, p);
	}
	free(p);
}
#endif

/*
 *	allokoiva sprintf,
 *	HUOMAA buf:n koko-rajoitus, ei tarkisteta...
 */

char * tr_savprintf(char *fmt, va_list ap)
{
	va_list ap_copy;

	char buf[8192];
	int size, resultsize;
	char *result = NULL;

	va_copy( ap_copy, ap);
	resultsize = vsnprintf (buf, sizeof(buf), fmt, ap_copy);
	va_end(ap_copy);

	if (resultsize == -1) { /* For Windows */
		va_copy( ap_copy, ap);
		resultsize = tr_getBufSize(8192, fmt, ap_copy);
		va_end(ap_copy);
	}
	if (resultsize >= sizeof(buf)) {
		size = resultsize;
		tr_Trace(1, 1, "tr_saprintf : size = %d\n", size);
		result = tr_zalloc(size+1);
		if (!result) {
			tr_OutOfMemory();  /* raise an exception */
		}
		vsprintf(result, fmt, ap);
	}

	if (result){
		return result;
	}
#if defined(DEBUG) && MEM_DEBUG==1
	return TDXMemDebug_tr_strdup(buf,__LINE__,__FILE__);
#else
	return tr_strdup(buf);
#endif
}


char * tr_saprintf(char *fmt, ...)
{
	va_list ap;
	char *result = NULL;

	va_start(ap, fmt);
	result = tr_savprintf(fmt, ap);
	va_end(ap);

	return result;
}

void tr_berzerk(char *fmt, ...)
{
	va_list ap;

	fflush(stdout);
	fprintf(stderr, "\007");
	fflush(stderr);
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	fflush(stderr);

#ifdef MACHINE_WNT
	*(int *) 0 = 0;
#endif
	abort();
}


#undef DEBUG
#undef MEM_DEBUG

/*======================================================================
	E D I S E R V E R   T R A N S L A T O R

	File:		tr_pack.c
	Programmer:	Mikko Valimaki (MV)
	Date:		Fri Oct  9 12:21:26 EET 1992
======================================================================*/
#include "conf/local_config.h"
MODULE("@(#)TradeXpress $Id: tr_pack.c 47429 2013-10-29 15:27:44Z cdemory $")
/*LIBRARY(libruntime_version)
*/
/*======================================================================
  Record all changes here and update the above string accordingly.
  3.01 21.07.93/MV	Added facility to use runtime segment location.
  3.02 05.12.94/JN	Added a call to bfDependencyError when such an
			error is detected.
  3.03 06.02.95/JN	Removed excess #includes
  3.04 24.02.95/JN	tr_CopySegment removed, not needed anymore.
			tr_segAddress update to FindRunTimeSegment.
  3.05 15.05.95/JN	Separated tr_packElems to another file.
  4.00 21.09.99/JH	EDIFACT 4 segment and group level dependency checks.
=======================================================================*/

#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "tr_strings.h"
#include "tr_externals.h"
#include "tr_privates.h"
#include "tr_prototypes.h"
#include "tr_bottom.h"

extern MessageNode tr_msgNodes[];
extern MessageNode *tr_segAddress;

#if 0
/*
 *  This function is no longer needed,
 *  data-arrays are included in
 *  the msgNodes.
 */
/*======================================================================
  Copies the pointers from global segment tray to the segment's
  own table.
======================================================================*/
tr_CopySegment (char **list, int count)
{
	int i;

	/*
	**  Copy the element pointers.
	*/
	for (i = 1; i < count; i++) {
		 /* PK 8.9 */
                if (list[i - 1] && list[i - 1] != TR_EMPTY)
                        tr_free(list[i - 1]);
		if (*tr_segTable[i])
			list[i - 1] = tr_strdup (tr_segTable[i]);
		else
			list[i - 1] = TR_EMPTY;
	}
}
#endif

/*======================================================================
  4.00/JH parameter type added to make it possible to write different
          warnings for message level dependecies.
======================================================================*/
void tr_DependencyError (char *name, char *dep, int type)
{
	/*
	**  MV: ADD HERE SOME CONTROL FOR THE USER!
	**      OR USE THE RELAX OPTIONS.
	**  JN: okokok.
	*/
	if (bfDependencyError(name, dep)) {
		/*
		 * Usersupplied function says also that it's
		 * a dependency-error alright.
		 * We have permission to go for the default action
		 * which is currently only a warning-message.
		 */
		/* 4.00/JH */
		if (type)
			tr_Log (TR_ERR_MSG_DEPENDENCY_ERROR, name, dep);
		else
			tr_Log (TR_ERR_DEPENDENCY_ERROR, name, dep);
	}
}

/*======================================================================
  4.00/JH EDIFACT 4 message level dependencies.
======================================================================*/
void tr_checkmsgdep ()
{
	bfCheckmsgdep();
}

/*======================================================================
	R U N T I M E   S E G M E N T   L O C A T I O N
======================================================================*/
#define N_NODETAB_DEPTH	256

/*
**  This static table is used in parsing
**  the group specifications.
*/
typedef struct {
	char *name;
	int repetition;
} GroupList;
static GroupList grpTab[N_NODETAB_DEPTH + 1];

static int tr_ParseGroupList (char *grpList, char *segName);
static int tr_ParseGroup (char *grpSpec, GroupList *grpTab, char *segName);


/*======================================================================
  This routine is part of the runtime library and the TCL compiler
  generates these function calls to code where ever runtime segment
  location facility is used.
======================================================================*/
int tr_FindRunTimeSegment (char **data, char *segName, int segRepetition, char *grpList)
{
	MessageNode *sp;	/* A pointer to the segment structure */
	MessageNode *segNode;
	MessageNode *grpNode;
	int enclosingGrp;	/* Index to parsed group list table */
	char *grpName;		/* Short cut name for innermost group */
	int grpRepetition;	/* Short cut repetition for innermost group */
	/*
	**  The "short cut" here means that the innermost group is always
	**  used and thus it is clear to use designated variables.  Those
	**  values are stored in the parsed group list table.
	**  The parsed group list table (grpTab) needs to referenced only
	**  when there are several enclosing groups.
	*/

#ifdef DEBUG
fprintf (stderr, "Calling tr_FindRunTimeSegment(0x%x, %s, %d, %s)...\n", data, segName, segRepetition, grpList);
#endif
	sp = tr_msgNodes;
	/*
	**  See if the group list contains anything.
	**  "!*grpList" is at least satisfied with EMPTY.
	*/
	if (!grpList || !*grpList) {
#ifdef DEBUG
fprintf (stderr, "No group name.\n");
#endif
		while (sp->name) {
			if ((ISSEGMENT(sp)) && !strcmp (segName, sp->name) && sp->level == 0) {
#ifdef DEBUG
fprintf (stderr, "Found segment (level %d, node %d)\n", sp->level, sp - tr_msgNodes);
#endif
				segNode = sp;
				/*
				**  Make the call.
				*/
#ifdef DEBUG
fprintf (stderr, "Making call tr_pushsegmentbyaddr (0x%x, 0x%x, %d)\n\n", data, segNode, segRepetition);
#endif
				tr_segAddress = segNode;
				return tr_pushsegmentbyaddr (data, segNode, segRepetition);
			}
			sp++;
		}
		tr_Log (TR_ERR_NO_SUCH_SEGMENT, segName, "levels 0 and 1");
		return 1;
	}

	/*
	**  Parse the group list into a table.  If the routine
	**  returns a negative value there has been an error.
	**  An appropriate error message has been written
	**  already earlier, so we just return.
	*/
	if ((enclosingGrp = tr_ParseGroupList (grpList, segName)) < 0)
		return 1;
	grpName = grpTab[enclosingGrp].name;
	grpRepetition = grpTab[enclosingGrp].repetition;

#ifdef DEBUG
fprintf (stderr, "Looking for group \"%s\"...(index enclosingGrp is %d)\n", grpName, enclosingGrp);
#endif
	while (sp->name) {
		if ((ISGROUP(sp)) && !strcmp (grpName, sp->name)) {
#ifdef DEBUG
fprintf (stderr, "Found group (level %d, node %d)...\n", sp->level, sp - tr_msgNodes);
#endif
			/*
			**  Save the group node and decrement the
			**  pointer to point to the first segment
			**  in the group.
			*/
			grpNode = sp--;
			/*
			**  Find the segment from the group.
			*/
			if (grpNode->next) {
				/*
				**  There are more segments/groups on
				**  this level.
				*/
				while (sp >= (MessageNode *)grpNode->next) {
					if (ISSEGMENT(sp)) {
						if ((sp->level == grpNode->level + 1 ||
						    sp->level == grpNode->level) && 
						    !strcmp (segName, sp->name)) {
#ifdef DEBUG
fprintf (stderr, "...and segment (level %d, node %d)\n", sp->level, sp - tr_msgNodes);
#endif
							segNode = sp;
							break;
						}
					}
					sp--;
				}
			} else {
				/*
				**  There are no more segments/groups on
				**  this level.  It suffices to check just
				**  the segments immediately following the
				**  group node.
				*/
				while (sp >= tr_msgNodes && ISSEGMENT(sp) && sp->level >= grpNode->level) {
					if (!strcmp (segName, sp->name)) {
						segNode = sp;
#ifdef DEBUG
fprintf (stderr, "...and segment (level %d, node %d)\n", sp->level, sp - tr_msgNodes);
#endif
						break;
					}
					sp--;
				}
			}
			if (!segNode) {
				tr_Log (TR_ERR_NO_SUCH_SEGMENT, segName, grpName);
				return 1;
			}
			/*
			**  Now the segment and its closest enclosing group have
			**  been found.  Check to see if the group is contained
			**  in any other groups.
			*/
			if (grpNode->level > 1) {
				MessageNode *tmpNode = grpNode;
				NodeLocation nodeTab[N_NODETAB_DEPTH + 1];
				int nodeIndex = N_NODETAB_DEPTH;

				/*
				**  Insert the segment and the closest
				**  group to the bottom of the node table.
				*/
				nodeTab[nodeIndex].addr = segNode;
				nodeTab[nodeIndex--].repetition = segRepetition;
				nodeTab[nodeIndex].addr = grpNode;
				nodeTab[nodeIndex--].repetition = grpRepetition;

				sp = grpNode + 1;
				while (sp->name && tmpNode->level > 1) {
					if (ISGROUP(sp) && sp->level < tmpNode->level) {
						/*
						**  The variable enclosing group is an index
						**  to the grpTab.  Originally its value is the
						**  index to the innermost group.  If that index
						**  is numerically greater than 0 it spells that
						**  there are more than one group specified and
						**  all specified ones have to match the actual
						**  groups.
						**  We test "if (enclosingGrp--)" to see what was
						**  originally, then decrement since the first
						**  value has been used already.
						**  
						*/
#ifdef DEBUG
fprintf (stderr, "enclosingGrp = %d\n", enclosingGrp);
#endif
						nodeTab[nodeIndex].addr = sp;
						if (enclosingGrp-- > 0) {
							if (strcmp (grpTab[enclosingGrp].name, sp->name)) {
								tr_Log (TR_ERR_GRPLIST_CONFLICT, segName, grpTab[enclosingGrp].name, sp->name);
								return 1;
							}
							nodeTab[nodeIndex--].repetition = grpTab[enclosingGrp].repetition;
						} else
							nodeTab[nodeIndex--].repetition = -1;
						/*
						**  In case that some hero somewhere (most
						**  likely in India) decides to create a
						**  message containing more than 256 nested
						**  groups here is a check for that.
						*/
						if (nodeIndex == 0) {
							tr_Log (TR_ERR_TOO_DEEP_NESTING, segName, N_NODETAB_DEPTH);
							return 1;
						}
						tmpNode = sp;
					}
					sp++;
				}
#ifdef DEBUG
{
	int i;
	fprintf (stderr, "Making call tr_pushsegmentbyaddrtab (0x%x, &nodeTab[%d + 1])\n", data, nodeIndex);
	for (i = nodeIndex + 1; i <= N_NODETAB_DEPTH; i++)
		fprintf (stderr, "\tnodeTab[%d] = {0x%x, %d}\n", i, nodeTab[i].addr, nodeTab[i].repetition);
	fprintf (stderr, "\n");
}
#endif
				tr_segAddress = segNode;
				return tr_pushsegmentbyaddrtab (data, &nodeTab[nodeIndex + 1]);
			} else {
				/*
				**  Only one containing group.
				*/
#ifdef DEBUG
	fprintf (stderr, "Making call tr_pushsegmentbyaddr (0x%x, 0x%x, -1, 0x%x, -1)\n\n", data, grpNode, segNode);
#endif
				tr_segAddress = segNode;
				return tr_pushsegmentbyaddr (data, grpNode, grpRepetition, segNode, segRepetition);
			}
			break;
		}
		sp++;
	}
	tr_Log (TR_ERR_NO_SUCH_SEGMENT, segName, grpName);
	return 1;
}

/*======================================================================
  This routine parses the group list given in text format.
  Different groups are separated by commas and they are
  assumed to be in order.
  The list looks like the samples below:

	"7=1,12=2"
	"7,12"
	"12"
	"7=2,12"

  No white spaces are tolerated.  If there later is desire
  after Elina's helpless cry to do so, use routines used
  to implement TCL function "strip".

  This function returns the index in the locally global
  group table.  In error conditions a negative value is
  returned.
======================================================================*/
static int tr_ParseGroupList (char *grpList, char *segName)
{
	/*
	**  Take the work copy through "tr_strip",
	**  which is used to take away all white
	**  spaces.
	**  tr_strip uses memory pool and thus we
	**  do not have to worry about freeing the
	**  copy.
	*/
	char *workCopy = tr_strip (grpList, " 	");
	char *tmp;
	int i = 0;

	if (strchr (workCopy, ',')) {
		if (tmp = strtok (workCopy, ",")) {
			do {
				if (i > N_NODETAB_DEPTH) {
					tr_Log (TR_ERR_TOO_DEEP_NESTING, segName, N_NODETAB_DEPTH);
					return -1;
				}
				if (tr_ParseGroup (tmp, &grpTab[i++], segName))
					return -1;
			} while (tmp = strtok (NULL, ","));
			i--;	/* Points now to last actual entry */
		} else {
			tr_Log (TR_ERR_INVALID_GROUP_LIST, segName, grpList);
			return -1;
		}
	} else
		/*
		**  If there is an error below, return
		**  just the error code.
		*/
		if (tr_ParseGroup (workCopy, &grpTab[0], segName))
			return -1;

#ifdef DEBUG
{
	int j;
	fprintf (stderr, "\nGroup list \"%s\" has been parsed to:\n", grpList);
	for (j = i; j >= 0; j--)
		fprintf (stderr, "[%d] %s : %d\n", j, grpTab[j].name,  grpTab[j].repetition);
	fprintf (stderr, "\n");
}
#endif
	/*
	**  Return the index of the last group in the
	**  group table.
	*/
	return i;
}

/*======================================================================
  This routine parses individual groups.  Group names are given
  in this runtime format without the leading 'g'.  They may or
  may not contain an equal sign followed by the repetition
  information.  If no repetition is specified, the value becomes
  -1, which in lower level routines means: "first available slot".
======================================================================*/
static int tr_ParseGroup (char *grpSpec, GroupList *grpTab, char *segName)
{
	char *p;
	char *q;

	if (p = strchr (grpSpec, '=')) {
		/*
		**  Repetition information specified.
		*/
		*p = (char)0;
		grpTab->name = grpSpec;
		if (*(p + 1)) {
			/*
			**  There is something.  Check that
			**  it is a valid number.
			*/
			q = p + 1;
			while (*q) {
				if (!isdigit(*q) && *q != '+' && *q != '-') {
					/*
					**  Non zero return code indicates error.
					*/
					tr_Log (TR_ERR_NON_NUMERIC_REPETITION, segName, grpSpec, p + 1);
					return 1;
				}
				q++;
			}
			grpTab->repetition = atoi (p + 1);
		} else {
			/*
			**  This exception is of form "7=".
			**  We forgive that and take it to
			**  mean "next available" i.e. -1.
			*/
			grpTab->repetition = -1;
		}
	} else {
		/*
		**  No repetition specification.
		*/
		grpTab->name = grpSpec;
		grpTab->repetition = -1;
	}
	return 0;
}

/*======================================================================
	
	  4.00/JH

	This routine is used to check EDIFACT 4 dependency rules for segments,
	goups and segments inside groups. The routine is cooked from
	tr_FindRunTimeSegment which is also in this same file. The calls
	for tr_pushsegmentbyaddr has been removed from this code.
======================================================================*/
int tr_CheckMsgDep (char **data, char *segName, char *grpList)
{
	MessageNode *sp;	/* A pointer to the segment structure */
	MessageNode *segNode = NULL;
	MessageNode *grpNode;
	int enclosingGrp;	/* Index to parsed group list table */
	char *grpName;		/* Short cut name for innermost group */
	int grpRepetition;	/* Short cut repetition for innermost group */
	int i;	/* JH */

/* #define DEBUG 1 */

	/*
	**  The "short cut" here means that the innermost group is always
	**  used and thus it is clear to use designated variables.  Those
	**  values are stored in the parsed group list table.
	**  The parsed group list table (grpTab) needs to referenced only
	**  when there are several enclosing groups.
	*/
#ifdef DEBUG
	fprintf (stderr, "Calling tr_CheckMsgDep(0x%x, %s, %s)...\n",
		 data, segName, grpList);
#endif

	sp = tr_msgNodes;
	/*
	**  See if the group list contains anything.
	**  "!*grpList" is at least satisfied with EMPTY.
	*/
	if (!grpList || !*grpList) {

#ifdef DEBUG
		fprintf (stderr, "No group name.\n");
#endif
		while (sp->name) {
			if ((ISSEGMENT(sp)) && !strcmp (segName, sp->name) && sp->level == 0) {

#ifdef DEBUG
				fprintf (stderr, "Found segment (level %d, node %d, occurrence %d)\n",
					sp->level, sp - tr_msgNodes, sp->occurrence);
#endif
				return(sp->occurrence?1:0);
			}
			sp++;
		}
		if (data != NULL)
			tr_Log (TR_ERR_NO_SUCH_SEGMENT, segName, "levels 0 and 1");
		return 0;
	}

	/*
	**  Parse the group list into a table.  If the routine
	**  returns a negative value there has been an error.
	**  An appropriate error message has been written
	**  already earlier, so we just return.
	*/
	if ((enclosingGrp = tr_ParseGroupList (grpList, segName)) < 0)
		return 0;
	grpName = grpTab[enclosingGrp].name;
	grpRepetition = grpTab[enclosingGrp].repetition;

#ifdef DEBUG
	fprintf (stderr, "Looking for group \"%s\"...(index enclosingGrp is %d)\n",
	grpName, enclosingGrp);
#endif

	while (sp->name) {
		if ((ISGROUP(sp)) && !strcmp (grpName, sp->name)) {

#ifdef DEBUG
			fprintf (stderr, "Found group (level %d, node %d)...\n",
				sp->level, sp - tr_msgNodes);
#endif

			/*
			 * If we were only looking for a group then return now.
			 */
			if (data == NULL)
				return (1);

			/*
			**  Save the group node and decrement the
			**  pointer to point to the first segment
			**  in the group.
			*/
			grpNode = sp--;
			/*
			**  Find the segment from the group.
			*/
			if (grpNode->next) {
				/*
				**  There are more segments/groups on
				**  this level.
				*/
				while (sp >= (MessageNode *)grpNode->next) {
					if (ISSEGMENT(sp)) {
						if ((sp->level == grpNode->level + 1 ||
						    sp->level == grpNode->level) && 
						    !strcmp (segName, sp->name)) {
#ifdef DEBUG
							fprintf (stderr, "...and segment (level %d, node %d, occurrence %d)\n",
								sp->level, sp - tr_msgNodes, sp->occurrence);
#endif
							/*segNode = sp;*/
							return(sp->occurrence?1:0);
						}
					}
					sp--;
				}
			} else {
				/*
				**  There are no more segments/groups on
				**  this level.  It suffices to check just
				**  the segments immediately following the
				**  group node.
				*/
				while (sp >= tr_msgNodes && ISSEGMENT(sp) && sp->level >= grpNode->level) {
					if (!strcmp (segName, sp->name)) {
						segNode = sp;
#ifdef DEBUG
						fprintf (stderr, "...and segment (level %d, node %d, occurrence %d)\n",
						sp->level, sp - tr_msgNodes, sp->occurrence); 
#endif
						return(sp->occurrence?1:0);
					}
					sp--;
				}
			}
			if (!segNode) {
				if (data)
					tr_Log (TR_ERR_NO_SUCH_SEGMENT, segName, "Z"); /*grpName);*/
				return 0;
			}
			break;
		}
		sp++;
	}
	if (data)
		tr_Log (TR_ERR_NO_SUCH_SEGMENT, segName, "X"); /*grpName);*/

	return 0;
}



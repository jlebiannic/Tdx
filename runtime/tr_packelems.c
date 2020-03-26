/*======================================================================
	E D I S E R V E R   T R A N S L A T O R

	File:		tr_packelems.c
	Programmer:	Mikko Valimaki (MV)
	Date:		Fri Oct  9 12:21:26 EET 1992
======================================================================*/
#include "conf/config.h"
MODULE("@(#)TradeXpress $Id: tr_packelems.c 47429 2013-10-29 15:27:44Z cdemory $")
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
  3.06 15.05.95/JN	Separated the routine from another file, tr_pack.c
			Also fixed a smallish leak when strings
			containing blanks where not free'ed.
  3.07 14.05.96/JN	incorporated 3.1.1 optional el/comp packing.
=======================================================================*/

#include <stdio.h>
#include <string.h>

#include "tr_strings.h"
#include "tr_externals.h"
#include "tr_privates.h"
#include "tr_prototypes.h"

/*======================================================================
  Strings in array `elements' are packed to beginning,
  trailing entries are set to TR_EMPTY.

  start   - the index of the starting element,
  tail    - the index of the last element which is to be included.
  size    - the count of strings which are to be kept as one.
  count   - the number of string-groups.
  is_elem - is this an element packing (component packing if false)

  size is 1 when we are packing composites inside a single element
  and when packing simple elements.
  Otherwise, size gives the count of composites in complex element.

  Example:
  Pack the 3 elements C213.n in segment GID
  -----------------------------------------

	0:  1496

	1:  C213.1.7224
	2:  C213.1.7065
	3:  C213.1.1131
	4:  C213.1.3055
	5:  C213.1.7064

	6:  C213.1.7224
	7:  C213.1.7065
	8:  C213.1.1131
	9:  C213.1.3055
	10: C213.1.7064

	11: C213.1.7224
	12: C213.1.7065
	13: C213.1.1131
	14: C213.1.3055
	15: C213.1.7064

  tr_PackElems(tr_GID,1,11,5,3, 1);


  Another example:
  Pack the elements C058, C080 and C059 in segment NAD
  ----------------------------------------------------

         0: 3035
         1: C082.3039
         2: C082.1131
         3: C082.3055

         4: C058.3124.1
         5: C058.3124.2
         6: C058.3124.3
         7: C058.3124.4
         8: C058.3124.5

         9: C080.3036.1
        10: C080.3036.2
        11: C080.3036.3

        12: C059.3042.1
        13: C059.3042.2
        14: C059.3042.3

        15: 3164
        16: 3229
        17: 3251
        18: 3207

    tr_PackElems(tr_NAD, 4, 8,1,5, 0);
    tr_PackElems(tr_NAD, 9,11,1,3, 0);
    tr_PackElems(tr_NAD,12,14,1,3, 0);


======================================================================*/
void tr_PackElems(char **elements, int start, int tail, int size, int count, int is_elem)
{
	int i;
	extern int tr_doPackElements;   /* these are from ... */
	extern int tr_doPackComponents; /* ... tclrc. */

	/*
	 *  First, check if we are prohibited of doing this packing.
	 *  Continue as long as there is more than one group.
	 */
	if (is_elem) {
		if (!tr_doPackElements)
			return;
	} else {
		if (!tr_doPackComponents)
			return;
	}
	while (--count > 0) {
		/*
		**  Check if all individual strings
		**  in this group are "empty".
		*/
		for (i = start; i < start + size; i++) {
			if (!tr_isempty (elements[i])) {
				break;
			}
		}
		if (i < start + size) {
			/*
			 * sized group contained something,
			 * and thus cannot be squeezed out.
			 * Continue with next group.
			 */
			start += size;
			continue;
		}
		/*
		**  Empty group - do packing.
		**
		**  Free memory in first group.
		*/
		for (i = start; i < start + size; i++) {
			if (elements[i]
			 && elements[i] != TR_EMPTY)
				tr_free(elements[i]);
		}
		/*
		**  Move all the remaining groups up,
		**  zeroing the pointers in the last portion.
		*/
		for ( ; i < tail + size; i++) {

			elements[i - size] = elements[i];
			elements[i] = TR_EMPTY;
		}
	}
}


/*============================================================================
	E D I S E R V E R   T R A N S L A T O R

	File:		tr_amcopy.c
	Programmer:	Mikko Valimaki (MV)
	Date:		Sat Oct 10 11:46:01 EET 1992
			Mon Jan 11 21:33:57 EET 1993

	Copyright (c) 1992 Telecom Finland/EDI Operations
============================================================================*/
#include "conf/local_config.h"
MODULE("@(#)TradeXpress $Id: tr_amcopy.c 55433 2020-03-16 12:37:08Z sodifrance $")
/*LIBRARY(libruntime_version)
*/
/*============================================================================
  3.00 11.01.93/MV	Created.
  3.01 13.02.96/JN	Change in tr_arrays, reason to fix sloppiness here.
  4.00	04.08.99/JH	tr_amelemcopy added.
  Jira TX-3143 24.09.2019 - Olivier REMAUD - Passage au 64 bits
  Jira TX-3143 16.03.2020 - Olivier REMAUD - Passage au 64 bits
============================================================================*/
#ifndef MACHINE_WNT
#include <strings.h>
#include <string.h>
#endif

#include "tr_externals.h"
#include "tr_privates.h"
#include "tr_prototypes.h"

/*============================================================================
============================================================================*/
int tr_amtextcopy(char *to, char *from)
{
	struct am_node *walker;
	char *idx, *val;

	tr_am_textremove(to);

	walker = tr_am_rewind(from);

	while (tr_amtext_getpair(&walker, &idx, &val))
		tr_am_textset(to, idx, val);

	/*
	**  Set the separators in case this was a load.
	*/
	tr_am_textset(taFORMSEPS, to, tr_am_textget(taFORMSEPS, from));
    return 0;
}

/*============================================================================
============================================================================*/
int tr_amnumcopy(char *to, char *from)
{
	struct am_node *walker;
	char *idx;
	double val;

	tr_am_numremove(to);

	walker = tr_am_rewind(from);

	while (tr_amnum_getpair(&walker, &idx, &val))
		tr_am_numset(to, idx, val);
    return 0;
}

/*============================================================================

  4.00/JH

============================================================================*/
#include "../bottom/tr_edifact.h"
int tr_amelemcopy(char *to, char *firstelem, char **element, int index, 
  double syntax, double composite, double component, void *segAddr)
{
	register int i, j;
	char arrname[30];	/* array to hold generic component names for C770.9424 */
	/*int ncomposite = 0, ncomponent = 0, n;*/
	int n;

	/* Handle to runtime segment */
	/*extern MessageNode *tr_segAddress;
	extern double tr_arrComposites;
	extern double tr_arrComponents;*/
#if 0
	/* Handle to the list of Element structs */
	Element *eleminfo = (Element*) ((MessageNode*)segAddr)->data;
#endif

	tr_am_textremove(to);

	if(segAddr) {
		switch((int)syntax) {
		/*case SYNTAX_EDIFACT:*/
		case SYNTAX_EDIFACT4:
			/*
			 * Copying ARR composites and components
			 */
			if(!strcmp(firstelem, "C770.9424")) {
				/*ncomposite = (int)tr_arrComposites;
				ncomponent = (int)tr_arrComponents;*/
				for(i = 0; i < composite; i++) {
					for(j = 0; j < component; j++) {
						n = index + i * composite + j;
						if (element[n]) {
							sprintf(arrname, "C770.%d.9424.%d", i+1, j+1);
							tr_am_textset(to, arrname, element[n]);

							/*fprintf(stderr, "tr_amcopy.c: tr_amelemcopy: infoname %s = %s %d\n", arrname,
								element[n], n);
							fflush(stderr);*/
						}
					}
				}
			}
#if 0
			/*
			 * Copying repeating composites that start with C or S
			 */
			else if(firstelem[0] == 'C' || firstelem[0] == 'S') {
				/* First element goes here */
				tr_am_textset(to, eleminfo[index].name, element[index]);
				for(i = index+1; i < segAddr->nels; i++) {
					if( !strncmp(firstelem, eleminfo[i].name, 9)) {
						tr_am_textset(to, eleminfo[i].name, element[i]);
						//fprintf(stderr, "infoname %d x %s = %s\n", i, eleminfo[i].name,element[i]);
					}
					else	/* No sense to check the rest of the elements */
						break;
				}
			}
			/*
			 * Copying repeating simple data elements
			 */
			else {
				/* First element goes here */
				tr_am_textset(to, eleminfo[index].name, element[index]);
				for(i = index+1; i < segAddr->nels; i++) {
					if( !strncmp(firstelem, eleminfo[i].name, 4)) {
						tr_am_textset(to, eleminfo[i].name, element[i]);
						//fprintf(stderr, "infoname %d x %s = %s\n", i, eleminfo[i].name,element[i]);
					}
					else	/* No sense to check the rest of the elements */
						break;
				}
			}
#endif

			break;
		}
	}
    return 0;
}

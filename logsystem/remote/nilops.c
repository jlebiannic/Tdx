/*========================================================================
:set ts=4 sw=4

	E D I S E R V E R

	File:		logsystem/nilops.c
	Author:		Juha Nurmela (JN)
	Date:		Fri Jan 13 13:13:13 EET 1994

	Copyright (c) 1997 Telecom Finland/EDI Operations

	Below stubs are written to always fail.

	'type'
		Whenever a 'type' is returned, it is 
		one of _String, _Time etc.

	replies
		Stubs that contain a call to reply or vreply,
		HAVE to reply. Silent stubs CANNOT reply. 

	'error'
		Generally 0 if all is well.

	'idx'
		Generally 0 on error.

========================================================================*/
#include "conf/local_config.h"
MODULE("@(#)TradeXpress $Id: nilops.c 47371 2013-10-21 13:58:37Z cdemory $")
/*========================================================================
  Record all changes here and update the above string accordingly.
  3.08 06.10.97/JN	Dummy.
========================================================================*/

#ifdef MACHINE_WNT
#include <windows.h>
#endif
#include <sys/types.h>
#include <sys/stat.h>
#ifndef MACHINE_WNT
#include <sys/time.h>
#endif
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <signal.h>
#include <stdarg.h>
#include <string.h>
#include <stdio.h>

#include "perr.h"
#include "bases.h"

/*
 *  Always called when a client opens a base.
 *  p->base->internal are valid.
 *  The base has been opened with open_base() already.
 */
static void
nil_init_client(Client *p)
{
	return;
}

/*
 *  Called when a client closes or is closed.
 *  Necessary cleanup etc. for data which is not visible
 *  to generic parts.
 *  p->base->internal are valid.
 */
static void
nil_cleanup_client(Client *p)
{
#if 0
	sample action:

	if (p->curfilter)
		logfilter_free(p->curfilter);
#endif
}

/*
 *  Called every now and then, when there is nothing to do.
 *  For disk updates etc. for example.
 *  Can be empty.
 */
static void
nil_periodic_sync(void *base_internal)
{
}

/*
 *  Open for the first time.
 *
 *  NULL & EFTYPE  - Not for us, base seems to exist but is not our type.
 *  NULL  other    - An error.
 *  other          - The internal structure (client->base->internal)
 */
static void *
nil_open_base(char *sysname)
{
	errno = ENOENT;
	return (NULL);
}

/*
 *  Close for real, after nobody has this open any more.
 *  internal is the client->base->internal.
 */
static void
nil_close_base(void *base_internal)
{
}

/*
 *  Return 0 if IO- or protocol-error (client terminated).
 *  void fns can set client->dead.
 */

/*
 *  void
 *
 *  int version
 *  int maxcount
 *  int numused
 *  int nfields
 *  int 0
 */

static void
nil_get_base_info(Client *p)
{
	vreply(p, RESPONSE, 0,
		_Int, 0,
		_Int, 0,
		_Int, 0,
		_Int, 0,
		_Int, 0,
		_End);
}

/*
 *  string field-name
 *
 *  fieldtype
 *  fieldsize
 *  fieldformat
 *  fieldheader
 */
static void
nil_tell_named_field(Client *p, char *fieldname)
{
	vreply(p, RESPONSE, 0,
		_Int, 0,
		_Int, 0,
		_End);
}

/*
 *  int field-index (in label)
 *
 *  fieldtype
 *  fieldsize
 *  fieldname
 *  fieldformat
 *  fieldheader
 */
/*
 *  int field-index
 *  (in label, client starts from 0 and continues until we return 0 type)
 *
 *  fieldname/fieldtype/fieldsize
 */

static void
nil_tell_next_field(Client *p, int fldnum, int how)
{
	switch (how) {
	default:
	case TELL_FIELD_TYPE:
	case TELL_FIELD_SIZE:
		vreply(p, RESPONSE, 0,
			_Int, 0,
			_End);
		break;
	case TELL_FIELD_NAME:
		vreply(p, RESPONSE, 0,
			_String, "",
			_End);
		break;
	}
}

/*
 *  void
 *
 *  no ack.
 */
static void
nil_clear_filter(Client *p)
{
}

/*
 *  int order, -n/0/+n
 *
 *  no ack.
 */
static void
nil_set_filter_order(Client *p, int order)
{
}

/*
 *  string keyname
 *
 *  no ack.
 */
static void
nil_set_filter_key(Client *p, char *keyname)
{
}

/*
 *  string filter
 *
 *  no ack.
 */
static void
nil_add_filter(Client *p, char *filterstring)
{
}

/*
 *  void
 *
 *  int keytype
 */
static void
nil_compile_filter(Client *p)
{
	p->walkidx = 1;

	vreply(p, RESPONSE, 0,
		_Int, 0,
		_End);
}

/*
 *  void
 *
 *  int  index
 */
static void
nil_get_next_match(Client *p)
{
	/*
	 *  End of matches.
	 */
	vreply(p, RESPONSE, 0, _Index, 0, _End);
}

/*
 *  int index
 *
 *  void
 */
static void
nil_read_entry(Client *p, int idx)
{
	vreply(p, RESPONSE, 0,
		_Index, 0,
		_End);
}

/*
 *  void
 *
 *  void
 */
static void
nil_write_entry(Client *p)
{
	vreply(p, RESPONSE, 0,
		_Int, -1,
		_End);
}

/*
 *  int fromidx
 *
 *  int error
 *
 *  Copy named record to current record.
 *  If named (old) record is not found active,
 *  read it in temporarily but free after copy.
 *  Be careful if copying to itself.
 */
static void
nil_copy_entry(Client *p, int fromidx)
{
	vreply(p, RESPONSE, 0,
		_Int, -1,
		_End);
}

/*
 *  void
 *
 *  int   index
 */
static void
nil_create_new(Client *p)
{
	vreply(p, RESPONSE, 0,
		_Index, 0,
		_End);
}

/*
 *  int  index
 *  string ext (can be missing, if the whole entry)
 *
 *  void
 */
static void
nil_remove_subfiles(Client *p, int idx, char *ext)
{
	vreply(p, RESPONSE, 0,
		_Int, -1,
		_End);
}

static void
nil_remove_entry(Client *p, int idx)
{
	vreply(p, RESPONSE, 0,
		_Int, -1,
		_End);
}

/*
 *  Supplemental files under logsystems.
 *
 *  When the real filename is constructed from parameters
 *  and other (base specific) information, a
 *  function do_really_open_file(p, filename, mode, perms)
 *  can be used to continue the process.
 *
 *  If, for some reason, the data is not in a file, but
 *  must be synthesized, call like
 *  do_synthetic_file(p, handler_fcn, handler_arg, mode)
 *  can be used. The call sets up the client structure so that
 *  I/O from client results in successive calls like
 *  handler_fcn(handler_arg, buffer, buffersize) and
 *  the return value from handler is bytecount, -1 on error and 0 on EOF.
 *  buffersize is the requested length for reads and
 *  the valid length for writes. When writing, EOF is marked as one additional
 *  call with zero buffersize.
 */

static BOOL
nil_open_file(Client *p,
	int idx,              /* index in logsystem */
	char *ext,            /* the supplemental extension */
	char *mode,           /* "r", "w" or "a" */
	int perms)            /* U*X style permissions for creation */
{
	vreply(p, RESPONSE, 0,
		_Int, -1,
		_Int, 0,
		_End);

	return (1);
}

/*
 *  Fields "STATUS" and "d.COUNTER" can be treated similarly
 *  if the underlying thing supports adding fields on the fly.
 */

/*
 *  Get anything in any format,
 *  from record.
 *  Plain (not base specific) parameter-files are not handled here.
 *  Conversions are made for any datatype.
 *  For some bases the name can contain a '.', then
 *  (d.COUNTER) -> get COUNTER from "d" file
 *
 *  applies to currently selected record!
 */
static void
nil_getavalue(Client *p,
	int how,
	char *name)
{
	switch (how) {
	case GET_NUM_FIELD:
	case GET_NUM_PARM:
		vreply(p, RESPONSE, 0, _Double, 0.0, _End);
		break;
	case GET_TIME_FIELD:
	case GET_TIME_PARM:
		vreply(p, RESPONSE, 0, _Time, 0, _End);
		break;
	case GET_TEXT_FIELD:
	case GET_TEXT_PARM:
		vreply(p, RESPONSE, 0, _String, "", _End);
		break;
	}
}

/*
 *  Set name to value (input type coded according to how).
 *
 *  For some basetypes, name can contain a '.' like "d.COUNTER",
 *  and supplemental file "d" for current record is then accessed
 *  for "COUNTER".
 *
 *  applies to currently selected record!
 *
 *  This cannot generate an error reply!
 */
static void
nil_setavalue(Client *p,
	int how,
	char *name,
	union rlsvalue *value)
{
	switch (how) {
	case SET_NUM_FIELD:
	case SET_TEXT_FIELD:
	case SET_TIME_FIELD:
	case SET_NUM_PARM:
	case SET_TEXT_PARM:
	case SET_TIME_PARM:
		;
	}
}

/*
 *  Enumerate possible extensions for one specific entry (idx),
 *  or all the extensions there could be for all the entries (idx == 0).
 *
 *  For example, 10.l, 10.i and 20.x exist.
 *   enumeration for 10 would return "l" and "i",
 *   but enumeration for 0 would return "l", "i" and "x".
 *
 *  Order is undefined.
 */
static void
nil_enum_extensions(Client *p,
	int idx)
{
	start_response(p, 0);
#if 0
	if (idx) {
		append_line(&p->buf, "l");
		append_line(&p->buf, "i");
	} else {
		append_line(&p->buf, "l");
		append_line(&p->buf, "i");
		append_line(&p->buf, "x");
	}
#endif
	flush_lines(p);
}

static int
nil_get_curidx(Client *p)
{
	return (0); /* no index here, never */
}

/*
 *  Delete all parameters for given LOG.d
 */
static void
nil_clear_parms(Client *p, int idx, char *ext)
{
	/* nothing to do */
}

static void
nil_enum_parms(Client *p, int idx, char *ext)
{
	start_response(p, 0);
#if 0
	append_line(&p->buf, "COUNTER");
	append_line(&p->buf, "MAXCOUNT");
#endif
	flush_lines(p);
}

static void
nil_fetch_parms(Client *p, int idx, char *ext)
{
	start_response(p, 0);
#if 0
	append_line(&p->buf, "COUNTER=123");
	append_line(&p->buf, "MAXCOUNT=9999");
#endif
	flush_lines(p);
}

static BOOL
nil_stat_file(Client *p,
        int idx,              /* index in logsystem */
        char *ext,            /* the supplemental extension */
        stat_t *st_ret)  /* result */
{
	memset(st_ret, 0, sizeof(*st_ret));
	return (0);
}

static void
nil_save_parms(Client *p,
                int idx,
                char *ext,
                char *s)
{
	vreply(p, RESPONSE, 0, _Int, 0, _End);
}

baseops_t nil_baseops = {

	nil_open_base,
	nil_close_base,
	nil_cleanup_client,

	nil_periodic_sync,

	nil_get_base_info,
	nil_tell_named_field,
	nil_tell_next_field,

	nil_clear_filter,
	nil_set_filter_order,
	nil_set_filter_key,
	nil_add_filter,
	nil_compile_filter,
	nil_get_next_match,

	nil_read_entry,
	nil_write_entry,
	nil_copy_entry,

	nil_create_new,
	nil_remove_subfiles,
	nil_remove_entry,

	nil_getavalue,
	nil_setavalue,

	"TYPOCHECK",   /* of different type to catch wrong initialization */

	nil_open_file,

	nil_enum_extensions,

	nil_init_client,

	nil_get_curidx,

	nil_clear_parms,
	nil_enum_parms,
	nil_fetch_parms,

	"TYPOCHECK",   /* of different type to catch wrong initialization */

        nil_stat_file,
	nil_save_parms,
};

/*
 *  baseops.c:init_baseops() calls each of these.
 *  The order there matters!
 */
baseops_t * init_nilops(void)
{
	return (&nil_baseops);
}


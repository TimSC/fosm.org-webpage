 /****************************************************************
 *								*
 *	Copyright 2004, 2007 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"

#include <stddef.h>

#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsblk.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "jnl.h"
#include "copy.h"
#include "iosp.h"
#include "repl_filter.h"
#include "repl_errno.h"
#include "jnl_typedef.h"
#include "collseq.h"

LITREF	boolean_t	jrt_is_replicated[JRT_RECTYPES];
LITREF	int		jrt_update[JRT_RECTYPES];
GBLREF	boolean_t	null_subs_xform;

/* Convert a transaction from jnl version 15  (GT.M versions V4.4-002 through V4.4-004) to jnl version 16 (V5)*/
/* Journal format is same, only null subscripts transformation may be needed */

int jnl_v15tov16(uchar_ptr_t jnl_buff, uint4 *jnl_len, uchar_ptr_t conv_buff, uint4 *conv_len, uint4 conv_bufsiz)
{
	unsigned char		*jb, *cb, *cstart, *jstart, *ptr;
	enum	jnl_record_type	rectype;
	int			status, reclen, conv_reclen;
	uint4			jlen;
	jrec_prefix 		*prefix;

	jb = jnl_buff;
	cb = conv_buff;
	status = SS_NORMAL;
	jlen = *jnl_len;
	assert(0 == ((UINTPTR_T)jb % sizeof(uint4)));
   	while (JREC_PREFIX_SIZE <= jlen)
	{
		assert(0 == ((UINTPTR_T)jb % sizeof(uint4)));
		prefix = (jrec_prefix *)jb;
		rectype = (enum	jnl_record_type)prefix->jrec_type;
		cstart = cb;
		jstart = jb;
   		if (0 != (reclen = prefix->forwptr))
		{
   			if (reclen <= jlen)
			{
				assert(IS_REPLICATED(rectype));
				conv_reclen = prefix->forwptr ;
				if (cb - conv_buff + conv_reclen > conv_bufsiz)
				{
					repl_errno = EREPL_INTLFILTER_NOSPC;
					status = -1;
					break;
				}
				memcpy(cb, jb, reclen);
				if (IS_SET_KILL_ZKILL(rectype) && null_subs_xform)
				{
					ptr = cb + FIXED_UPD_RECLEN + sizeof(jnl_str_len_t);
					/* Prior to V16, GT.M supports only GTM NULL collation */
					assert(GTMNULL_TO_STDNULL_COLL == null_subs_xform);
					GTM2STDNULLCOLL(ptr, *((jnl_str_len_t *)(cb + FIXED_UPD_RECLEN)));
				}
				cb = cb + reclen ;
				jb = jb + reclen ;
				assert(cb == cstart + conv_reclen);
				assert(jb == jstart + reclen);
				jlen -= reclen;
				continue;
			}
			repl_errno = EREPL_INTLFILTER_INCMPLREC;
			assert(FALSE);
			status = -1;
			break;
		}
		repl_errno = EREPL_INTLFILTER_BADREC;
		assert(FALSE);
		status = -1;
		break;
	}
	if ((-1 != status) && (0 != jlen))
	{
		repl_errno = EREPL_INTLFILTER_INCMPLREC;
		assert(FALSE);
		status = -1;
	}
	assert(0 == jlen || -1 == status);
	*jnl_len = (uint4)(jb - jnl_buff);
	*conv_len = (uint4)(cb - conv_buff);
	return(status);
}

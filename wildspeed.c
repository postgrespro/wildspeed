#include "postgres.h"

#include "catalog/pg_type.h"
#include "mb/pg_wchar.h"
#include "utils/array.h"

/*
 * MARK_SIGN is a sign of end of string and this character should
 * be regular character. The best candidate of this is a zero byte
 * which is accepted for any locale used by postgres. But it's impossible
 * to show it, so we will replace it to another one (MARK_SIGN_SHOW) which 
 * can be noticed well. But we can't use it as mark because it's allowed
 * to be inside string.
 */ 

#define		MARK_SIGN		'\0'
#define		MARK_SIGN_SHOW	'$'


#define WC_BEGIN		0x01   /* should be in begining of string */
#define WC_MIDDLE		0x02   /* should be in middle of string */
#define WC_END			0x04   /* should be in end of string */

PG_MODULE_MAGIC;

static text*
appendStrToText( text *src, char *str, int32 len, int32 maxlen )
{
	int32	curlen;

	if (src == NULL )
	{
		Assert( maxlen >= 0 );
		src = (text*)palloc( VARHDRSZ + sizeof(char*) * maxlen );
		SET_VARSIZE(src, 0 + VARHDRSZ);
	}

	curlen = VARSIZE(src) - VARHDRSZ;

	if (len>0)
		memcpy( VARDATA(src) + curlen, str, len );

	SET_VARSIZE(src, curlen + len + VARHDRSZ);

	return src;
}

static text*
appendMarkToText( text *src, int32 maxlen )
{
	char sign = MARK_SIGN;

	return appendStrToText( src, &sign, 1, maxlen );
}

static text*
setFlagOfText( char flag, int32 maxlen )
{
	char flagstruct[2];

	Assert( maxlen > 0 );
	/*
	 * Mark text by setting first byte to MARK_SIGN to indicate
	 * that text has flags. It's a safe for non empty string, 
	 * because first character can not be a MARK_SIGN (see
	 * gin_extract_permuted() )
	 */

	flagstruct[0] = MARK_SIGN;
	flagstruct[1] = flag;
	
	return appendStrToText(NULL, flagstruct, 2, maxlen );
}

PG_FUNCTION_INFO_V1(gin_extract_permuted);
Datum		gin_extract_permuted(PG_FUNCTION_ARGS);
Datum
gin_extract_permuted(PG_FUNCTION_ARGS)
{
	text	*src = PG_GETARG_TEXT_P(0);
	int32	*nentries = (int32 *) PG_GETARG_POINTER(1);
	Datum   *entries = NULL;
	int32	srclen = pg_mbstrlen_with_len(VARDATA(src), VARSIZE(src) - VARHDRSZ);

	*nentries = srclen;

	if ( srclen == 0 )
	{
		/*
		 * Empty string is encoded by alone MARK_SIGN character
		 */
		*nentries = 1;
		entries = (Datum*) palloc(sizeof(Datum));
		entries[0] = PointerGetDatum( appendMarkToText( NULL, 1 ) );
	}
	else
	{
		text 	*dst;
		int32 	i, 
				offset=0; /* offset to current position in src in bytes */ 
		int32	nbytes = VARSIZE(src) - VARHDRSZ;
		char	*srcptr = VARDATA(src);

		/*
		 * Permutation: hello will be permuted to hello$, ello$h, llo$he, lo$hel, o$hell.
		 * So, number of entries is equial to number of characters (not a bytes)
		 */

		entries = (Datum*)palloc(sizeof(char*) * nbytes );
		for(i=0; i<srclen;i++) {
		
			/*
			 * Copy first part. For llo$he it will be 'llo'
			 */
			dst = appendStrToText( NULL, srcptr + offset, nbytes - offset, nbytes + 1 ); 

			/*
			 * Set mark sign ($)
			 */
			dst = appendMarkToText( dst, -1 );

			/*
			 * Copy rest of string (in example above 'he')
			 */
			dst = appendStrToText( dst, srcptr, offset, -1 );

			entries[i] = PointerGetDatum(dst);
			offset += pg_mblen( srcptr + offset );
		}
	}

	PG_FREE_IF_COPY(src,0);
	PG_RETURN_POINTER(entries);
}

PG_FUNCTION_INFO_V1(wildcmp);
Datum       wildcmp(PG_FUNCTION_ARGS);
Datum
wildcmp(PG_FUNCTION_ARGS)
{
	text	*a = PG_GETARG_TEXT_P(0);
	text	*b = PG_GETARG_TEXT_P(1);
	bool	partialMatch = PG_GETARG_BOOL(2);
	int32	cmp;
	int		lena,
			lenb;
	char 	*ptra = VARDATA(a),
			*ptrb = VARDATA(b);
	char	flag = 0;

	lena = VARSIZE(a) - VARHDRSZ;
	lenb = VARSIZE(b) - VARHDRSZ;

	/*
	 * sets correct pointers and lengths in case of flags
	 * presence
	 */
	if ( lena > 2 && *ptra == MARK_SIGN )
	{
		flag = *(ptra+1);
		ptra+=2;
		lena-=2;

		if ( lenb > 2 && *ptrb == MARK_SIGN )
		{
			/*
			 * If they have different flags then they can not be equal, this 
			 * place works only during check of equality of keys
			 * to search
			 */
			if ( flag != *(ptrb+1) )
				return 1;
			ptrb+=2;
			lenb-=2;

			/* b can not be a product of gin_extract_wildcard for partial match mode */
			Assert( partialMatch == false );
		}
	} 
	else  if ( lenb > 2 && *ptrb == MARK_SIGN )
	{
		/* b can not be a product of gin_extract_wildcard for partial match mode */
		Assert( partialMatch == false );

		ptrb+=2;
		lenb-=2;
	}

	if ( lena == 0 )
	{
		if ( partialMatch )
			cmp = 0; /* full scan for partialMatch*/
		else
			cmp = (lenb>0) ? -1 : 0;
	}
	else
	{
		/*
		 * We couldn't use strcmp because of MARK_SIGN
		 */
		cmp = memcmp(ptra, ptrb, Min(lena, lenb));

		if ( partialMatch )
		{
			if ( cmp == 0 )
			{
				if ( lena > lenb )
				{
					/*
					 * b argument is not beginning with argument a
					 */
					cmp = 1;
				}
				else if ( flag > 0 && lenb>lena /* be safe */ )
				{ /* there is some flags to check */
					char	actualFlag;

					if ( ptrb[ lenb - 1 ] == MARK_SIGN )
						actualFlag = WC_BEGIN;  
					else if ( ptrb[ lena ] == MARK_SIGN )
						actualFlag = WC_END;
					else
						actualFlag = WC_MIDDLE;

					if ( (flag & actualFlag) == 0 )
					{
						/* 
						 * Prefix are matched but this prefix s not placed as needed.
						 * so we should give a smoke signal to GIN that we don't want
						 * this match, but wish to continue scan 
						 */
						cmp = -1;
					}
				}
			} 
			else if (cmp < 0)
			{
				cmp = 1; /* prevent continue scan */
			}
		} 
		else if ( (cmp == 0) && (lena != lenb) )
		{
			cmp = (lena < lenb) ? -1 : 1;
		}
	}

	PG_FREE_IF_COPY(a,0);
	PG_FREE_IF_COPY(b,1);
	PG_RETURN_INT32( cmp );	
}

#ifdef OPTIMIZE_WILDCARD_QUERY

typedef struct 
{
	Datum	entry;
	int32	len;
	char	flag;
} OptItem;


/*
 * Function drops most short search word to speedup 
 * index search by preventing use word which gives
 * a lot of matches
 */
static void 
optimize_wildcard_search( Datum *entries, int32 *nentries )
{
	int32	maxlen=0;
	OptItem	*items;
	int		i, nitems = *nentries;
	char 	*ptr,*p;

	items = (OptItem*)palloc( sizeof(OptItem) * (*nentries) );
	for(i=0;i<nitems;i++)
	{
		items[i].entry = entries[i];
		items[i].len = VARSIZE(entries[i]) - VARHDRSZ;
		ptr = VARDATA(entries[i]);

		if ( items[i].len > 2 && *ptr == MARK_SIGN )
		{
			items[i].len-=2;
			items[i].flag = *(ptr+1);
		}
		else
		{
			items[i].flag = 0;
			if ( items[i].len > 1 && (p=strchr(ptr, MARK_SIGN)) != NULL )
			{
				if ( p == ptr + items[i].len -1 )
					items[i].flag = WC_BEGIN;
				else 
					items[i].flag = WC_BEGIN | WC_END;
			}
		}

		if ( items[i].len > maxlen )
			maxlen = items[i].len;
	}
	
	*nentries=0;

	for(i=0;i<nitems;i++)
	{
		if ( (items[i].flag & WC_BEGIN) && (items[i].flag & WC_END) )
		{	/* X$Y use always */
			entries[ *nentries ] = items[i].entry;
			(*nentries)++;
		}
		else if ( (items[i].flag & WC_MIDDLE) == 0 )
		{ 
			/* 
			 * for begin-only or end-only word we set more low limit than for 
			 * other variants
			 */
			if ( 3*items[i].len > maxlen )
			{
				entries[ *nentries ] = items[i].entry;
				(*nentries)++;
			}
		}
		else if ( 2*items[i].len > maxlen )
		{	
			/* 
			 * use only items with biggest length 
			 */
			entries[ *nentries ] = items[i].entry;
			(*nentries)++;
		}
	}

	Assert( *nentries>0 );

}
#endif

typedef struct 
{
	bool	iswildcard;
	int32	len;
	char	*ptr;
} WildItem;

PG_FUNCTION_INFO_V1(gin_extract_wildcard);
Datum		gin_extract_wildcard(PG_FUNCTION_ARGS);
Datum
gin_extract_wildcard(PG_FUNCTION_ARGS)
{
	text			*q = PG_GETARG_TEXT_P(0);
	int32			lenq = VARSIZE(q) - VARHDRSZ;
	int32			*nentries = (int32 *) PG_GETARG_POINTER(1);
#ifdef NOT_USED
	StrategyNumber 	strategy = PG_GETARG_UINT16(2);
#endif
	bool			*partialmatch, 
					**ptr_partialmatch = (bool**) PG_GETARG_POINTER(3);
	Datum      		*entries = NULL;
	char			*qptr = VARDATA(q);
	int				clen,
					splitqlen = 0,
					i;
	WildItem		*items;
	text			*entry;

	*nentries = 0;

	if ( lenq == 0 )
	{
		partialmatch = *ptr_partialmatch = (bool*)palloc0(sizeof(bool));
		*nentries = 1;
		entries = (Datum*) palloc(sizeof(Datum));
		entries[0] = PointerGetDatum( appendMarkToText( NULL, 1 ) );

		PG_RETURN_POINTER(entries);
	}

	partialmatch = *ptr_partialmatch = (bool*)palloc0(sizeof(bool) * lenq);
	entries = (Datum*) palloc(sizeof(Datum) * lenq);
	items=(WildItem*) palloc0( sizeof(WildItem) * lenq );


	/*
	 * Parse expression to the list of constant parts and
	 * wildcards
	 */
	while( qptr - VARDATA(q) < lenq )
	{
		clen = pg_mblen(qptr);

		if ( clen==1 && (*qptr == '_' || *qptr == '%' ) )
		{
			if ( splitqlen == 0 )
			{
				items[ splitqlen ].iswildcard = true;
				splitqlen++;
			} 
			else if ( items[ splitqlen-1 ].iswildcard == false )
			{
				items[ splitqlen-1 ].len = qptr - items[ splitqlen-1 ].ptr;
				items[ splitqlen ].iswildcard = true;
				splitqlen++;
			}
			/*
			 * ignore wildcard, because we don't make difference beetween
			 * %, _ or a combination of its
			 */
		}
		else
		{
			if ( splitqlen == 0 || items[ splitqlen-1 ].iswildcard == true )
			{
				items[ splitqlen ].ptr = qptr;
				splitqlen++;
			}
		}
		qptr += clen;
	}

	Assert( splitqlen >= 1 );
	if ( items[ splitqlen-1 ].iswildcard == false )
		items[ splitqlen-1 ].len = qptr - items[ splitqlen-1 ].ptr;

	if ( items[ 0 ].iswildcard == false )
	{
		/* X... */
		if ( splitqlen == 1 )
		{
			/*   X => X$, exact match */
			*nentries = 1;
			entry = appendStrToText(NULL, items[ 0 ].ptr, items[ 0 ].len, lenq+1);
			entry = appendMarkToText( entry, -1 );
			entries[0] = PointerGetDatum( entry );
		} 
		else if ( items[ splitqlen-1 ].iswildcard == false ) 
		{
			/*   X * [X1 * [] ] ] Y => Y$X* [ + X1* [] ] */

			*nentries = 1;
			entry = appendStrToText(NULL, items[ splitqlen-1 ].ptr, items[ splitqlen-1 ].len, lenq+1);
			entry = appendMarkToText( entry, -1 );
			entry = appendStrToText(entry, items[ 0 ].ptr, items[ 0 ].len, -1);
			partialmatch[0] = true;
			entries[0] = PointerGetDatum( entry );

			for(i=1; i<splitqlen-1; i++)
			{
				if ( items[ i ].iswildcard )
					continue;
				entry = setFlagOfText( WC_MIDDLE, lenq + 1 /* MARK_SIGN */ + 2 /* flag */ ); 
				entry = appendStrToText(entry, items[ i ].ptr, items[ i ].len, -1 );
				partialmatch[ *nentries ] = true;
				entries[ *nentries ] =  PointerGetDatum( entry );
				(*nentries)++;
			}
		}
		else
		{
			/*   X * [ X1 * [] ]  => X*$ [ + X1* [] ] */
		
			entry = setFlagOfText( WC_BEGIN, lenq + 1 /* MARK_SIGN */ + 2 /* flag */ );
			entry = appendStrToText(entry, items[ 0 ].ptr, items[ 0 ].len, -1);
			*nentries = 1;
			partialmatch[ 0 ] = true;
			entries[0] = PointerGetDatum( entry );

			for(i=2; i<splitqlen-1; i++)
			{
				if ( items[ i ].iswildcard )
					continue;
				entry = setFlagOfText( (i==splitqlen-2) ? (WC_MIDDLE | WC_END) : WC_MIDDLE, 
										lenq + 1 /* MARK_SIGN */ + 2 /* flag */ );
				entry = appendStrToText(entry, items[ i ].ptr, items[ i ].len, -1);
				partialmatch[ *nentries ] = true;
				entries[ *nentries ] =  PointerGetDatum( entry );
				(*nentries)++;
			}
		}
	} 
	else
	{
		/* *...  */

		if ( splitqlen == 1 )
		{
			/* any word => full scan */
			*nentries = 1;
			entry = appendStrToText(NULL, "", 0, lenq+1);
			partialmatch[0] = true;
			entries[0] = PointerGetDatum( entry );
		}
		else if ( items[ splitqlen-1 ].iswildcard == false )
		{
			/*     * [ X1 * [] ] X  => X$* [ + X1* [] ]  */
			*nentries = 1;
			entry = appendStrToText(NULL, items[ splitqlen-1 ].ptr, items[ splitqlen-1 ].len, lenq+1);
			entry = appendMarkToText( entry, -1 );
			partialmatch[0] = true;
			entries[0] = PointerGetDatum( entry );

			for(i=1; i<splitqlen-1; i++)
			{
				if ( items[ i ].iswildcard )
					continue;
				entry = setFlagOfText( (i==1) ? (WC_MIDDLE | WC_BEGIN) : WC_MIDDLE, 
										lenq + 1 /* MARK_SIGN */ + 2 /* flag */ );
				entry = appendStrToText(entry, items[ i ].ptr, items[ i ].len, -1);
				partialmatch[ *nentries ] = true;
				entries[ *nentries ] =  PointerGetDatum( entry );
				(*nentries)++;
			}
		}
		else
		{
			/* * X [ * X1 [] ] * => X* [ + X1* [] ] */
			for(i=1; i<splitqlen-1; i++)
			{
				if ( items[ i ].iswildcard )
					continue;

				if ( splitqlen > 3 )
				{
					if ( i==1 )
						entry = setFlagOfText( WC_MIDDLE | WC_BEGIN, lenq + 1 /* MARK_SIGN */ + 2 /* flag */ );
					else if ( i == splitqlen-2 )
						entry = setFlagOfText( WC_MIDDLE | WC_END, lenq + 1 /* MARK_SIGN */ + 2 /* flag */ );
					else
						entry = setFlagOfText( WC_MIDDLE, lenq + 1 /* MARK_SIGN */ + 2 /* flag */ ); 
				}
				else
					entry = NULL;
				entry = appendStrToText(entry, items[ i ].ptr, items[ i ].len, lenq+1);
				partialmatch[ *nentries ] = true;
				entries[ *nentries ] =  PointerGetDatum( entry );
				(*nentries)++;
			}
		}
	}

	PG_FREE_IF_COPY(q,0);

#ifdef OPTIMIZE_WILDCARD_QUERY
	if ( *nentries > 1 )
		optimize_wildcard_search( entries, nentries );
#endif

	PG_RETURN_POINTER(entries);
}


PG_FUNCTION_INFO_V1(gin_consistent_wildcard);
Datum       gin_consistent_wildcard(PG_FUNCTION_ARGS);
Datum
gin_consistent_wildcard(PG_FUNCTION_ARGS)
{
	bool       	*check = (bool *) PG_GETARG_POINTER(0);
	bool        res = true;
	int         i;
	int32 		nentries;

	if ( fcinfo->flinfo->fn_extra == NULL )
	{
		bool	*pmatch;

		/*
		 * we need to get nentries, we'll get it by regular way
		 * and store it in function context
		 */

		fcinfo->flinfo->fn_extra = MemoryContextAlloc(fcinfo->flinfo->fn_mcxt,
														sizeof(int32));

		DirectFunctionCall4(
					gin_extract_wildcard,
					PG_GETARG_DATUM(2),  /* query */
					PointerGetDatum( fcinfo->flinfo->fn_extra ), /* &nentries */
					PG_GETARG_DATUM(1),  /* strategy */
					PointerGetDatum( &pmatch )
		);
	}

	nentries = *(int32*) fcinfo->flinfo->fn_extra;

	for (i = 0; res && i < nentries; i++)
		if (check[i] == false)
			res = false;

	PG_RETURN_BOOL(res);
}

/*
 * Mostly debug fuction
 */
PG_FUNCTION_INFO_V1(permute);
Datum       permute(PG_FUNCTION_ARGS);
Datum
permute(PG_FUNCTION_ARGS)
{
	Datum		src = PG_GETARG_DATUM(0);
	int32		nentries = 0;
	Datum 		*entries;
	ArrayType	*res;
	int 		i;

	/*
	 * Get permuted values by gin_extract_permuted()
	 */
	entries = (Datum*) DatumGetPointer(DirectFunctionCall2(
					gin_extract_permuted, src, PointerGetDatum(&nentries)
			));

	/*
	 * We need to replace MARK_SIGN to MARK_SIGN_SHOW.
	 * See comments above near definition of MARK_SIGN and MARK_SIGN_SHOW.
	 */
	if ( nentries == 1 && VARSIZE(entries[0]) == VARHDRSZ + 1)
	{
		*(VARDATA(entries[0])) = MARK_SIGN_SHOW;		
	}
	else
	{
		int32  	offset = 0; /* offset of MARK_SIGN */
		char	*ptr;

		/*
		 * We scan array from the end because it allows simple calculation
		 * of MARK_SIGN position: on every iteration it's moved one 
		 * character to the end.
		 */
		for(i=nentries-1;i>=0;i--) 
		{
			ptr = VARDATA(entries[i]);

			offset += pg_mblen(ptr);
			Assert( *(ptr + offset) == MARK_SIGN );
			*(ptr + offset) = MARK_SIGN_SHOW;
		}
	}

	res = construct_array(
					entries,
					nentries,
					TEXTOID,
					-1,
					false,
					'i'
			);

	PG_RETURN_POINTER(res);
}

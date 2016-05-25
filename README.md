# Wildspeed - fast wildcard search for LIKE operator

Wildspeed extension provides GIN index support for wildcard search
for LIKE operator.

Online version of this document is available
http://www.sai.msu.su/~megera/wiki/wildspeed

## Authors

* Oleg Bartunov <oleg@sai.msu.su>, Moscow, Moscow University, Russia
* Teodor Sigaev <teodor@sigaev.ru>, Moscow, Moscow University,Russia

## License

Stable version, included into PostgreSQL distribution, released under
BSD license. Development version, available from this site, released
under the GNU General Public License, version 2 (June 1991)

## Downloads

Stable version of wildspeed is available from
http://www.sigaev.ru/cvsweb/cvsweb.cgi/wildspeed/

## Installation

    % make USE_PGXS=1
    % make install
    % make installcheck
    % psql DB -c 'CREATE EXTENSION wildspeed'

Wildspeed provides opclass (wildcard_ops) and uses partial match
feature of GIN, available since 8.4. Also, it supports full index scan.

The size of index can be very big, since it contains entries for all
permutations of the original word, see [1] for details. For example,
word hello will be indexed as well as its all permutations:

    =# select permute('hello');
                   permute
    --------------------------------------
     {hello$,ello$h,llo$he,lo$hel,o$hell}
    
       Notice, symbol '$' is used only for visualization, in actual
       implementation null-symbol '\0' is used.
    
       Search query rewritten as prefix search:
    *X  -> X$*
    X*Y -> Y$X*
    *X* -> X*

For example, search for 'hel*o' will be rewritten as 'o$hel'.

Special function `permute(TEXT)`, which returns all permutations of
argument, provided for test purposes.

Performance of wildspeed depends on search pattern. Basically,
wildspeed is less effective than btree index with text_pattern_ops for
prefix search (the difference is greatly reduced for long prefixes) and
much faster for wildcard search.

Wildspeed by default uses optimization (skip short patterns if there
are long one), which can be turned off in Makefile by removing define
`-DOPTIMIZE_WILDCARD_QUERY`.

## References

* http://www.cs.wright.edu/~tkprasad/courses/cs499/L05TolerantIR.ppt
* http://nlp.stanford.edu/IR-book/html/htmledition/permuterm-indexes-1.html

## Examples

Table words contains 747358 records, w1 and w2 columns contains the
same data in order to test performance of Btree (w1) and GIN (w2)
indexes:

       Table "public.words"
     Column | Type | Modifiers
    --------+------+-----------
     w1     | text |
     w2     | text |
    
    words=# create index bt_idx on words using btree (w1 text_pattern_ops);
    CREATE INDEX
    Time: 1885.195 ms
    words=# create index gin_idx on words using gin (w2 wildcard_ops);
    vacuum analyze;
    CREATE INDEX
    Time: 530351.223 ms

Size:

    words=# select pg_relation_size('words');
     pg_relation_size
    ------------------
             43253760
    
    words=# select pg_relation_size('gin_idx');
     pg_relation_size
    ------------------
            417816576
    (1 row)
    
    words=# select pg_relation_size('bt_idx');
     pg_relation_size
    ------------------
             23437312
    (1 row)
    
       Prefix search:
    words=# select count(*) from words where w1 like 'a%';
     count
    -------
     15491
    (1 row)
    
    Time: 7.502 ms
    words=# select count(*) from words where w2 like 'a%';
     count
    -------
     15491
    (1 row)
    
    Time: 31.152 ms
    
Wildcard search:

    words=# select count(*) from words where w1 like '%asd%';
     count
    -------
        26
    (1 row)
    
    Time: 147.308 ms
    words=# select count(*) from words where w2 like '%asd%';
     count
    -------
        26
    (1 row)
    
    Time: 0.339 ms
    
Full index scan:

    words=# set enable_seqscan to off;
    words=# explain analyze select count(*) from words where w2 like '%';
                                                                    QUERY PLAN
    
    --------------------------------------------------------------------------------------------------------------
    -----------------------------
     Aggregate  (cost=226274.98..226274.99 rows=1 width=0) (actual time=2218.709..2218.709 rows=1 loops=1)
       ->  Bitmap Heap Scan on words  (cost=209785.73..224406.77 rows=747283 width=0) (actual time=1510.516..1913.
    430 rows=747358 loops=1)
             Filter: (w2 ~~ '%'::text)
             ->  Bitmap Index Scan on gin_idx  (cost=0.00..209598.91 rows=747283 width=0) (actual time=1509.358..1
    509.358 rows=747358 loops=1)
                   Index Cond: (w2 ~~ '%'::text)
     Total runtime: 2218.747 ms
    (6 rows)

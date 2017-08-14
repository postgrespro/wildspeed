-- complain if script is sourced in psql, rather than via CREATE EXTENSION
\echo Use "CREATE EXTENSION wildspeed" to load this file. \quit

-- support functions for gin
CREATE OR REPLACE FUNCTION gin_extract_permuted(text, internal)
RETURNS internal
AS 'MODULE_PATHNAME'
LANGUAGE C IMMUTABLE;

CREATE OR REPLACE FUNCTION wildcmp(text, text)
RETURNS int32
AS 'MODULE_PATHNAME'
LANGUAGE C IMMUTABLE;

CREATE OR REPLACE FUNCTION wildcmp_prefix(text, text, int2)
RETURNS int32
AS 'MODULE_PATHNAME'
LANGUAGE C IMMUTABLE;

CREATE OR REPLACE FUNCTION gin_extract_wildcard(text, internal, int2, internal)
RETURNS internal
AS 'MODULE_PATHNAME'
LANGUAGE C IMMUTABLE;

CREATE OR REPLACE FUNCTION gin_consistent_wildcard(internal, int2, text)
RETURNS internal
AS 'MODULE_PATHNAME'
LANGUAGE C IMMUTABLE;

CREATE OR REPLACE FUNCTION gin_triconsistent_wildcard(internal, int2, text, int4, internal, internal, internal)
RETURNS internal
AS 'MODULE_PATHNAME'
LANGUAGE C IMMUTABLE;

CREATE OPERATOR CLASS wildcard_ops
FOR TYPE text USING gin
AS
	OPERATOR        1       ~~,
	FUNCTION        1       wildcmp(text,text),
	FUNCTION        2       gin_extract_permuted(text, internal),
	FUNCTION        3       gin_extract_wildcard(text, internal, int2, internal),
	FUNCTION        4       gin_consistent_wildcard(internal, int2, text),
	FUNCTION        5       wildcmp_prefix(text,text,int2),
	FUNCTION		6		gin_triconsistent_wildcard(internal, int2, text, int4, internal, internal, internal),
STORAGE         text;


--debug function
CREATE OR REPLACE FUNCTION permute(text)
RETURNS _text
AS 'MODULE_PATHNAME'
LANGUAGE C STRICT IMMUTABLE;

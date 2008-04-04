BEGIN;

DROP FUNCTION IF EXISTS permute(text) CASCADE;
DROP OPERATOR CLASS IF EXISTS wildcard_ops USING gin CASCADE;
DROP FUNCTION IF EXISTS gin_extract_permuted(text, internal)  CASCADE;
DROP FUNCTION IF EXISTS wildcmp(text, text, bool) CASCADE;
DROP FUNCTION IF EXISTS gin_extract_wildcard(text, internal, int2, internal) CASCADE;
DROP FUNCTION IF EXISTS gin_consistent_wildcard(internal, int2, text) CASCADE;

END;

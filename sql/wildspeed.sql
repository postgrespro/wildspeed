--
-- first, define the datatype.  Turn off echoing so that expected file
-- does not depend on contents of wildspeed.sql.
--

SET client_min_messages = warning;
\set ECHO none
\i wildspeed.sql
\set ECHO all
RESET client_min_messages;


SELECT permute('');
SELECT permute('0');
SELECT permute('01');
SELECT permute('hello');

CREATE TABLE testlike ( t text );
\copy testlike from 'data/testlike.data'
INSERT INTO testlike VALUES ('');

SELECT * FROM testlike WHERE t LIKE '' ORDER BY t;
SELECT * FROM testlike WHERE t LIKE '%' ORDER BY t;
SELECT * FROM testlike WHERE t LIKE 'hello' ORDER BY t;
SELECT * FROM testlike WHERE t LIKE 'hello%' ORDER BY t;
SELECT * FROM testlike WHERE t LIKE 'hel%' ORDER BY t;
SELECT * FROM testlike WHERE t LIKE '%hello' ORDER BY t;
SELECT * FROM testlike WHERE t LIKE '%lo' ORDER BY t;
SELECT * FROM testlike WHERE t LIKE '%hello%' ORDER BY t;
SELECT * FROM testlike WHERE t LIKE '%ll%' ORDER BY t;
SELECT * FROM testlike WHERE t LIKE 'h%o' ORDER BY t;
SELECT * FROM testlike WHERE t LIKE 'h%l%o' ORDER BY t;
SELECT * FROM testlike WHERE t LIKE 'h%l%o' ORDER BY t;
SELECT * FROM testlike WHERE t LIKE 'h%e%l%o' ORDER BY t;
SELECT * FROM testlike WHERE t LIKE '%e%l%' ORDER BY t;
SELECT * FROM testlike WHERE t LIKE '%e%o' ORDER BY t;
SELECT * FROM testlike WHERE t LIKE 'h%o%' ORDER BY t;
SELECT * FROM testlike WHERE t LIKE 'j%e%' ORDER BY t;
SELECT * FROM testlike WHERE t LIKE 'j%k%' ORDER BY t;
SELECT * FROM testlike WHERE t LIKE 'h%k%' ORDER BY t;

CREATE INDEX like_idx ON testlike USING gin ( t wildcard_ops );

SET enable_seqscan=off;

SELECT * FROM testlike WHERE t LIKE '' ORDER BY t;
SELECT * FROM testlike WHERE t LIKE '%' ORDER BY t;
SELECT * FROM testlike WHERE t LIKE 'hello' ORDER BY t;
SELECT * FROM testlike WHERE t LIKE 'hello%' ORDER BY t;
SELECT * FROM testlike WHERE t LIKE 'hel%' ORDER BY t;
SELECT * FROM testlike WHERE t LIKE '%hello' ORDER BY t;
SELECT * FROM testlike WHERE t LIKE '%lo' ORDER BY t;
SELECT * FROM testlike WHERE t LIKE '%hello%' ORDER BY t;
SELECT * FROM testlike WHERE t LIKE '%ll%' ORDER BY t;
SELECT * FROM testlike WHERE t LIKE 'h%o' ORDER BY t;
SELECT * FROM testlike WHERE t LIKE 'h%l%o' ORDER BY t;
SELECT * FROM testlike WHERE t LIKE 'h%l%o' ORDER BY t;
SELECT * FROM testlike WHERE t LIKE 'h%e%l%o' ORDER BY t;
SELECT * FROM testlike WHERE t LIKE '%e%l%' ORDER BY t;
SELECT * FROM testlike WHERE t LIKE '%e%o' ORDER BY t;
SELECT * FROM testlike WHERE t LIKE 'h%o%' ORDER BY t;
SELECT * FROM testlike WHERE t LIKE 'j%e%' ORDER BY t;
SELECT * FROM testlike WHERE t LIKE 'j%k%' ORDER BY t;
SELECT * FROM testlike WHERE t LIKE 'h%k%' ORDER BY t;




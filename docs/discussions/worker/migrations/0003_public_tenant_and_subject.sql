-- New comments authorize against the OAuth provider's stable subject. Legacy comments
-- retain the login fallback until their author next writes them.
ALTER TABLE comments ADD COLUMN author_subject TEXT NOT NULL DEFAULT '';

BEGIN EXCLUSIVE TRANSACTION;

PRAGMA user_version = 8;

ALTER TABLE pkg ADD is_hybrid INTEGER NOT NULL DEFAULT 0;

COMMIT TRANSACTION;

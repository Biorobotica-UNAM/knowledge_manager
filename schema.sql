-- schema.sql (v4)
-- SQLite schema for the Justina knowledge_bank world-state database.
-- v4 changes from v3:
--   - person.photo (nullable BLOB) added, to store a person's photo
--     directly in the database. NULL (not an empty blob) means "no
--     photo stored"; see the PERSON section below for how this
--     interacts with KnowledgeRepository's default vs. WithPhoto
--     retrieval methods.
-- v3 changes from v2:
--   - object.name is now globally UNIQUE (was UNIQUE per category),
--     matching the domain's assumption that ManipulableObject.name
--     alone is identity (KnowledgeRepository.loadObjectByName(String)
--     has no category parameter, so it must be unambiguous).
--   - person.name is now UNIQUE, matching ObjectOwnership.heldByPerson
--     using a plain person name as identity.
--   - person.favorite_drink default changed from 'unknown' to '' to
--     match the domain contract ("empty otherwise", not "unknown").
--
-- Every CREATE TABLE below uses IF NOT EXISTS so that
-- KnowledgeRepository::ensureSchema() is idempotent: it can safely
-- run every time the process starts, including after a restart
-- against an already-initialized database file, without failing on
-- "table already exists".
--
-- OBJECT_TYPICAL_LOCATION (static "where objects are usually found",
-- for GPSR search-without-a-hint tasks) intentionally omitted for now.
-- It is purely additive and can be added later without touching
-- anything below.

PRAGMA foreign_keys = ON;

-- ---------------------------------------------------------------------
-- POSE_3D
-- Shared pose table referenced by known_location, object, person, etc.
-- ---------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS pose_3d (
    id      INTEGER PRIMARY KEY AUTOINCREMENT,
    x       REAL NOT NULL DEFAULT 0.0,
    y       REAL NOT NULL DEFAULT 0.0,
    z       REAL NOT NULL DEFAULT 0.0,
    qx      REAL NOT NULL DEFAULT 0.0,
    qy      REAL NOT NULL DEFAULT 0.0,
    qz      REAL NOT NULL DEFAULT 0.0,
    qw      REAL NOT NULL DEFAULT 1.0
);

-- ---------------------------------------------------------------------
-- KNOWN_LOCATION
-- name is the natural key (matches how locations are referred to
-- everywhere else: phrases, dictionaries, YAML seeds).
-- ---------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS known_location (
    name        TEXT PRIMARY KEY,
    pose_id     INTEGER REFERENCES pose_3d(id) ON DELETE SET NULL
);

-- ---------------------------------------------------------------------
-- ROBOT
-- Singleton table (CHECK id = 1 enforces a single row).
-- left/right hand are NOT columns here anymore -- see object_state.
-- ---------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS robot (
    id              INTEGER PRIMARY KEY CHECK (id = 1),
    name            TEXT NOT NULL,
    location_name   TEXT REFERENCES known_location(name) ON DELETE SET NULL
);

-- ---------------------------------------------------------------------
-- OBJECT_CATEGORY / PERSON_CATEGORY
-- ---------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS object_category (
    id      INTEGER PRIMARY KEY AUTOINCREMENT,
    name    TEXT NOT NULL UNIQUE
);

CREATE TABLE IF NOT EXISTS person_category (
    id      INTEGER PRIMARY KEY AUTOINCREMENT,
    name    TEXT NOT NULL UNIQUE
);

-- ---------------------------------------------------------------------
-- OBJECT
-- name is globally unique: the domain (ManipulableObject.getName(),
-- KnowledgeRepository.loadObjectByName(String)) treats name alone as
-- identity, with no category disambiguation.
-- ---------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS object (
    id              INTEGER PRIMARY KEY AUTOINCREMENT,
    name            TEXT NOT NULL UNIQUE,
    category_id     INTEGER NOT NULL REFERENCES object_category(id) ON DELETE RESTRICT,
    nav_pose_id     INTEGER REFERENCES pose_3d(id) ON DELETE SET NULL,
    manip_pose_id   INTEGER REFERENCES pose_3d(id) ON DELETE SET NULL
);

CREATE INDEX IF NOT EXISTS idx_object_category ON object(category_id);

-- ---------------------------------------------------------------------
-- PERSON_DESCRIPTION
-- accessories_phrase holds the already-natural-language clause
-- (e.g. "with glasses and a hat" / "no accessories") since the
-- structured accessories list is never queried independently.
-- ---------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS person_description (
    id                  INTEGER PRIMARY KEY AUTOINCREMENT,
    gender              TEXT,
    clothes             TEXT,
    age                 TEXT,
    hair                TEXT,
    facial_expression   TEXT,
    accessories_phrase  TEXT
);

-- ---------------------------------------------------------------------
-- PERSON
-- name is globally unique, matching ObjectOwnership.heldByPerson(name)
-- and KnowledgeRepository.loadPersonByName(String) using name alone
-- as identity. location_name is nullable: a person can be detected
-- before their room is known. favorite_drink defaults to '' (empty),
-- not 'unknown', per Person.getFavoriteDrink()'s contract ("empty
-- otherwise" when personType isn't guest). photo is a nullable BLOB
-- (no DEFAULT): NULL means "no photo stored", never an empty blob --
-- this is what lets savePerson's "photo = COALESCE(excluded.photo,
-- person.photo)" leave an existing photo untouched when a Person
-- loaded without one (see loadPersonByName vs.
-- loadPersonByNameWithPhoto) is saved back.
-- ---------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS person (
    id                  INTEGER PRIMARY KEY AUTOINCREMENT,
    name                TEXT NOT NULL UNIQUE,
    category_id         INTEGER NOT NULL REFERENCES person_category(id) ON DELETE RESTRICT,
    description_id      INTEGER REFERENCES person_description(id) ON DELETE SET NULL,
    favorite_drink      TEXT NOT NULL DEFAULT '',
    location_name       TEXT REFERENCES known_location(name) ON DELETE SET NULL,
    nav_pose_id         INTEGER REFERENCES pose_3d(id) ON DELETE SET NULL,
    photo               BLOB
);

CREATE INDEX IF NOT EXISTS idx_person_category ON person(category_id);

-- ---------------------------------------------------------------------
-- OBJECT_STATE
-- Exclusive-arc: an object's current "owner" is exactly one of
-- a known_location, a person, or one of the robot's hands.
-- 1:1 with object (object_id is both PK and FK).
-- ---------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS object_state (
    object_id           INTEGER PRIMARY KEY REFERENCES object(id) ON DELETE CASCADE,
    location_name       TEXT REFERENCES known_location(name) ON DELETE SET NULL,
    held_by_person_id   INTEGER REFERENCES person(id) ON DELETE SET NULL,
    held_by_robot_hand  TEXT CHECK (held_by_robot_hand IN ('left', 'right')),
    CHECK (
        (location_name      IS NOT NULL) +
        (held_by_person_id  IS NOT NULL) +
        (held_by_robot_hand IS NOT NULL) = 1
    )
);

CREATE INDEX IF NOT EXISTS idx_object_state_location ON object_state(location_name);
CREATE INDEX IF NOT EXISTS idx_object_state_person   ON object_state(held_by_person_id);

-- ---------------------------------------------------------------------
-- PERSON_ORDER
-- References the object catalog by FK instead of a free-text item
-- name, so an order can never point at a nonexistent object.
-- ---------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS person_order (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
    person_id   INTEGER NOT NULL REFERENCES person(id) ON DELETE CASCADE,
    object_id   INTEGER NOT NULL REFERENCES object(id) ON DELETE RESTRICT,
    seq         INTEGER NOT NULL DEFAULT 0
);

CREATE INDEX IF NOT EXISTS idx_person_order_person ON person_order(person_id);
CREATE INDEX IF NOT EXISTS idx_person_order_object ON person_order(object_id);

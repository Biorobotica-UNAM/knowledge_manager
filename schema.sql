-- schema.sql (v2)
-- SQLite schema for the Justina knowledge_bank world-state database.
-- Incorporates: unified object_state (exclusive-arc ownership),
-- category tables, dropped accessories table (folded into
-- person_description.accessories_phrase), person_order -> object FK,
-- nullable person.location_name, robot as a singleton table.
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
CREATE TABLE pose_3d (
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
CREATE TABLE known_location (
    name        TEXT PRIMARY KEY,
    pose_id     INTEGER REFERENCES pose_3d(id) ON DELETE SET NULL
);

-- ---------------------------------------------------------------------
-- ROBOT
-- Singleton table (CHECK id = 1 enforces a single row).
-- left/right hand are NOT columns here anymore -- see object_state.
-- ---------------------------------------------------------------------
CREATE TABLE robot (
    id              INTEGER PRIMARY KEY CHECK (id = 1),
    name            TEXT NOT NULL,
    location_name   TEXT REFERENCES known_location(name) ON DELETE SET NULL
);

-- ---------------------------------------------------------------------
-- OBJECT_CATEGORY / PERSON_CATEGORY
-- ---------------------------------------------------------------------
CREATE TABLE object_category (
    id      INTEGER PRIMARY KEY AUTOINCREMENT,
    name    TEXT NOT NULL UNIQUE
);

CREATE TABLE person_category (
    id      INTEGER PRIMARY KEY AUTOINCREMENT,
    name    TEXT NOT NULL UNIQUE
);

-- ---------------------------------------------------------------------
-- OBJECT
-- ---------------------------------------------------------------------
CREATE TABLE object (
    id              INTEGER PRIMARY KEY AUTOINCREMENT,
    name            TEXT NOT NULL,
    category_id     INTEGER NOT NULL REFERENCES object_category(id) ON DELETE RESTRICT,
    nav_pose_id     INTEGER REFERENCES pose_3d(id) ON DELETE SET NULL,
    manip_pose_id   INTEGER REFERENCES pose_3d(id) ON DELETE SET NULL,
    UNIQUE (category_id, name)
);

CREATE INDEX idx_object_category ON object(category_id);

-- ---------------------------------------------------------------------
-- PERSON_DESCRIPTION
-- accessories_phrase holds the already-natural-language clause
-- (e.g. "with glasses and a hat" / "no accessories") since the
-- structured accessories list is never queried independently.
-- ---------------------------------------------------------------------
CREATE TABLE person_description (
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
-- location_name is nullable: a person can be detected before their
-- room is known.
-- ---------------------------------------------------------------------
CREATE TABLE person (
    id                  INTEGER PRIMARY KEY AUTOINCREMENT,
    name                TEXT NOT NULL DEFAULT 'unknown',
    category_id         INTEGER NOT NULL REFERENCES person_category(id) ON DELETE RESTRICT,
    description_id      INTEGER REFERENCES person_description(id) ON DELETE SET NULL,
    favorite_drink      TEXT NOT NULL DEFAULT 'unknown',
    location_name       TEXT REFERENCES known_location(name) ON DELETE SET NULL,
    nav_pose_id         INTEGER REFERENCES pose_3d(id) ON DELETE SET NULL
);

CREATE INDEX idx_person_category ON person(category_id);

-- ---------------------------------------------------------------------
-- OBJECT_STATE
-- Exclusive-arc: an object's current "owner" is exactly one of
-- a known_location, a person, or one of the robot's hands.
-- 1:1 with object (object_id is both PK and FK).
-- ---------------------------------------------------------------------
CREATE TABLE object_state (
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

CREATE INDEX idx_object_state_location ON object_state(location_name);
CREATE INDEX idx_object_state_person   ON object_state(held_by_person_id);

-- ---------------------------------------------------------------------
-- PERSON_ORDER
-- References the object catalog by FK instead of a free-text item
-- name, so an order can never point at a nonexistent object.
-- ---------------------------------------------------------------------
CREATE TABLE person_order (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
    person_id   INTEGER NOT NULL REFERENCES person(id) ON DELETE CASCADE,
    object_id   INTEGER NOT NULL REFERENCES object(id) ON DELETE RESTRICT,
    seq         INTEGER NOT NULL DEFAULT 0
);

CREATE INDEX idx_person_order_person ON person_order(person_id);
CREATE INDEX idx_person_order_object ON person_order(object_id);

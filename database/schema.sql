-- upgrade_1.sql ---------------------------------------------------- MainVerte
CREATE TABLE enum_climate_zone (key INTEGER PRIMARY KEY, desc TEXT NOT NULL) STRICT;
INSERT INTO enum_climate_zone (key, desc) VALUES
    (0, 'Unknown'),
    (1, 'Temperate'),
    (2, 'Tropical'),
    (3, 'Montane'),
    (4, 'Desert'),
    (5, 'Subarctic');

CREATE TABLE enum_moisture (key INTEGER PRIMARY KEY, desc TEXT NOT NULL) STRICT;
INSERT INTO enum_moisture (key, desc) VALUES
    (0 , 'Moderate'),
    (1 , 'Wet'),
    (2 , 'Dry'),
    (3 , 'SeasonallyWet'),
    (4 , 'SeasonallyDry');

CREATE TABLE enum_region (key INTEGER PRIMARY KEY, desc TEXT NOT NULL) STRICT;
INSERT INTO enum_region (key, desc) VALUES
    (0,  'Unknown'),
    (1,  'Africa'),
    (2,  'Antarctica'),
    (3,  'Australasia'),
    (4,  'Caribbean'),
    (5,  'CentralAmerica'),
    (6,  'CentralAsia'),
    (7,  'EastAsia'),
    (8,  'Europe'),
    (9,  'Mediterranean'),
    (10,  'MiddleEast'),
    (11, 'NorthAmerica'),
    (12, 'PacificIslands'),
    (13, 'SouthAmerica'),
    (14, 'SouthAsia'),
    (15, 'SoutheastAsia'),
    (16, 'Subantarctic');

CREATE TABLE enum_shape (key INTEGER PRIMARY KEY, desc TEXT NOT NULL) STRICT;
INSERT INTO enum_shape (key, desc) VALUES
    (0, 'Unknown'),
    (1, 'Bamboo'),
    (2, 'Bulbous'),
    (3, 'Caudiciform'),
    (4, 'Climber'),
    (5, 'Herb'),
    (6, 'Rhizomatous'),
    (7, 'SemiSucculent'),
    (8, 'Succulent'),
    (9, 'Shrub'),
    (10, 'Tree'),
    (11, 'Tuberous');

CREATE TABLE enum_lifetime (key INTEGER PRIMARY KEY, desc TEXT NOT NULL) STRICT;
INSERT INTO enum_lifetime (key, desc) VALUES
    (0, 'Unknown'),
    (1, 'Annual'),
    (2, 'Biennial'),
    (3, 'Perennial'),
    (4, 'Monocarpic');

CREATE TABLE species (
    id                INTEGER PRIMARY KEY,
    slug              TEXT,
    name              TEXT,
    family            TEXT,
    genus             TEXT,
    photo_uri         TEXT,
    geographic_origin INTEGER,
    shape             INTEGER, -- enum_shape
    lifetime          INTEGER, -- enum_lifetime
    climate_zone      INTEGER, -- enum_climate_zone
    moisture          INTEGER,  -- enum_moisture
    UNIQUE (family, genus, name)
) STRICT;

CREATE INDEX species_slug_index ON species(slug);

CREATE TABLE specimen (
    id                INTEGER PRIMARY KEY,
    name              TEXT,
    photo_uri         TEXT,
    species_id        INTEGER REFERENCES species(id) ON DELETE SET NULL,
    date_acquisition  INTEGER,
    last_update       INTEGER,
    last_watering_at  INTEGER
) STRICT;
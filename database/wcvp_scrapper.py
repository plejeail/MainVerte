from collections import defaultdict
from typing import Dict, List
import csv
import sys
import time
import zipfile

ZIP_FILE          = sys.argv[1]
INPUT_CSV_FILE    = "wcvp_names.csv"
PREPARED_CSV_FILE = "./wcvp_prepared.csv"
OUTPUT_SQL_FILE   = sys.argv[2]
RANK_COLUMN       = "taxon_rank"
STATUS_COLUMN     = "taxon_status"
REVIEW_COLUMN     = "reviewed"
FAMILY_COL        = "family"
GENUS_COL         = "genus"
SPECIES_COL       = "species"

# Comparaison en UPPER pour cohérence
RANKS_KEEP = { "SPECIES" }

# mapping in -> out ; seules ces colonnes seront gardées/renommées
COLUMNS: Dict[str, str] = {
    "family": "family",
    "genus": "genus",
    "species": "species",
    "geographic_area": "geographic_area",
    "lifeform_description": "lifeform_description",
    "climate_description": "climate_description",
}

def _norm(s: str) -> str:
    # strip + handle None
    return s.strip().lower() if isinstance(s, str) else ""

def _build_index_map(header: List[str]) -> Dict[str, int]:
    # normalise l'en-tête pour faire un matching tolérant (casse/espaces)
    norm_to_idx = {h.strip().lower(): i for i, h in enumerate(header)}
    idx_map: Dict[str, int] = {}
    for in_col in COLUMNS.keys():
        key = in_col.strip().lower()
        if key in norm_to_idx:
            idx_map[in_col] = norm_to_idx[key]
        # sinon : colonne absente -> on ne l'écrit pas (filtrage)
    return idx_map

def prepareCSV(zip_path, zip_file, out_path) -> None:
    start = time.perf_counter()
    total, kept, duplicates = 0, 0, 0

    with zipfile.ZipFile(zip_path, 'r') as z:
        with z.open(zip_file) as fin, \
             open(out_path, "w", newline="", encoding="utf-8") as fout:
            reader = csv.reader(fin.read().decode("utf-8").splitlines(), delimiter='|')
            writer = csv.writer(fout)

            # Lire l'entête
            try:
                header = next(reader)
            except StopIteration:
                writer.writerow(list(COLUMNS.values()))
                print(f"✅ done, output file: {PREPARED_CSV_FILE}")
                print(f"→ {kept}/{total} lines kept (empty input)")
                return

            # Préparer les indices utiles
            # Tolérance casse/espaces
            hnorm = {h.strip().lower(): i for i, h in enumerate(header)}
            idx_rank   = hnorm.get(RANK_COLUMN.strip().lower(),   None)
            idx_status = hnorm.get(STATUS_COLUMN.strip().lower(), None)
            idx_review = hnorm.get(REVIEW_COLUMN.strip().lower(), None)
            idx_family = hnorm.get(FAMILY_COL.strip().lower(),    None)
            idx_genus  = hnorm.get(GENUS_COL.strip().lower(),     None)
            idx_spec   = hnorm.get(SPECIES_COL.strip().lower(),   None)

            if idx_rank is None or idx_status is None or idx_review is None:
                raise ValueError(
                    f"Missing required column(s) in header: "
                    f"{RANK_COLUMN}, {STATUS_COLUMN}, {REVIEW_COLUMN}"
                )

            idx_map = _build_index_map(header)

            # Écrire l'entête de sortie (ordre = valeurs du mapping)
            out_header = list(COLUMNS.values())
            writer.writerow(out_header)

            # Pré-binds pour micro-optimisations
            ranks_keep = RANKS_KEEP
            w_row = writer.writerow
            _upper = str.upper
            _norm_local = _norm

            seen = set()
            # Parcours streaming
            for row in reader:
                total += 1
                # accès direct par index → beaucoup plus rapide que DictReader
                reviewed = _norm_local(row[idx_review])
                status   = _norm_local(row[idx_status])
                rank     = _norm_local(row[idx_rank])

                if _upper(status) == "ACCEPTED" and _upper(rank) in ranks_keep:
                    # construire la ligne de sortie en suivant out_header
                    out_vals = []
                    key = (_norm_local(row[idx_family]), _norm_local(row[idx_genus]), _norm_local(row[idx_spec]))
                    if key in seen:
                        duplicates += 1
                    else:
                        seen.add(key)
                        for in_col, out_col in COLUMNS.items():
                            i = idx_map.get(in_col)
                            if i is None:
                                # colonne d'entrée absente → filtrée (pas de champ en sortie)
                                # on remplit quand même la position par "" pour garder l'ordre des out_cols
                                out_vals.append("")
                            else:
                                out_vals.append(_norm_local(row[i]))
                        w_row(out_vals)
                        kept += 1

    end = time.perf_counter()
    print(f"✅ done in ⏱️{end - start:.3f} seconds, 📒 output file: {PREPARED_CSV_FILE}")
    print(f"→ {kept}/{total} lines kept (reviewed=Y, status=ACCEPTED, rank in {sorted(RANKS_KEEP)}, {duplicates} duplicates)")

def validateCSV(csv_path) -> int:
    """
    Return 0 if csv is valid, 1 if csv is invalid. 
    """
    start = time.perf_counter()
    combos = defaultdict(list)

    with open(csv_path, newline="", encoding="utf-8") as f:
        reader = csv.DictReader(f)

        # Vérification minimale des colonnes
        required = {
            "family",
            "genus",
            "species",
            "geographic_area",
            "lifeform_description",
            "climate_description",
        }
        missing = required - set(reader.fieldnames or [])
        if missing:
            print(f"missing columns in CSV : {', '.join(sorted(missing))}")
            return 1

        for line_no, row in enumerate(reader, start=2):
            genus = (row["genus"])
            species = (row["species"])

            # Clé de combinaison
            key = (genus, species)

            combos[key].append({
                "line": line_no,
                "family": (row["family"] or "").strip(),
                "geographic_area": (row["geographic_area"] or "").strip(),
                "lifeform_description": (row["lifeform_description"] or "").strip(),
                "climate_description": (row["climate_description"] or "").strip(),
            })

    # Filtre les combinaisons avec plus d'une occurrence
    duplicates = {
        key: rows
        for key, rows in combos.items()
        if len(rows) > 1
    }

    if duplicates:
        for (genus, species), rows in sorted(duplicates.items()):
            print(f"{genus} {species} → {len(rows)} occurrences :")
            for info in rows:
                print(
                    f"  - ligne {info['line']}, "
                    f"family={info['family']}, "
                    f"geographic_area={info['geographic_area']}, "
                    f"lifeform={info['lifeform_description']}, "
                    f"climate={info['climate_description']}"
                )
            print()

        return 1
    
    end = time.perf_counter()
    print(f"✅ csv output validated in ⏱️{end - start:.3f} seconds")
    return 0

def csvToSqlInsert(csv_path, sql_path):
    BATCH_SIZE = 5000
    IGNORED_LIFEFORMS = {
        "epiphyte",
        "helophyte",
        "epiphytic or lithophytic chamaephyte",
        "epiphyte or lithophyte",
        "lithophyte",
        "hemiparasitic epiphyte",
        "lithophyte or epiphyte",
        "holomycotroph",
        "holoparasitic chamaephyte",
        "hemiepiphyte",
        "lithophyte or helophyte",
        "holoparasite",
        "geophyte",
        "holomycotrophic geophyte",
        "holoparasitic geophyte",
        "helophyte or lithophyte",
        "helophyte or epiphyte",
        "epiphytic hydrophyte",
        "rhizome lithophyte or epiphyte",
        "hemiparasite",
        "parasitic",
        "holoparasitic epiphyte"
    }

    def escape(value: str) -> str:
        return value.replace("'", "''")

    def start_insert(fout):
        fout.write(
            "INSERT INTO species "
            "(id, slug, name, family, genus, geographic_origin, shape, lifetime, climate_zone, moisture)\n"
            "VALUES\n"
        )

    start = time.perf_counter()
    count = 0
    first = True
    with open(csv_path, "r", newline="", encoding="utf-8") as fin, \
         open(sql_path, "w", newline="", encoding="utf-8") as fout:
        reader = csv.DictReader(fin)
        start_insert(fout)
        rowid = 0
        for line_no, row in enumerate(reader, start=2):
            genus = escape((row["genus"] or "").strip().lower())
            family = escape((row["family"] or "").strip().lower())
            species = escape((row["species"] or "").strip().lower())

            moisture = 0
            climate_zone = -1
            climate_description = (row["climate_description"] or "").strip().lower()
            if len(climate_description) == 0:
                climate_zone = 0
            elif "temperate" in climate_description:
                climate_zone = 1
            elif "tropical" in climate_description:
                climate_zone = 2
            elif "alpine" in climate_description or "montane" in climate_description:
                climate_zone = 3
            elif "desert" in climate_description:
                climate_zone = 4
            elif "subarctic" in climate_description:
                climate_zone = 5
               
            assert climate_zone > -1, f"Invalid climate description '{climate_description}'"

            if "seasonally dry" in climate_description:
                moisture = 4
            elif "seasonally wet" in climate_description:
                moisture = 3
            elif "dry" in climate_description:
                moisture = 2
            elif "wet" in climate_description:
                moisture = 1
                
            shape = 0
            lifetime = 0
            lifeform_description = (row["lifeform_description"] or "").strip().lower()
            if "annual" in lifeform_description:
                lifetime = 1
            elif "biennial" in lifeform_description:
                lifetime = 2
            elif "perennial" in lifeform_description:
                lifetime = 3
            elif "monocarpic" in lifeform_description:
                lifetime = 4

            if "scrambling" in lifeform_description or "liana" in lifeform_description or "climbing" in lifeform_description or "climber" in lifeform_description:
                shape = 4
            elif "herb" in lifeform_description:
                shape = 5
            elif "bulb" in lifeform_description:
                shape = 2                
            elif "tree" in lifeform_description:
                shape = 10
            elif "semisucculent" in lifeform_description:
                shape = 7
            elif "succulent" in lifeform_description:
                shape = 8
            elif "shrub" in lifeform_description:
                shape = 9
            elif "tuberous" in lifeform_description:
                shape = 11
            elif "rhizomatous" in lifeform_description:
                shape = 6
            elif "caudex" in lifeform_description:
                shape = 3
            elif "bamboo" in lifeform_description:
                shape = 1

            if lifeform_description not in IGNORED_LIFEFORMS:
                assert shape != 0 or lifetime != 0 or len(lifeform_description) == 0, f"invalid lifeform {lifeform_description}"

            if not first:
                fout.write(',\n')

            slug = f"{family} {genus} {species}"
            fout.write(f"({rowid}, '{slug}', '{species}','{family}','{genus}',0,{shape},{lifetime},{climate_zone},{moisture})")
            first = False
            rowid += 1

            count += 1
            if count % BATCH_SIZE == 0:
                fout.write(";\n")
                start_insert(fout)
                first = True


        fout.write(";\n")
    
    end = time.perf_counter()
    print(f"✅ sql insert generated in ⏱️{end - start:.3f} seconds")


if __name__ == "__main__":
    prepareCSV(ZIP_FILE, INPUT_CSV_FILE, PREPARED_CSV_FILE)
    if validateCSV(PREPARED_CSV_FILE) == 1:
        assert False, "Invalid csv"

    csvToSqlInsert(PREPARED_CSV_FILE, OUTPUT_SQL_FILE)
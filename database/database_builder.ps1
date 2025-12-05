# --- Constants ---------------------------------------------------------------
$Sqlite      = "sqlite3"
$Python      = "python"
$DbPath      = "../app/src/main/assets/db/mainverte.db"
$SchemaFile  = "schema.sql"
$SeedFile    = "seed.sql"
$wcvpUrl     = "https://sftp.kew.org/pub/data-repositories/WCVP/wcvp.zip"

# --- Utilities ---------------------------------------------------------------
function Fail($msg) {
    Write-Host "ERROR: $msg" -ForegroundColor Red
    exit 1
}

function Run-SqliteCommand([string]$db, [string]$command) {
    Write-Host "sqlite3 $db >>> $command"
    & $Sqlite $db $command
    if ($LASTEXITCODE -ne 0) {
        Fail "sqlite3 command failed : $command"
    }
}

function Run-SqliteScript([string]$db, [string]$scriptPath) {
    if (-not (Test-Path $scriptPath)) {
        Fail "sql script not found: $scriptPath"
    }

    Write-Host "sqlite3 $db >>> .read $scriptPath"
    & $Sqlite $db ".read `"$scriptPath`""
    if ($LASTEXITCODE -ne 0) {
        Fail "sqlite3 script failed: $scriptPath"
    }
}

function Run-PythonScript([string]$scriptPath, [string[]]$scriptArgs) {
    if (-not (Test-Path $scriptPath)) {
        Fail "python script not found: $scriptPath"
    }

    $argLine  = $scriptArgs -join " "
    Write-Host "python >>> $scriptPath $argLine"

    & $Python $scriptPath @scriptArgs
    if ($LASTEXITCODE -ne 0) {
        Fail "python script failed : $scriptPath"
    }
}

function Download-File([string]$url, [string]$destinationPath) {
    Write-Host "Download: $url"
    if ((Test-Path $destinationPath)) {
        Write-Host "file already exists, nothing to be done: $destinationPath"
        return
    }

    # Crée le dossier s'il n'existe pas
    $dir = Split-Path $destinationPath -Parent
    if ($dir -and -not (Test-Path $dir)) {
        Write-Host "Folder creation: $dir"
        New-Item -ItemType Directory -Path $dir | Out-Null
    }

    try {
        Invoke-WebRequest -Uri $url -OutFile $destinationPath -UseBasicParsing
    }
    catch {
        Fail "Download failed from url ${url}: $($_.Exception.Message)"
    }

    if (-not (Test-Path $destinationPath)) {
        Fail "Downloaded file does not exists: $destinationPath"
    }
}

# --- Verification ------------------------------------------------------------
# check sqlite
$sqliteVersion = & $Sqlite -version 2>$null
if ($LASTEXITCODE -ne 0) {
    Fail "sqlite3 not found or not executable: $Sqlite"
}

# check python
$pythonVersion = & $Python --version 2>$null
if ($LASTEXITCODE -ne 0) {
    Fail "python not found or not executable: $Python"
}

# Remove existing database
if (Test-Path $DbPath) {
    Write-Host "old database removed: $DbPath"
    Remove-Item $DbPath -Force
}

# Create database folder
$DbDir = Split-Path $DbPath -Parent
if ($DbDir -and -not (Test-Path $DbDir)) {
    Write-Host "Folder creation for DB: $DbDir"
    New-Item -ItemType Directory -Path $DbDir -Force | Out-Null
}

Write-Host "sqlite3    : $Sqlite"
Write-Host "output     : $DbPath"
Write-Host "sql schema : $SchemaFile"
Write-Host "sql seed   : $SeedFile"
Write-Host "source data: $wcvpUrl"
Write-Host ""

# Fetch database data ---------------------------------------------------------
Download-File -url $wcvpUrl -destinationPath "wcvp.zip"
Run-PythonScript "wcvp_scrapper.py" @("wcvp.zip", $SeedFile)

# --- Database Creation -------------------------------------------------------
Run-SqliteScript -db $DbPath -scriptPath $SchemaFile
Run-SqliteScript -db $DbPath -scriptPath $SeedFile
Run-SqliteCommand -db $DbPath -command "PRAGMA user_version = 1;"

# --- Optimization ------------------------------------------------------------
Run-SqliteCommand -db $DbPath -command "VACUUM;"
Run-SqliteCommand -db $DbPath -command "ANALYZE;"
Run-SqliteCommand -db $DbPath -command "PRAGMA optimize;"
Run-SqliteCommand -db $DbPath -command "VACUUM;"

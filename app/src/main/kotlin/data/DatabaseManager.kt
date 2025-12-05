package com.plej.mainverte.data

import android.annotation.SuppressLint
import android.content.Context
import android.database.sqlite.*
import android.database.sqlite.SQLiteOpenHelper
import com.plej.mainverte.utilities.Logger
import com.plej.mainverte.utilities.expect
import com.plej.mainverte.utilities.measureTime
import java.io.File
import java.io.FileNotFoundException
import java.io.FileOutputStream
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext

fun ensurePrepopulatedDatabase(context: Context, dbName: String, assetPath: String) {
    val dbFile: File = context.getDatabasePath(dbName)

    if (dbFile.exists()) {
        return
    }

    dbFile.parentFile?.let { parent ->
        if (!parent.exists()) {
            parent.mkdirs()
        }
    }

    context.assets.open(assetPath).use { input ->
        FileOutputStream(dbFile).use { output ->
            val buffer = ByteArray(8 * 1024)
            while (true) {
                val read = input.read(buffer)
                if (read <= 0) break
                output.write(buffer, 0, read)
            }
            output.flush()
        }
    }
}

class MainVerteDbHelper private constructor(
    private val context: Context,
    dbName: String,
    version: Int,
) : SQLiteOpenHelper(context, dbName, null, version) {
    companion object {
        private const val DB_NAME    = "mainverte.db"
        private const val DB_VERSION = 1
        private const val DB_ASSET   = "db/mainverte.db"

        @SuppressLint("StaticFieldLeak") // not garbage collected, fine
        private var singleton: MainVerteDbHelper? = null

        fun initialize(context: Context): MainVerteDbHelper {
            if ( singleton != null ) {
                return singleton!!
            }

            val appCtx = context.applicationContext
            ensurePrepopulatedDatabase(appCtx, DB_NAME, DB_ASSET)
            singleton = MainVerteDbHelper(appCtx, DB_NAME, DB_VERSION)
            singleton!!.triggerUpdate()

            return singleton!!
        }

        fun instance(): MainVerteDbHelper {
            expect { singleton != null }
            return singleton!!
        }
    }

    override fun onConfigure(db: SQLiteDatabase) {
        db.rawQuery("PRAGMA journal_mode=WAL;", null).use {}
        db.execSQL("PRAGMA foreign_keys=ON;")
        db.execSQL("PRAGMA synchronous=NORMAL;")
        db.execSQL("PRAGMA temp_store=MEMORY;")
    }

    override fun onCreate(db: SQLiteDatabase) {
        expect { false }
    }

    @SuppressLint("UseKtx")
    override fun onUpgrade(
        db: SQLiteDatabase,
        oldVersion: Int,
        newVersion: Int
    ) {
        measureTime("database upgrade") {
            expect { oldVersion < newVersion }

            for (v in (oldVersion + 1) .. newVersion) {
                val upgradePath = "db/upgrade_$v.sql"
                val seedPath = "db/seed_$v.sql"

                val upgradeSql = readAssetOrNull(context, upgradePath)
                val seedSql = readAssetOrNull(context, seedPath)

                expect { upgradeSql != null || seedSql != null }

                db.beginTransaction()
                try {
                    if (upgradeSql != null) {
                        execSqlScript(db, upgradeSql)
                        Logger.d { "database schema upgrade $v done" }
                    }
                    if (seedSql != null) {
                        execSqlScript(db, seedSql)
                        Logger.d { "database data update $v done" }
                    }

                    db.setTransactionSuccessful()
                } finally {
                    db.endTransaction()
                }
            }
        }
    }

    fun triggerUpdate() {
        writableDatabase
    }

    private fun readAssetOrNull(ctx: Context, assetPath: String): String? {
        return try {
            ctx.assets.open(assetPath).bufferedReader().use { it.readText() }
        } catch (_: FileNotFoundException) {
            null
        }
    }

    private fun execSqlScript(db: SQLiteDatabase, script: String) {
        val statements = splitSqlStatements(script)
        for (sql in statements) {
            val s = sql.trim()
            if (s.isNotEmpty()) {
                db.execSQL(s)
            }
        }
    }

    private fun splitSqlStatements(script: String): List<String> {
        val out = ArrayList<String>()
        val sb = StringBuilder()

        var i = 0
        var inSingle = false
        var inDouble = false
        var inLineComment = false
        var inBlockComment = false

        fun peek(offset: Int): Char {
            return if (i + offset < script.length) { script[i + offset] }
                   else { '\u0000' }
        }

        while (i < script.length) {
            val c = script[i]

            if (!inSingle && !inDouble) {
                if (!inBlockComment && !inLineComment && c == '-' && peek(1) == '-') {
                    inLineComment = true; i += 2; continue
                }
                if (!inBlockComment && !inLineComment && c == '/' && peek(1) == '*') {
                    inBlockComment = true; i += 2; continue
                }
                if (inLineComment) {
                    if (c == '\n' || c == '\r') {
                        inLineComment = false
                    }

                    ++i
                    continue
                }
                if (inBlockComment) {
                    if (c == '*' && peek(1) == '/') {
                        inBlockComment = false
                        i += 2
                        continue
                    }

                    ++i
                    continue
                }
            }

            // quotes
            if (!inDouble && c == '\'' ) {
                inSingle = if (inSingle) {
                    if (peek(1) == '\'') {
                        sb.append("''")
                        i += 2
                        continue
                    } else false
                } else true
                sb.append(c); i++; continue
            }

            if (!inSingle && c == '"') {
                inDouble = if (inDouble) {
                    if (peek(1) == '"') {
                        sb.append("\"\"")
                        i += 2
                        continue
                    } else false
                } else true
                sb.append(c); i++; continue
            }

            if (!inSingle && !inDouble && c == ';') {
                out.add(sb.toString())
                sb.setLength(0)
                ++i
                continue
            }

            sb.append(c)
            ++i
        }

        val tail = sb.toString().trim()
        if (tail.isNotEmpty()) {
            out.add(tail)
        }

        return out
    }
}

object DbExecutor {
    suspend fun <T> query(block: (SQLiteDatabase) -> T): T {
        return withContext(Dispatchers.IO) {
            val db = MainVerteDbHelper.instance().readableDatabase
            block(db)
        }
    }

    suspend fun write(block: (SQLiteDatabase) -> Unit) {
        withContext(Dispatchers.IO) {
            val db = MainVerteDbHelper.instance().writableDatabase
            db.beginTransaction()
            try {
                block(db)
                db.setTransactionSuccessful()
            } finally {
                db.endTransaction()
            }
        }
    }
}

fun SQLiteStatement.bindNullableString(index: Int, value: String?) {
    if (value == null) bindNull(index) else bindString(index, value)
}

fun SQLiteStatement.bindNullableLong(index: Int, value: Long?) {
    if (value == null) bindNull(index) else bindLong(index, value)
}

fun SQLiteStatement.bindInt(index: Int, value: Int) {
    bindLong(index, value.toLong())
}

fun SQLiteStatement.bindNullableInt(index: Int, value: Int?) {
    if (value == null) bindNull(index) else bindLong(index, value.toLong())
}


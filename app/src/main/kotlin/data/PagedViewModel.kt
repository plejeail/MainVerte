package com.plej.mainverte.data

import android.database.Cursor
import androidx.core.database.getLongOrNull
import androidx.core.database.getStringOrNull
import androidx.lifecycle.ViewModel
import androidx.lifecycle.viewModelScope
import com.plej.mainverte.Specimen
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.launch

data class PagedState<T>(
    val items: List<T> = emptyList(),
    val isLoading: Boolean = false,
    val endReached: Boolean = false
)

data class Query(val sql: String, val args: Array<String>)

abstract class PagedViewModel<T>(
    private val pageSize: Int = 150
) : ViewModel() {
    protected var currentOffset: Int = 0
    protected var searched: String = ""

    private val _state = MutableStateFlow(PagedState<T>())
    val state: StateFlow<PagedState<T>> = _state

    init {
        // loadNextPage()
    }

    fun updateSearch(search: String) {
        searched = search

        currentOffset = 0

        _state.value = PagedState(
            items = emptyList(),
            isLoading = false,
            endReached = false
        )

        loadNextPage()
    }

    protected abstract fun buildQuery(search: String): Query
    protected abstract fun parseResult(cursor: Cursor): ArrayList<T>

    fun loadNextPage() {
        val current = _state.value

        if (current.isLoading || current.endReached) {
            return
        }

        viewModelScope.launch {
            val searchSnapshot = searched

            _state.value = current.copy(isLoading = true)

            val queryObj = buildQuery(searchSnapshot)
            val finalSql = queryObj.sql + " LIMIT $pageSize OFFSET $currentOffset;"

            val page = DbExecutor.query { db ->
                val cursor = db.rawQuery(finalSql, queryObj.args)
                cursor.use { parseResult(it) }
            }

            if (searchSnapshot != searched) {
                return@launch
            }

            val newEndReached = page.isEmpty()
            currentOffset += page.size

            _state.value = _state.value.copy(
                items = _state.value.items + page,
                isLoading = false,
                endReached = newEndReached
            )
        }
    }
}

class SpecimenViewModel : PagedViewModel<Specimen>() {
    override fun buildQuery(search: String): Query {
        var query = """
            SELECT
                specimen.id              AS specimen_id,
                specimen.name            AS specimen_name,
                specimen.photo_uri       AS specimen_photo,
                specimen.last_watering_at AS specimen_watering_at,
                specimen.last_turning_at AS specimen_last_turning_at,
                species.family           AS species_family,
                species.genus            AS species_genus,
                species.name             AS species_species
            FROM specimen LEFT JOIN species ON specimen.species_id = species.id
        """.trimIndent()
        
        val args = ArrayList<String>()
        if (search.isNotEmpty()) {
            query += " WHERE specimen_name LIKE ?"
            args.add("%$search%")
        }

        query += " ORDER BY last_update"
        return Query(query, args.toTypedArray())
    }

    override fun parseResult(cursor: Cursor): ArrayList<Specimen> {
        val out = ArrayList<Specimen>()
        val idxId       = cursor.getColumnIndexOrThrow("specimen_id")
        val idxName     = cursor.getColumnIndexOrThrow("specimen_name")
        val idxPhoto    = cursor.getColumnIndexOrThrow("specimen_photo")
        val idxWatering = cursor.getColumnIndexOrThrow("specimen_watering_at")
        val idxTurning  = cursor.getColumnIndexOrThrow("specimen_last_turning_at")
        val idxFamily   = cursor.getColumnIndexOrThrow("species_family")
        val idxGenus    = cursor.getColumnIndexOrThrow("species_genus")
        val idxSpecies  = cursor.getColumnIndexOrThrow("species_species")

        while (cursor.moveToNext()) {
            val id       = cursor.getInt(idxId)
            val name     = cursor.getString(idxName)
            val photoUri = cursor.getStringOrNull(idxPhoto)
            val family   = cursor.getStringOrNull(idxFamily)  ?: ""
            val genus    = cursor.getStringOrNull(idxGenus)   ?: ""
            val species  = cursor.getStringOrNull(idxSpecies) ?: ""
            val watering = cursor.getLongOrNull(idxWatering)
            val turning  = cursor.getLongOrNull(idxTurning)
            out.add(Specimen(id, name, photoUri, family, genus, species, watering, turning))
        }

        return out
    }
}

data class SpeciesRow(
    val id: Long,
    val family: String,
    val genus: String,
    val name: String,
) {
    fun scientificName(): String {
        return "$family $genus $name"
    }
}

class SpeciesViewModel : PagedViewModel<SpeciesRow>() {
    override fun buildQuery(search: String): Query {
        var query = "SELECT id, family, genus, name FROM species"
        val args = ArrayList<String>()
        if (search.isNotEmpty()) {
            query += " WHERE slug LIKE ?"
            args.add("%$search%")
        }

        query += " ORDER BY slug"
        return Query(query, args.toTypedArray())
    }

    override fun parseResult(cursor: Cursor): ArrayList<SpeciesRow> {
        val out = ArrayList<SpeciesRow>()
        val idxId   = cursor.getColumnIndexOrThrow("id")
        val idxName   = cursor.getColumnIndexOrThrow("name")
        val idxGenus  = cursor.getColumnIndexOrThrow("genus")
        val idxFamily = cursor.getColumnIndexOrThrow("family")

        while (cursor.moveToNext()) {
            val id      = cursor.getLong(idxId)
            val family  = cursor.getString(idxFamily)
            val genus   = cursor.getString(idxGenus)
            val species = cursor.getString(idxName)
            out.add(SpeciesRow(id, family, genus, species))
        }

        return out
    }
}

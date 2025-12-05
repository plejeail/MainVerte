package com.plej.mainverte.ui

import android.content.Context
import android.graphics.BitmapFactory
import android.net.Uri
import android.os.Environment
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.Image
import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.AddAPhoto
import androidx.compose.material.icons.filled.Delete
import androidx.compose.material.icons.filled.InvertColors
import androidx.compose.material3.DatePicker
import androidx.compose.material3.DatePickerDialog
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.Icon
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.material3.rememberDatePickerState
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.produceState
import androidx.compose.runtime.remember
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.focus.onFocusChanged
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.asImageBitmap
import androidx.compose.ui.layout.ContentScale
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.TextRange
import androidx.compose.ui.text.input.TextFieldValue
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.core.content.FileProvider
import androidx.core.net.toUri
import androidx.lifecycle.viewmodel.compose.viewModel
import com.plej.mainverte.MainVerteApp
import com.plej.mainverte.R
import com.plej.mainverte.Specimen
import com.plej.mainverte.data.DbExecutor
import com.plej.mainverte.data.SpeciesRow
import com.plej.mainverte.data.SpeciesViewModel
import com.plej.mainverte.data.bindInt
import com.plej.mainverte.data.bindNullableString
import com.plej.mainverte.utilities.Logger
import com.plej.mainverte.utilities.deleteSpecimenPhoto
import com.plej.mainverte.utilities.measureTime
import com.plej.mainverte.utilities.photoExists
import java.io.File
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale
import kotlin.collections.find
import kotlin.collections.isNotEmpty
import kotlinx.coroutines.withContext

data object SpecimenDetailScreen : IScreen {
    override val isSearchable  = false
    override val hasSettings   = false
    override val bottomBarMode = BottomBarMode.Confirm
    override val title         = R.string.specimen_title

    var initialPhotoUri: String? = null

    var currentSpecimen: Specimen = Specimen(-1, "", null, "", "", "", null)
        private set

    fun setSpecimen(specimen: Specimen) {
        currentSpecimen = specimen
        initialPhotoUri = currentSpecimen.photoUri
    }

    @Composable
    override fun Draw(navigator: Navigator) {
        SpecimenDetailContent(currentSpecimen)
    }

    override fun onCancel(navigator: Navigator) {
        if (initialPhotoUri != currentSpecimen.photoUri) {
            deleteSpecimenPhoto(MainVerteApp.App, currentSpecimen.photoUri)
        }

        navigator.pop()
    }

    override suspend fun onDelete(navigator: Navigator) {
        DbExecutor.write { db ->
            val stmt = db.compileStatement("DELETE FROM specimen WHERE id = ?")
            stmt.bindInt(1, currentSpecimen.id)
            stmt.executeUpdateDelete()
        }

        val context = MainVerteApp.App
        if (photoExists(context, currentSpecimen.photoUri)) {
            deleteSpecimenPhoto(context, currentSpecimen.photoUri)
        }

        if (photoExists(context, initialPhotoUri)) {
            deleteSpecimenPhoto(context, initialPhotoUri)
        }
    }

    override suspend fun onConfirm(navigator: Navigator) {
        DbExecutor.write { db ->
        measureTime("specimen saved") {
            val stmt = db.compileStatement(
                """
                    INSERT INTO specimen (id, name, photo_uri, species_id)
                    VALUES (
                        ?1, -- specimen id (NULL pour un nouveau)
                        ?2, -- name
                        ?3, -- photo_uri
                        CASE
                            WHEN ?6 IS NULL OR LENGTH(?6) = 0 THEN NULL
                            ELSE (
                                SELECT s.id
                                FROM species s
                                WHERE s.family = ?4
                                AND   s.genus  = ?5
                                AND   s.name   = ?6
                            )
                        END
                    )
                    ON CONFLICT(id) DO UPDATE SET
                        name      = excluded.name,
                        photo_uri = excluded.photo_uri,
                        species_id = excluded.species_id;
                """.trimIndent()
            )

            if (currentSpecimen.id > -1) {
                stmt.bindInt(1, currentSpecimen.id)
            } else {
                stmt.bindNull(1)
            }

            stmt.bindString(2, currentSpecimen.name)
            stmt.bindNullableString(3, currentSpecimen.photoUri)
            stmt.bindString(4, currentSpecimen.family)
            stmt.bindString(5, currentSpecimen.genus)
            stmt.bindString(6, currentSpecimen.species)
            Logger.i { "${currentSpecimen.family} ${currentSpecimen.genus} ${currentSpecimen.species}" }
            stmt.executeInsert()
        }}

        if (initialPhotoUri != currentSpecimen.photoUri) {
            if (photoExists(MainVerteApp.App, initialPhotoUri)) {
                deleteSpecimenPhoto(MainVerteApp.App, initialPhotoUri)
            }
        }

        navigator.pop()
    }

    override fun isDeletable(): Boolean {
        return currentSpecimen.id > -1
    }

    @Composable
    fun SpecimenDetailContent(specimen: Specimen, speciesViewModel: SpeciesViewModel = viewModel()) {
        val scrollState = rememberScrollState()
        var name by rememberSaveable { mutableStateOf(specimen.name) }
        var photoUri by rememberSaveable { mutableStateOf(specimen.photoUri) }

        val initialScientific = remember(specimen) {
            if (specimen.family.isNotBlank() && specimen.genus.isNotBlank() && specimen.species.isNotBlank()) {
                "${specimen.family} ${specimen.genus} ${specimen.species}"
            } else {
                ""
            }
        }

        var speciesField by rememberSaveable(stateSaver = TextFieldValue.Saver) {
            mutableStateOf(TextFieldValue(initialScientific))
        }

        val speciesState by speciesViewModel.state.collectAsState()
        LaunchedEffect(speciesField.text) {
            speciesViewModel.updateSearch(speciesField.text)
        }

        val suggestions: List<SpeciesRow> = speciesState.items.take(6)
        val isSpeciesValid = speciesField.text.isBlank() || speciesState.items.any {
            it.scientificName() == speciesField.text
        }

        for (s in speciesState.items) {
            Logger.i { "${s.scientificName()} vs ${speciesField.text}" }
        }
        var lastWateringAt by rememberSaveable { mutableStateOf(specimen.lastWateringAt) }

        Column(
            modifier = Modifier
                .fillMaxSize()
                .verticalScroll(scrollState)
                .padding(16.dp),
            horizontalAlignment = Alignment.CenterHorizontally
        ) {
            DrawSpecimenPhoto(
                specimen.name,
                photoUri,
                onPhotoCaptured = { newUri ->
                    photoUri = newUri
                    if (specimen.photoUri != initialPhotoUri) {
                        deleteSpecimenPhoto(MainVerteApp.App, specimen.photoUri)
                    }

                    specimen.photoUri = newUri
                    currentSpecimen.photoUri = newUri
                },
                onPhotoDeleted = {
                    photoUri = null
                    if (specimen.photoUri != initialPhotoUri) {
                        deleteSpecimenPhoto(MainVerteApp.App, specimen.photoUri)
                    }

                    specimen.photoUri = null
                    currentSpecimen.photoUri = null
                },
                modifier = Modifier.fillMaxWidth()
            )

            Spacer(Modifier.height(24.dp))

            OutlinedTextField(
                value = name,
                onValueChange = {
                    name = it
                    specimen.name = it
                },
                label = { Text(stringResource(R.string.specimen_name)) },
                singleLine = true,
                modifier = Modifier.fillMaxWidth()
            )

            Spacer(Modifier.height(16.dp))

            SpeciesAutocompleteField(
                value = speciesField,
                onValueChange = {
                    speciesField = it
                    val species = suggestions.find { it2 ->
                        it2.scientificName() == it.text
                    }

                    if (species != null) {
                        specimen.species = species.name
                        specimen.family  = species.family
                        specimen.genus   = species.genus
                    }
                },
                suggestions = suggestions,
                onSuggestionSelected = { suggestion ->
                    val text = suggestion.scientificName()
                    speciesField = TextFieldValue(
                        text = text,
                        selection = TextRange(text.length)
                    )

                    val species = suggestions.find { it2 ->
                        it2.scientificName() == text
                    }

                    if (species != null) {
                        specimen.species = species.name
                        specimen.family  = species.family
                        specimen.genus   = species.genus
                    }
                },
                isValid = isSpeciesValid,
                modifier = Modifier.fillMaxWidth()
            )

            Spacer(Modifier.height(16.dp))

            WateringField(
                lastWateringAt = lastWateringAt,
                onChange = { newValue ->
                    lastWateringAt = newValue
                    specimen.lastWateringAt = newValue
                    currentSpecimen.lastWateringAt = newValue
                },
                modifier = Modifier.fillMaxWidth()
            )
        }
    }
}

@Composable
private fun SpeciesAutocompleteField(
    value: TextFieldValue,
    onValueChange: (TextFieldValue) -> Unit,
    suggestions: List<SpeciesRow>,
    onSuggestionSelected: (SpeciesRow) -> Unit,
    isValid: Boolean,
    modifier: Modifier = Modifier
) {
    var hasFocus by remember { mutableStateOf(false) }
    var hideSuggestions by remember { mutableStateOf(false) }

    Column(modifier = modifier) {
        OutlinedTextField(
            value = value,
            onValueChange = { newValue ->
                onValueChange(newValue)
                hideSuggestions = false
            },
            label = { Text(stringResource(R.string.specimen_scientific_name)) },
            singleLine = true,
            isError = !isValid,
            modifier = Modifier
                .fillMaxWidth()
                .onFocusChanged { focusState ->
                    hasFocus = focusState.isFocused
                    if (!focusState.isFocused) {
                        hideSuggestions = false
                    }
                }
        )

        if (!isValid) {
            Text(
                text = stringResource(R.string.specimen_scientific_name_error),
                style = MaterialTheme.typography.bodySmall,
                color = MaterialTheme.colorScheme.error
            )
        }

        val showSuggestions = hasFocus && !hideSuggestions && suggestions.isNotEmpty()

        if (showSuggestions) {
            Spacer(Modifier.height(4.dp))

            Column(
                modifier = Modifier
                    .fillMaxWidth()
                    .clip(RoundedCornerShape(8.dp))
                    .background(MaterialTheme.colorScheme.surfaceVariant)
            ) {
                suggestions.forEach { suggestion ->
                    Text(
                        text = suggestion.scientificName(),
                        modifier = Modifier
                            .fillMaxWidth()
                            .clickable {
                                onSuggestionSelected(suggestion)
                                hideSuggestions = true
                            }
                            .padding(horizontal = 12.dp, vertical = 8.dp),
                        maxLines = 1,
                        overflow = TextOverflow.Ellipsis,
                        style = MaterialTheme.typography.bodyMedium
                    )
                }
            }
        }
    }
}

@Composable
private fun DrawSpecimenPhoto(
    specimenName: String,
    photoUri: String?,
    onPhotoCaptured: (String) -> Unit,
    onPhotoDeleted: () -> Unit,
    modifier: Modifier = Modifier
) {
    val context = LocalContext.current
    val resolver = context.contentResolver
    val hasValidPhoto = photoExists(context, photoUri)

    var isExpanded by rememberSaveable { mutableStateOf(false) }
    var tempUri by remember { mutableStateOf<Uri?>(null) }

    val takePictureLauncher =
            rememberLauncherForActivityResult(ActivityResultContracts.TakePicture()) { success ->
                if (success && tempUri != null) {
                    onPhotoCaptured(tempUri.toString())
                }
            }

    val bitmap by produceState<android.graphics.Bitmap?>(initialValue = null, photoUri) {
        if (!hasValidPhoto) {
            value = null
            return@produceState
        }
        value = withContext(kotlinx.coroutines.Dispatchers.IO) {
            runCatching {
                resolver.openInputStream(photoUri!!.toUri())?.use { input ->
                    val opts = BitmapFactory.Options().apply {
                        inPreferredConfig = android.graphics.Bitmap.Config.RGB_565
                        inSampleSize = 2 // adjust to target cell size
                    }
                    BitmapFactory.decodeStream(input, null, opts)
                }
            }.getOrNull()
        }
    }

    val boxModifier =
            if (isExpanded) {
                modifier
                    .fillMaxWidth()
                    .fillMaxHeight()
                    // .height(260.dp)
            } else {
                modifier
                    .size(180.dp)
            }

    Box(
        modifier = boxModifier
            .clip(RoundedCornerShape(16.dp))
            .background(Color(0xFFE0E0E0))
            .clickable {
                isExpanded = !isExpanded
            },
        contentAlignment = Alignment.Center
    ) {
        if (hasValidPhoto) {
            if (bitmap != null) {
                Image(
                    bitmap = bitmap!!.asImageBitmap(),
                    contentDescription = null,
                    modifier = Modifier.fillMaxSize(),
                    contentScale = if (isExpanded) { ContentScale.Fit } else { ContentScale.Crop }
                )
            } else {
                Icon(
                    painter = painterResource(R.drawable.generic_plant),
                    contentDescription = null,
                    modifier = Modifier.fillMaxSize(0.4f)
                )
            }
        } else {
            Icon(
                painter = painterResource(R.drawable.generic_plant),
                contentDescription = null,
                modifier = Modifier.fillMaxSize(0.4f)
            )
        }

        if (isExpanded) {
            Icon(
                imageVector = Icons.Default.AddAPhoto,
                contentDescription = stringResource(R.string.specimen_take_photo),
                modifier = Modifier
                    .align(Alignment.TopEnd)
                    .padding(8.dp)
                    .size(32.dp)
                    .clickable {
                        val (_, uri) = createSpecimenPhotoFile(context, specimenName)
                        tempUri = uri
                        takePictureLauncher.launch(uri)
                    }
            )

            if (hasValidPhoto) {
                Icon(
                    imageVector = Icons.Default.Delete,
                    contentDescription = stringResource(R.string.specimen_delete_photo),
                    modifier = Modifier
                        .align(Alignment.TopStart)
                        .padding(8.dp)
                        .size(32.dp)
                        .clickable {
                            onPhotoDeleted()
                        }
                )
            }
        }
    }
}

private fun createSpecimenPhotoFile(context: Context, specimenName: String): Pair<File, Uri> {
    val picturesDir = context.getExternalFilesDir(Environment.DIRECTORY_PICTURES)
    val dir = File(picturesDir, "specimens")

    if (!dir.exists()) {
        dir.mkdirs()
    }

    val timeStamp = SimpleDateFormat("yyyyMMdd_HHmmss", Locale.US).format(Date())
    val safeName = specimenName.ifBlank { "specimen" }
        .replace(Regex("[^a-zA-Z0-9_-]"), "_")

    val file = File(dir, "${safeName}_$timeStamp.jpg")

    val uri = FileProvider.getUriForFile(
        context,
        "${context.packageName}.fileprovider",
        file
    )

    return file to uri
}

@OptIn(ExperimentalMaterial3Api::class)
@Composable
private fun WateringField(
    lastWateringAt: Long?,
    onChange: (Long?) -> Unit,
    modifier: Modifier = Modifier
) {
    val dateFormatter = remember {
        SimpleDateFormat("yyyy-MM-dd", Locale.getDefault())
    }

    var showDatePicker by remember { mutableStateOf(false) }

    val datePickerState = rememberDatePickerState(
        initialSelectedDateMillis = lastWateringAt ?: System.currentTimeMillis()
    )

    if (showDatePicker) {
        DatePickerDialog(
            onDismissRequest = { showDatePicker = false },
            confirmButton = {
                TextButton(
                    onClick = {
                        val millis = datePickerState.selectedDateMillis
                        onChange(millis)
                        showDatePicker = false
                    }
                ) {
                    Text(stringResource(android.R.string.ok))
                }
            },
            dismissButton = {
                TextButton(
                    onClick = { showDatePicker = false }
                ) {
                    Text(stringResource(android.R.string.cancel))
                }
            }
        ) {
            DatePicker(state = datePickerState)
        }
    }

    val text = if (lastWateringAt == null) {
        stringResource(R.string.specimen_watering_unknown)
    } else {
        val date = Date(lastWateringAt)

        val now = System.currentTimeMillis()
        val millisPerDay = 24L * 60L * 60L * 1000L
        val days = (now - lastWateringAt) / millisPerDay
        val daysLabel = stringResource(R.string.specimen_days)
        "${dateFormatter.format(date)} ($days $daysLabel)"
    }

    Row(
        modifier = modifier,
        verticalAlignment = Alignment.CenterVertically,
        horizontalArrangement = Arrangement.Start
    ) {
        Icon(
            imageVector = Icons.Default.InvertColors,
            contentDescription = stringResource(R.string.specimen_last_watering_icon),
            modifier = Modifier.size(24.dp)
        )

        Spacer(Modifier.width(12.dp))

        Column(
            modifier = Modifier
                .weight(1f)
                .clickable { showDatePicker = true }
        ) {
            Text(
                text = stringResource(R.string.specimen_last_watering),
                style = MaterialTheme.typography.labelSmall,
                color = MaterialTheme.colorScheme.onSurfaceVariant
            )
            Text(
                text = text,
                style = MaterialTheme.typography.bodyMedium
            )
        }

        Spacer(Modifier.width(8.dp))

        TextButton(
            onClick = { onChange(System.currentTimeMillis()) }
        ) {
            Text(stringResource(R.string.specimen_watering_today))
        }
    }
}

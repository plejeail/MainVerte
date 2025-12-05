package com.plej.mainverte.ui

import android.graphics.BitmapFactory
import androidx.compose.foundation.Image
import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.aspectRatio
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.lazy.grid.GridCells
import androidx.compose.foundation.lazy.grid.LazyVerticalGrid
import androidx.compose.foundation.lazy.grid.itemsIndexed
import androidx.compose.foundation.lazy.grid.rememberLazyGridState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Add
import androidx.compose.material3.CircularProgressIndicator
import androidx.compose.material3.Icon
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.runtime.produceState
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.asImageBitmap
import androidx.compose.ui.layout.ContentScale
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.core.net.toUri
import androidx.lifecycle.viewmodel.compose.viewModel
import com.plej.mainverte.R
import com.plej.mainverte.Specimen
import com.plej.mainverte.data.SpecimenViewModel
import kotlinx.coroutines.withContext

data object SpecimenGridScreen : IScreen {
    override val isSearchable  = true
    override val hasSettings   = true
    override val bottomBarMode = BottomBarMode.Navigation
    override val title         = R.string.collection_title

    @Composable
    override fun Draw(navigator: Navigator) {
        DrawCollectionGrid(navigator.searchString,
           onItemClick = { row ->
                navigator.push(SpecimenDetailScreen(row))
           },
           onAddClick  = {
                navigator.push(SpecimenDetailScreen(Specimen(-1, "", null, "", "", "", null)))
           }
        )
    }
}

@Composable
fun DrawCollectionGrid(
    searched: String,
    onItemClick: (Specimen) -> Unit,
    onAddClick: () -> Unit,
    modifier: Modifier = Modifier,
    viewModel: SpecimenViewModel = viewModel(),
) {
    LaunchedEffect(searched) {
        viewModel.updateSearch(searched)
    }

    val state by viewModel.state.collectAsState()
    val listState = rememberLazyGridState()

    LazyVerticalGrid(
        state                 = listState,
        columns               = GridCells.Adaptive(minSize = 140.dp),
        modifier              = modifier,
        horizontalArrangement = Arrangement.spacedBy(12.dp),
        verticalArrangement   = Arrangement.spacedBy(12.dp),
        contentPadding        = PaddingValues(12.dp),
    ) {
        itemsIndexed(
            items = state.items,
            key = { _, item -> item.id } // stable key
        ) { _, item ->
            DrawSpecimenGridCell(
                name     = item.name,
                photoUri = item.photoUri,
                onClick = { onItemClick(item) }
            )
        }

        item(key = "MV%ADD%SPECIMEN%") {
            AddSpecimenGridCell(onClick = onAddClick)
        }
    }
}

@Composable
private fun DrawSpecimenGridCell(
    name: String,
    photoUri: String?,
    onClick: () -> Unit,
    modifier: Modifier = Modifier
) {
    val context = LocalContext.current
    val resolver = context.contentResolver
    val hasPhoto = !photoUri.isNullOrBlank()

    val bitmap by produceState<android.graphics.Bitmap?>(initialValue = null, photoUri) {
        if (!hasPhoto) {
            value = null
            return@produceState
        }

        value = withContext(kotlinx.coroutines.Dispatchers.IO) {
            runCatching {
                val uri = photoUri.toUri()
                resolver.openInputStream(uri)?.use { input ->
                    val options = BitmapFactory.Options().apply {
                        inPreferredConfig = android.graphics.Bitmap.Config.RGB_565
                        inSampleSize = 2 // adjust if you know target cell size
                    }
                    BitmapFactory.decodeStream(input, null, options)
                }
            }.getOrNull()
        }
    }

    Column(
        modifier = modifier
            .fillMaxWidth()
            .clickable(onClick = onClick),
        horizontalAlignment = Alignment.CenterHorizontally
    ) {
        Box(
            modifier = Modifier
                .aspectRatio(1f)
                .clip(RoundedCornerShape(12.dp))
                .background(Color(0xFFE0E0E0)),
            contentAlignment = Alignment.Center
        ) {
            when {
                bitmap != null -> Image(
                    bitmap = bitmap!!.asImageBitmap(),
                    contentDescription = null,
                    modifier = Modifier.fillMaxSize(),
                    contentScale = ContentScale.Crop
                )
                hasPhoto -> CircularProgressIndicator(modifier = Modifier.fillMaxSize(0.2f))
                else -> Icon(
                    painter = painterResource(R.drawable.generic_plant),
                    contentDescription = null,
                    modifier = Modifier.fillMaxSize(0.6f)
                )
            }
        }

        Spacer(Modifier.height(4.dp))

        Text(
            text = name,
            maxLines = 1,
            overflow = TextOverflow.Ellipsis,
            style = MaterialTheme.typography.bodySmall,
            textAlign = TextAlign.Center,
            modifier = Modifier.fillMaxWidth()
        )
    }
}

@Composable
private fun AddSpecimenGridCell(
    onClick: () -> Unit,
    modifier: Modifier = Modifier
) {
    Column(
        modifier = modifier
            .fillMaxWidth()
            .clickable(onClick = onClick),
        horizontalAlignment = Alignment.CenterHorizontally
    ) {
        Box(
            modifier = Modifier
                .aspectRatio(1f)
                .clip(RoundedCornerShape(12.dp))
                .background(Color(0xFFE8F5E9)), // un peu "MainVerte"
            contentAlignment = Alignment.Center
        ) {
            Icon(
                imageVector = Icons.Default.Add,
                contentDescription = null,
                modifier = Modifier.fillMaxSize(0.4f)
            )
        }

        Spacer(Modifier.height(4.dp))

        Text(
            text = "",
            style = MaterialTheme.typography.bodySmall,
            textAlign = TextAlign.Center,
            modifier = Modifier.fillMaxWidth()
        )
    }
}

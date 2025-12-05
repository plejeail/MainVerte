package com.plej.mainverte.ui

import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.itemsIndexed
import androidx.compose.foundation.lazy.rememberLazyListState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.Card
import androidx.compose.material3.CardDefaults
import androidx.compose.material3.CircularProgressIndicator
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.lifecycle.viewmodel.compose.viewModel
import com.plej.mainverte.R
import com.plej.mainverte.data.SpeciesRow
import com.plej.mainverte.data.SpeciesViewModel

data object SpeciesGridScreen : IScreen {
    override val isSearchable  = true
    override val hasSettings   = true
    override val bottomBarMode = BottomBarMode.Navigation
    override val title         = R.string.species_title

    @Composable
    override fun Draw(navigator: Navigator) {
        DrawGrid(navigator.searchString)
    }

    @Composable
    fun DrawGrid(searchString: String, viewModel: SpeciesViewModel = viewModel()) {
        LaunchedEffect(searchString) {
            viewModel.updateSearch(searchString)
        }

        val state by viewModel.state.collectAsState()
        val listState = rememberLazyListState()

        Box {
            LazyColumn(
                state = listState,
            ) {
                itemsIndexed(
                    items = state.items,
                    key = { _, item -> item.id }
                ) { index, item ->
                    DrawSpeciesRow(item) {
                    }

                    if (!state.isLoading && index >= state.items.size - 20) {
                        viewModel.loadNextPage()
                    }
                }

                if (state.isLoading) {
                    item {
                        Box(
                            modifier = Modifier
                                .fillMaxWidth()
                                .padding(16.dp),
                            contentAlignment = Alignment.Center
                        ) {
                            CircularProgressIndicator()
                        }
                    }
                } else if (state.items.isEmpty()) {
                    item {
                        Box(
                            modifier = Modifier
                                .fillMaxWidth()
                                .padding(16.dp),
                            contentAlignment = Alignment.Center
                        ) {
                            Text(stringResource(R.string.species_not_found))
                        }
                    }
                }
            }
        }
    }
}

@Composable
fun DrawSpeciesRow(
    species: SpeciesRow,
    onClick: () -> Unit
) {
    Card(
        modifier = Modifier
            .fillMaxWidth()
            .clickable(onClick = onClick),
        shape = RoundedCornerShape(8.dp),
        elevation = CardDefaults.cardElevation(defaultElevation = 2.dp)
    ) {
        Row(
            modifier = Modifier
                .fillMaxWidth()
                .padding(12.dp),
            verticalAlignment = Alignment.CenterVertically
        ) {
            Column(
                modifier = Modifier
                    .weight(1f)
            ) {
                Text(
                    text = species.scientificName(),
                    style = MaterialTheme.typography.titleMedium,
                    maxLines = 1,
                    overflow = TextOverflow.Ellipsis
                )
            }
        }
    }
}

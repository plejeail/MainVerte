package com.plej.mainverte

import android.app.Application
import android.database.sqlite.SQLiteDatabase
import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.WindowInsets
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.filled.ArrowBack
import androidx.compose.material.icons.filled.Check
import androidx.compose.material.icons.filled.Close
import androidx.compose.material.icons.filled.CollectionsBookmark
import androidx.compose.material.icons.filled.Delete
import androidx.compose.material.icons.filled.Search
import androidx.compose.material.icons.filled.Settings
import androidx.compose.material3.Button
import androidx.compose.material3.ButtonDefaults.buttonColors
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.MaterialTheme.colorScheme
import androidx.compose.material3.NavigationBar
import androidx.compose.material3.NavigationBarItem
import androidx.compose.material3.Scaffold
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.material3.TextField
import androidx.compose.material3.TopAppBar
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.font.FontStyle.Companion.Italic
import androidx.compose.ui.text.input.ImeAction
import androidx.compose.ui.unit.dp
import com.plej.mainverte.data.MainVerteDbHelper
import com.plej.mainverte.ui.*
import com.plej.mainverte.utilities.measureTime
import kotlinx.coroutines.launch

class MainActivity : ComponentActivity() {
    override fun onDestroy() {
        super.onDestroy()
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        measureTime("activity creation") {
            super.onCreate(savedInstanceState)
            val dbHelper = MainVerteDbHelper.initialize(this)
            dbHelper.triggerUpdate()

            enableEdgeToEdge()
            setContent {
                val navigator = androidx.compose.runtime.saveable.rememberSaveable(
                    saver = com.plej.mainverte.ui.navigatorSaver()
                ) { Navigator(SpecimenGridScreen) }

                DrawFullScreen(navigator)
            }
        }
    }
}

@Composable
private fun DrawFullScreen(navigator: Navigator) {
    val currentScreen = navigator.current()

    Scaffold(
        contentWindowInsets = WindowInsets(0, 0, 0, 0),
        topBar = { DrawTopBar(navigator, navigator.searchString) { q ->
                     navigator.searchString = q
                 } },
        bottomBar = { DrawBottomBar(navigator) },
    ) { padding -> Column(Modifier.padding(padding).fillMaxSize()) {
            currentScreen.Draw(navigator)
        }
    }
}

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun DrawTopBar(navigator: Navigator,
               searchQuery: String,
               onSearchQueryChange: (String) -> Unit) {
    val scope = rememberCoroutineScope()
    var searchExpanded by rememberSaveable { mutableStateOf(false) }
    val current = navigator.current()

    // Collapse search when the active screen is not searchable.
    LaunchedEffect(current) {
        if (!current.isSearchable && searchExpanded) {
            searchExpanded = false
        }
    }

    TopAppBar(
        windowInsets = WindowInsets(0, 100, 0, 0),
        title = {
            if (searchExpanded && current.isSearchable) {
                TextField(
                    value = searchQuery,
                    onValueChange = onSearchQueryChange,
                    singleLine = true,
                    placeholder = { Text(stringResource(R.string.bar_search)) },
                    modifier = Modifier.fillMaxWidth(),
                    keyboardOptions = KeyboardOptions(
                        imeAction = ImeAction.Search
                    ),
                    trailingIcon = {
                        if (searchQuery.isNotEmpty()) {
                            IconButton(onClick = { onSearchQueryChange("") }) {
                                Icon(
                                    imageVector = Icons.Default.Close,
                                    contentDescription = "Clear"
                                )
                            }
                        }
                    }
                )
            } else {
                Column {
                    Row(verticalAlignment = Alignment.CenterVertically) {
                        Text(
                            text = stringResource(current.title),
                            style = MaterialTheme.typography.titleLarge
                        )

                        if (searchQuery.isNotEmpty()) {
                            Spacer(modifier = Modifier.width(8.dp))

                            Text(
                                text = searchQuery,
                                style = MaterialTheme.typography.bodySmall.copy(
                                    fontStyle = Italic
                                ),
                                color = colorScheme.onSurfaceVariant
                            )
                        }
                    }
                }
            }
         },
        navigationIcon = {
            if (navigator.canGoBack()) {
                IconButton(
                    onClick = { navigator.pop() }
                ) {
                    Icon(Icons.AutoMirrored.Filled.ArrowBack, contentDescription = "Back/Cancel")
                }
            } else {
                Icon(
                    painter = painterResource(id = R.mipmap.launcher_mainverte_foreground),
                    contentDescription = "MainVerte logo",
                    modifier = Modifier.size(64.dp),
                    tint = Color.Unspecified,
                )
            }
        },
        actions = {
            if (current.isSearchable) {
                IconButton(onClick = { searchExpanded = !searchExpanded }) {
                    Icon(Icons.Default.Search, contentDescription = "Search")
                }
            }

            if (current.hasSettings) {
                IconButton(onClick = { navigator.push(SettingsScreen) }) {
                    Icon(Icons.Default.Settings, contentDescription = "Settings")
                }
            }

            if (current.isDeletable()) {
                IconButton(
                    onClick = {
                        scope.launch {
                            try {
                                current.onDelete(navigator)
                                navigator.pop()
                            } catch (_: Throwable) {
                                // TODO: surface feedback/log the failure
                            }
                        }
                    }
                ) {
                    Icon(Icons.Default.Delete, contentDescription = "Delete")
                }
            }
        }

    )
}

@Composable
fun DrawBottomBar(navigator: Navigator) {
    val scope = rememberCoroutineScope()
    val current = navigator.current()
    when (current.bottomBarMode) {
        BottomBarMode.None       -> Unit

        BottomBarMode.Navigation -> {
            NavigationBar(
                windowInsets = WindowInsets(0, 0, 0, 6),
            ) {
                NavigationBarItem(
                    selected = current is SpecimenGridScreen,
                    onClick = { navigator.replace(SpecimenGridScreen) },
                    icon = { Icon(Icons.Default.CollectionsBookmark, contentDescription = "Collection") },
                    label = { Text("Collection") },
                )

                NavigationBarItem(
                    selected = current is SpeciesGridScreen,
                    onClick = { navigator.replace(SpeciesGridScreen) },
                    icon = { Icon(Icons.Default.CollectionsBookmark, contentDescription = "Species") },
                    label = { Text("Species") },
                )
            }
        }

        BottomBarMode.Confirm    -> {
            ConfirmBar(
                onCancel  = { current.onCancel(navigator)  },
                onConfirm = { scope.launch { current.onConfirm(navigator) } }
            )
        }
    }

}

@Composable
private fun ConfirmBar(onCancel: () -> Unit, onConfirm: () -> Unit) {
    Surface(tonalElevation = 3.dp) {
        Row(
            Modifier
                .fillMaxWidth()
                .padding(horizontal = 12.dp, vertical = 8.dp),
            horizontalArrangement = Arrangement.SpaceBetween,
        ) {
            Button(
                onClick = onCancel,
                colors = buttonColors(containerColor = colorScheme.surfaceVariant),
            ) {
                Icon(Icons.Default.Close, contentDescription = null)
                Spacer(Modifier.width(8.dp))
                Text("Cancel")
            }

            Button(onClick = onConfirm) {
                Icon(Icons.Default.Check, contentDescription = null)
                Spacer(Modifier.width(8.dp))
                Text("Validate")
            }
        }
    }
}

class MainVerteApp : Application() {
    override fun onCreate() {
        super.onCreate()
        App = this
    }

    companion object {
        lateinit var App: MainVerteApp
            private set
    }
}

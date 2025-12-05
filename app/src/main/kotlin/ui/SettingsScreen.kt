package com.plej.mainverte.ui

import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import com.plej.mainverte.R

data object SettingsScreen : IScreen {
    override val isSearchable  = false
    override val hasSettings   = false
    override val bottomBarMode = BottomBarMode.Navigation
    override val title         = R.string.settings_title

    @Composable
    override fun Draw(navigator: Navigator) {
        Text(
            text = "SETTINGS",
            style = MaterialTheme.typography.titleLarge
        )
    }
}
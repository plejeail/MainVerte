package com.plej.mainverte.ui

import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateListOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.setValue
import com.plej.mainverte.utilities.expectDead

/**  */
enum class BottomBarMode {
    None,
    Navigation,
    Confirm,
}

/**
 * Contract between a screen and the main activity. Each screen declares whether it supports search,
 * what kind of bottom bar it wants, and how to react to global actions (search / confirm / cancel).
 *
 * This interface is intentionally low-level to keep  control in the Activity rather than using a navigation framework.
 */
sealed interface IScreen {
    /** Indicates whether this screen supports the search bar. */
    val isSearchable  : Boolean
    /** Show the settings button. */
    val hasSettings   : Boolean
    /** Determines which type of bottom bar should be shown for this screen. */
    val bottomBarMode : BottomBarMode
    /** Title used (string resource id). */
    val title         : Int

    /** Method used to draw the screen. */
    @Composable
    fun Draw(navigator: Navigator)

    /** Method called when the user presses the global 'Confirm' button (bottom bar in confirm mode). */
    suspend fun onConfirm(navigator: Navigator) { expectDead() }
    /** Method called when the user presses the global 'Cancel' button (bottom bar in confirm mode). */
    fun onCancel(navigator: Navigator) { expectDead() }

    /** What to do on clicking the trash button. */
    suspend fun onDelete(navigator: Navigator) { expectDead() }

    /** Returns true if the screen should display a trash button. */
    fun isDeletable(): Boolean { return false }
}

/**
 *
*/
class Navigator(initial: IScreen) {
    private val _backstack = mutableStateListOf(initial)
    var searchString  by mutableStateOf("")

    /** Whether the navigator has a previous screen or not. */
    fun canGoBack(): Boolean { return _backstack.size > 1 }

    /** Get currently displayed screen. */
    @Composable fun current(): IScreen { return _backstack.last() }

    /** Replace current screen. */
    fun replace(s: IScreen) {
        searchString = ""
        _backstack[_backstack.lastIndex] = s
    }

    /** Add a new current screen. */
    fun push(s: IScreen) {
        searchString = ""
        _backstack += s
    }

    /** Go back to previous screen. */
    fun pop() {
        if (canGoBack()) {
            searchString = ""
            _backstack.removeAt(_backstack.lastIndex)
        }
    }

    /** Return the backstack as a list of simple class names for saving. */
    fun backstackNames(): List<String> = _backstack.map { it.javaClass.simpleName }
}

/** Helpers to save/restore Navigator via rememberSaveable. */
fun navigatorSaver(): androidx.compose.runtime.saveable.Saver<Navigator, *> {
    return androidx.compose.runtime.saveable.Saver(
        save = { nav ->
            // Save the backstack as a list of screen ids and the search string
            val names = nav.backstackNames()
            listOf(names, nav.searchString)
        },
        restore = { state ->
            try {
                @Suppress("UNCHECKED_CAST")
                val pair = state as List<Any>
                val names = pair[0] as List<String>
                val search = pair[1] as String
                val first = resolveScreenByName(names.firstOrNull() ?: "SpecimenGridScreen")
                val nav = Navigator(first)
                nav.searchString = search
                // push remaining screens (if any)
                for (i in 1 until names.size) {
                    resolveScreenByName(names[i])?.let { nav.push(it) }
                }
                nav
            } catch (e: Throwable) {
                Navigator(resolveScreenByName("SpecimenGridScreen")!!)
            }
        }
    )
}



/** Resolve a screen singleton from its simple class name. */
fun resolveScreenByName(name: String): IScreen? {
    return when (name) {
        "SpecimenGridScreen" -> com.plej.mainverte.ui.SpecimenGridScreen
        "SpeciesGridScreen"  -> com.plej.mainverte.ui.SpeciesGridScreen
        "SettingsScreen"     -> com.plej.mainverte.ui.SettingsScreen
        else -> null
    }
}

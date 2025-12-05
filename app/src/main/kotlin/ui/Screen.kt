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
    var searchVersion by mutableIntStateOf(0)

    /** Whether the navigator has a previous screen or not. */
    fun canGoBack(): Boolean { return _backstack.size > 1 }

    /** Get currently displayed screen. */
    @Composable fun current(): IScreen { return _backstack.last() }

    /** Replace current screen. */
    fun replace(s: IScreen) {
        searchString = ""
        ++searchVersion
        _backstack[_backstack.lastIndex] = s
    }

    /** Add a new current screen. */
    fun push(s: IScreen) {
        searchString = ""
        ++searchVersion
        _backstack += s
    }

    /** Go back to previous screen. */
    fun pop() {
        if (canGoBack()) {
            searchString = ""
            ++searchVersion
            _backstack.removeAt(_backstack.lastIndex)
        }
    }
}

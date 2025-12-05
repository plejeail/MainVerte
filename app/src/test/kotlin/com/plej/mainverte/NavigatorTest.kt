package com.plej.mainverte

import com.plej.mainverte.ui.SpecimenGridScreen
import com.plej.mainverte.ui.SpecimenDetailScreen
import com.plej.mainverte.ui.Navigator
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class NavigatorTest {
    @Test
    fun pushAndPopBackstackBehavior() {
        val nav = Navigator(SpecimenGridScreen)
        assertFalse(nav.canGoBack())
        assertEquals(listOf(SpecimenGridScreen.javaClass.simpleName), nav.backstackNames())

        nav.push(SpecimenDetailScreen)
        assertTrue(nav.canGoBack())
        assertEquals(2, nav.backstackNames().size)

        nav.pop()
        assertFalse(nav.canGoBack())
        assertEquals(1, nav.backstackNames().size)
    }
}

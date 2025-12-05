package com.plej.mainverte

import com.plej.mainverte.ui.Navigator
import com.plej.mainverte.ui.SpecimenGridScreen
import com.plej.mainverte.ui.SpecimenDetailScreen
import com.plej.mainverte.ui.navigatorSaver
import org.junit.Assert.*
import org.junit.Test

class NavigatorSaverTest {
    @Test
    fun saverSavesAndRestoresBackstackAndSearch() {
        val nav = Navigator(SpecimenGridScreen)
        nav.push(SpecimenDetailScreen)
        nav.searchString = "monkey"

        val saver = navigatorSaver()
        val saved = saver.save(nav)
        assertNotNull(saved)

        val restored = saver.restore(saved!!)
        assertNotNull(restored)
        assertEquals(2, restored.backstackNames().size)
        assertEquals("monkey", restored.searchString)
    }
}

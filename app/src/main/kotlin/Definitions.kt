//! Definitions.kt ------------------------------------------------------------
//!
//! ----------------------------------------------------------------- MainVerte
package com.plej.mainverte

enum class ClimateZone {
    Unknown,
    Temperate,
    Tropical,
    Montane,
    Desert,
    Subarctic,
}

enum class Moisture {
    Moderate,
    Wet,
    Dry,
    SeasonallyWet,
    SeasonallyDry,
}

enum class LifeTime {
    Unknown,
    Annual,
    Biennial,
    Perennial,
    Monocarpic,
}

enum class Shape {
    Unknown,
    Bamboo,
    Bulbous,
    Caudiciform,
    Climber,
    Herb,
    Rhizomatous,
    SemiSucculent,
    Succulent,
    Shrub,
    Tree,
    Tuberous,
}

enum class Region {
    Unknown,
    Africa,
    Antarctica,
    Australasia,
    CentralAmerica,
    CentralAsia,
    EastAsia,
    Europe,
    Mediterranean,
    MiddleEast,
    NorthAmerica,
    PacificIslands,
    SouthAmerica,
    SouthAsia,
    SoutheastAsia,
    Subantarctic,
}

class Species (
    val name        : String,
    val genus       : String,
    val family      : String,
    val photoUri    : String?,
    val climateZone : ClimateZone,
    val moisture    : Moisture,
    val lifeTime    : LifeTime,
    val shape       : Shape,
    val origin      : Region,
)

class Specimen (
    val id       : Int,
    var name     : String,
    var photoUri : String?,
    var species  : String,
    var family   : String,
    var genus    : String,
    var lastWateringAt: Long?,
)

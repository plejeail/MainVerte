# MainVerte Icon Design Guidelines

This document defines the visual and technical rules for icons used throughout the MainVerte application.

The objective is to maintain a coherent visual language across care rules, actions, navigation elements and future features.

---

## 1. Design philosophy

MainVerte icons should feel:

* simple
* organic
* calm
* modern
* immediately readable
* consistent with the Material visual language
* distinctive enough to belong to MainVerte

The preferred direction is inspired by **Material Symbols Rounded**, without requiring icons to reproduce existing Material symbols exactly.

Icons should prioritize **recognition and silhouette** over detail.

At small sizes, every element must have a clear semantic purpose.

---

## 2. Default icon style

MainVerte uses a **filled icon style with negative-space details**.

### Primary shape

Each icon should contain one strong, filled primary silhouette representing the main object or action.

Examples:

* watering can → watering
* fertilizer bottle → fertilizing
* flower pot → repotting

The primary silhouette should remain recognizable even if all internal details are removed.

### Secondary symbol

When additional semantic information is useful, it should preferably be represented as a **negative-space cutout inside the main silhouette**.

Example:

* watering can + negative-space water droplet

This approach is preferred over adding several independent shapes around the primary object.

The secondary symbol should clarify the meaning of the icon, not merely decorate it.

---

## 3. Geometry

### Canvas

Icons are designed on a:

```text
24 × 24
```

viewport.

This matches the standard Material icon coordinate system and simplifies integration with Android UI components.

### Optical bounds

The visible drawing should normally remain approximately inside:

```text
2 → 22
```

on both axes.

Small exceptions are acceptable when required for optical balance.

Icons should not mechanically fill the entire canvas.

### Visual weight

All icons in a set should have approximately the same perceived mass.

A very dense icon should not appear beside a very thin or fragile icon.

Optical consistency takes priority over mathematically identical surface area.

---

## 4. Shape language

MainVerte favors **soft geometric forms**.

Use:

* rounded corners
* smooth transitions
* simple curves
* compact silhouettes
* moderate asymmetry when natural for the represented object

Avoid:

* sharp decorative corners
* excessive mechanical geometry
* thin protruding elements
* complex contours
* ornamental details

The result should remain organic without becoming cartoonish.

---

## 5. Corners and curves

Corners should generally follow the visual character of **Material Rounded**.

Prefer small radii rather than completely circular shapes.

Rounded geometry should soften the object without destroying its structure.

For example, a watering can may have:

* a slightly rounded rectangular body
* a smooth handle
* a simplified tapered spout

It should not look inflated or toy-like.

---

## 6. Negative space

Negative space is a core part of the MainVerte icon language.

Internal cutouts should:

* have simple silhouettes
* remain readable at small sizes
* have sufficient clearance from surrounding edges
* represent meaningful information

Avoid very narrow negative-space channels.

As a practical rule, an internal cutout should remain clearly identifiable when the icon is displayed at approximately **20–24 px**.

---

## 7. Detail budget

Every detail must survive reduction.

Before retaining a detail, ask:

> Does removing this detail make the icon meaningfully harder to understand?

If not, remove it.

Avoid:

* tiny holes
* thin lines
* multiple small droplets
* texture
* realistic object construction
* decorative ridges
* unnecessary segmentation

A MainVerte icon should normally contain:

1. one primary silhouette
2. optionally one secondary semantic cutout
3. only the minimum structural details required for recognition

---

## 8. Filled versus outline icons

### Default

MainVerte-specific functional icons use:

**Filled silhouette + negative-space detail**

This is the canonical MainVerte icon style.

### Outline icons

Outline icons may still be used when relying directly on standard Material Symbols, especially for generic UI actions such as:

* back
* close
* menu
* search
* settings
* edit

Do not redraw generic UI symbols solely to make them MainVerte-specific.

MainVerte's custom visual language should primarily be applied to **domain-specific horticultural concepts**.

---

## 9. Icon categories

### Application UI icons

Generic interface concepts should normally use standard Material Symbols.

Examples:

```text
Back
Search
Settings
Edit
Delete
More
Add
```

### Horticultural icons

Plant-care concepts should use MainVerte custom icons whenever possible.

Examples:

```text
Watering
Fertilizing
Repotting
Pot rotation
Pruning
Misting
Humidity
Light
Temperature
```

These icons form the distinctive MainVerte visual family.

---

## 10. Semantic construction

Whenever possible, care icons should follow:

```text
OBJECT + CARE CONCEPT
```

The object provides immediate recognition.

The secondary symbol provides semantic precision.

Example:

```text
Watering can
    +
Water droplet cutout
    =
Watering
```

Avoid abstract representations when an obvious physical metaphor exists.

The icon should generally be understandable without a label.

---

## 11. Orientation

Objects should use a consistent natural orientation.

When direction is arbitrary, prefer:

```text
left → right
```

For example, the watering can spout should point toward the right.

This follows the natural reading direction of the application and gives the icon family a consistent visual flow.

Do not mirror icons without a semantic reason.

---

## 12. Color

Icons are designed as **monochrome vector assets**.

Color must be applied by the UI layer rather than encoded into the SVG geometry.

Icons should work correctly in:

* light theme
* dark theme
* enabled state
* disabled state
* selected state
* alert/error contexts

An icon must never rely on multiple colors to remain understandable.

---

## 13. SVG requirements

Source icons should preferably be stored as SVG.

Requirements:

```text
viewBox="0 0 24 24"
```

Prefer:

* paths
* simple vector geometry
* minimal path count
* no embedded raster images
* no gradients
* no shadows
* no filters
* no baked background
* no baked UI color

SVG files should be easy to recolor from the application.

---

## 14. Small-size validation

Every custom icon must be visually checked at:

```text
16 px
20 px
24 px
32 px
48 px
```

The **24 px rendering is the primary reference**.

At 20–24 px:

* the silhouette must remain immediately recognizable
* the negative-space symbol must remain visible
* no elements should visually merge accidentally
* no important detail should disappear

If an icon only works at 48 px, it is too complex.

---

## 15. Optical consistency

Icons should be evaluated together, not independently.

When adding a new icon, compare it against existing MainVerte icons for:

* apparent size
* density
* corner softness
* negative-space size
* silhouette complexity
* center of visual mass

Perfect geometric uniformity is less important than consistent perception.

---

## 16. Naming convention

Custom icon files should use clear lowercase snake_case names.

Examples:

```text
watering.svg
fertilizing.svg
repotting.svg
pot_rotation.svg
misting.svg
humidity.svg
```

Avoid implementation-specific names such as:

```text
icon1.svg
watering_v3_final.svg
new_water.svg
```

Git provides version history; filenames should describe semantic identity, not revision history.

---

## 17. Watering icon reference

The watering icon establishes the initial reference for the MainVerte care icon family.

Concept:

```text
compact watering can
+
negative-space water droplet
```

Design rules:

* compact filled body
* small secondary handle
* simplified spout pointing upward and to the right
* simplified watering head
* rounded Material-like geometry
* centered water-droplet cutout in the reservoir
* no streams of water leaving the spout
* no decorative construction details

The watering can must remain recognizable from its external silhouette alone.

The droplet reinforces the care concept without competing with the primary shape.

---

## 18. Design test

Before accepting a custom icon, verify:

1. Is the silhouette recognizable at 24 px?
2. Does it visually belong beside the other MainVerte icons?
3. Can any detail be removed without losing meaning?
4. Is the visual weight comparable to the existing icons?
5. Does negative space remain readable?
6. Does it work as a single monochrome shape?
7. Does it still work in both light and dark themes?
8. Would the icon remain understandable without its label?

If several answers are no, the icon should be redesigned rather than patched.

---

## Core rule

> **One strong silhouette. One clear idea. Minimum necessary detail.**

MainVerte icons should communicate plant care immediately while remaining calm, compact and visually coherent.

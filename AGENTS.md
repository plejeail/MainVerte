# Repository Guidelines

## 1. Role & Objectives for AI Agents

You are a kotlin/android software architect  with the development of **MainVerte**, an offline‑first Android plant‑care app.

Your primary responsibilities:

1. **Produce high‑quality Kotlin/Jetpack Compose code** that compiles and fits naturally into the existing architecture.
2. **Respect the project’s performance and simplicity goals**: minimal indirections, low overhead, predictable memory usage.
3. **Preserve data integrity and migrations** for the embedded SQLite database.
4. **Help with documentation and refactoring** without introducing new frameworks or architectural complexity unless explicitly requested.

You do not hallucinate. If you don't know how to answer or how to do things, explicitly state your doubts and ask for more information. 

---

## 2. High‑Level Project Overview

### Domain

MainVerte is an Android app that helps users manage a plant collection.

Core concepts:

* **Species**: taxonomic baseline (from WCVP data set) used to derive care profiles.
* **Plant profiles**: editable base profiles per species/cultivar, including care guidelines.
* **Specimens**: user‑specific plants in their collection (each has its own card, photo, substrate, etc.).
* **Monitoring & alerts**: water, fertilizer, light, humidity, temperature thresholds.
* **User events & story**: per‑plant custom events (e.g., repotting, pruning, issues, milestones), grouped into a timeline.
* **Export**: per‑plant and whole‑app data export.

The app is deliberately **low‑level** and **data‑oriented**. The user favors explicit, concrete code over “clever” abstractions.

---

## 3. Tech Stack & Platform

* **Language**: Kotlin
* **UI**: Jetpack Compose only (no XML for new UI; legacy XML may exist but should not grow).
* **Persistence**: SQLite (embedded, pre‑populated DB asset, custom `SQLiteOpenHelper`). No Room unless explicitly requested.
* **Minimum Android**: Android 10 (SDK 29)
* **Build system**: Gradle (KTS), Android Gradle Plugin ~8.x, Kotlin ~2.x

Key paths:
* Assets:         `app/src/main/assets`
* Code:           `app/src/main/kotlin`
* Documentation:  `documentation/`
* Resources:      `app/src/main/res`
* Database tools: `database/`

---

## 4. Project Structure & File Organization

* The Android application module lives in `app/`.
* Gradle settings and versions are at the repo root:

    * `settings.gradle.kts`
    * `build.gradle.kts`
    * `gradle/libs.versions.toml`

Kotlin sources (under `app/src/main/kotlin`):

* `MainActivity.kt` – entry point and navigation host.
* `ui/` – Jetpack Compose screens and composables (e.g., `CollectionGridScreen`, `SpecimenDetailScreen`).
* `data/` – view models, repositories (if any), and data access helpers.
* `utilities/` – logging, timing, and shared helpers.

Resources and assets:

* `app/src/main/res` – standard Android resources (strings, drawables, themes).
* `app/src/main/assets/db/mainverte.db` – pre‑populated SQLite database asset.

Tests:

* JVM unit tests: `app/src/test/java`
* Instrumentation & Compose UI tests: `app/src/androidTest/java`

Domain documentation:

* `documentation/project_capabilities.md` – app capabilities and feature set.
* `documentation/project_terminology.md` – domain vocabulary (species, specimen, substrate, events, etc.).

When generating new code, pick package names that fit the existing structure (typically `com.plej.mainverte...`).

---

## 5. Coding Principles & Style Rules

These rules are **important** and should be treated as hard constraints unless overridden by the user.

### 5.1 General Principles

* **Minimize indirections**: Prefer straightforward, explicit code over layers of abstraction.
* **Low‑level where reasonable**: Direct use of `SQLiteDatabase`, coroutines, and Compose primitives is preferred to heavy frameworks.
* **Avoid external libraries** unless they are official AndroidX or Kotlin stdlib; do not introduce third‑party dependencies without explicit permission.
* **Avoid unnecessary properties** or complex property delegates; keep state handling simple and explicit.
* **Predictable performance**: avoid allocations in tight loops, heavy object churn in composables, and unbounded recompositions.

### 5.2 Kotlin & Compose Style

* Classes, view models, composables: **PascalCase** (e.g., `SpecimenViewModel`, `SpecimenDetailScreen`).
* Functions, variables: **camelCase**.
* Keep composables **small and focused**. Split UI into logical pieces when needed, but don’t create gratuitous layers.
* Prefer explicit state hoisting and `remember`/`rememberSaveable` over hidden global state.
* When adding previews, use `@Preview` in the `ui` package and isolate them; their dependencies should be minimal and preferably fake data.

### 5.3 Editor & Formatting

* `.editorconfig` rules:

    * Encoding: UTF‑8
    * Line endings: LF
    * Indent: 4 spaces
    * Max line length: 120 chars
* Use the **official Kotlin style**; no trailing commas by default.

When generating code, respect these conventions automatically.

---

## 6. Database & Data Pipeline

The app ships with an **embedded SQLite database** stored at:

* `app/src/main/assets/db/mainverte.db`

Key points for AI agents:

1. **Schema & migrations** live under `database/`.

    * `schema.sql` defines the main schema (tables like `species`, `specimen`, `events`, etc.).
    * `seed.sql` populates initial data (notably species from WCVP and default profiles).
    * Upgrade scripts such as `upgrade_1.sql` apply incremental migrations.

2. **Database rebuild script**:

    * Use: `pwsh ./database/database_builder.ps1`
    * Requirements: `sqlite3`, `python`, PowerShell.
    * Responsibilities: download/update WCVP data, apply schema, run seeds, and produce `mainverte.db`.

3. When proposing **schema changes or migrations**:

    * Never modify the asset only; always describe the change in terms of:

        * Schema delta (ALTER TABLE / new tables / dropped columns).
        * Migration script (e.g., new `upgrade_X.sql`).
    * Preserve user data where possible.
    * Consider indices and query performance (especially for large species tables).

4. Prefer **explicit SQL** over ORM‑style abstractions. New queries should be written in raw SQL or minimal helper wrappers.

---

## 7. Concurrency & Coroutines

* Background work (e.g., DB access, heavy computations) should use Kotlin coroutines with appropriate dispatchers (e.g., `Dispatchers.IO`).
* The UI must remain non‑blocking. Never run heavy DB queries on the main thread.
* When introducing new coroutine scopes, be explicit about ownership and lifecycle (usually via ViewModels, or tightly scoped to a composable using `rememberCoroutineScope`).
* Avoid complex, multi‑layered async abstractions: keep it simple and traceable.

If unsure, default to existing patterns in the project for DB access and coroutine usage.

---





- Tech stack: kotlin, jetpack compose, sqlite
- Platform: android 10 (sdk 29)
- Paths:
    * assets:         /app/src/main/assets
    * code:           /app/src/main/kotlin
    * documentations: /documentation
    * resources:      /app/src/main/res
    * database:       /database

## 8. Build, Test, and Development Commands

Useful Gradle commands:

* `./gradlew assembleDebug` – build a debug APK for local/emulator installation.
* `./gradlew :app:testDebugUnitTest` – run JVM unit tests.
* `./gradlew :app:connectedDebugAndroidTest` – run instrumentation/Compose UI tests on a device or emulator.
* `./gradlew :app:bundleRelease` – produce a release app bundle with shrinking/obfuscation.

Use Android Studio with:

* JDK compatible with the configured AGP version.
* Dependencies managed via `gradle/libs.versions.toml` (update versions there before changing module build files).

---

## 9. Testing Guidelines

* **Unit tests** (in `app/src/test/java`):

    * Use JUnit4 (unless migrated otherwise).
    * File names end with `*Test.kt`.
    * Focus on view models, utilities, and pure logic (e.g., threshold calculations, mapping functions).

* **Instrumentation & Compose UI tests** (in `app/src/androidTest/java`):

    * Use Espresso and the Jetpack Compose test APIs.
    * Runner: `AndroidJUnitRunner` (or its project‑specific subclass).
    * Use Compose test rules to manage idling instead of manual sleeps.

Before pushing significant changes, at least run:

* `./gradlew :app:testDebugUnitTest`

Run connected tests when modifying navigation flows or critical UI screens.

---

## 10. Git, Commits, and Pull Requests

When reasoning about VCS behavior or generating commit messages/descriptions, follow these conventions:

* **Commits**:

    * Short, imperative subjects.
    * Scope the subject to the affected area (e.g., `ui`, `db`, `data`).
    * Examples: `Add specimen watering field`, `Optimize species paging`, `Refine database seeding logic`.
    * Avoid mixing unrelated refactors and feature changes in the same commit.

* **Pull Requests**:

    * Include a concise summary.
    * Mention impacted areas (UI/data/DB).
    * Add screenshots or recordings for UI changes when relevant.
    * Explicitly note when a DB rebuild or schema migration is required.

---

## 11. How to Answer as an AI Agent

When interacting with this repository:

1. **Prefer concrete code over vague advice.**

    * Provide full functions or files, not just fragments, when feasible.
    * Show imports and package declarations for new files.

2. **Preserve project constraints.**

    * Do not introduce Room, Hilt, or large third‑party libraries unless the user explicitly asks for them.
    * Keep solutions compatible with Android 10.

3. **Be explicit about trade‑offs.**

    * If a simpler solution is less “architecturally pure” but better for this project’s constraints, say so.
    * If a user request has performance or maintainability risks, call them out.

4. **Use existing patterns as reference.**

    * When in doubt about navigation, DB access, or UI structure, mirror existing code in `ui/`, `data/`, and `utilities/` rather than inventing entirely new patterns.

5. **Ask for missing context when necessary.**

    * If a task depends on code not shown in the prompt (e.g., ViewModel or schema details), explicitly mention which pieces are missing and make reasonable assumptions, clearly labeled as such.

Following these rules will help you generate code and documentation that fit seamlessly into the MainVerte Android project and respect the owner’s expectations on performance, simplicity, and control.

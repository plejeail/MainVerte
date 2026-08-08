# MainVerte — Roadmap

## V0.1 — Minimal Caring

### Watering

* Display watering information in specimen detail
* Add **"Watered today"** action
* Display last watering date
* Allow the user to define the next watering reminder
* Allow the reminder to be edited, postponed, or disabled

### Notifications

* Schedule system notifications for watering reminders
* Handle notification cancellation/rescheduling when a reminder changes
* Provide internal feedback for immediate actions

    * watering recorded
    * reminder updated
    * operation failed

* Replace Userfeedback with notifications on the android side (Core keep them as conceptually different things)

### Architecture

* Introduce a generic care/reminder model even if watering is initially the only supported care type
* Keep care state specimen-specific
* Do not derive watering needs automatically from species data yet

---

## V0.2 — Care History / Journal

### Journal

* Provide one chronological journal view per specimen
* Store all horticultural events as timestamped events
* Display event type, date/time, and optional details
* Allow manual journal entries / notes
* Allow events to be edited or deleted where appropriate
* Store watering events persistently

### Initial event types

* Watering
* Note
* Specimen created
* Specimen edited

### Data portability

* Export a specimen journal
* Import a specimen journal
* Define a versioned and documented journal exchange format
* Validate imported data before modifying the local database

### Architecture

* Treat the journal as a view over specimen events rather than requiring a dedicated `Journal` domain entity
* Keep event storage extensible for future care types:

    * fertilizing
    * repotting
    * pruning
    * treatment
    * measurement
    * photo

---

## V0.3 — Species

### Species browsing

* Add Species list fragment
* Add Species detail fragment
* Add species search
* Support filtering/search by botanical and common names

### Specimen integration

* Make species editable from specimen detail
* Add species picker/search from specimen detail
* Allow a specimen to have no identified species
* Allow changing the associated species without affecting specimen history

### Species data

* Botanical name
* Common name(s)
* Genus
* Family
* Description
* Basic horticultural information
* Optional species photo

### Architecture

* Clearly separate:

    * `Species`: shared botanical/horticultural knowledge
    * `Specimen`: state of an individual plant
    * `Care`: actions and state changing over time
* Species data may later provide care recommendations but must not directly own specimen-specific state such as watering dates

---

## V0.4 — Settings

### Appearance

* Add Settings screen
* Theme selection:

    * System default
    * Light
    * Dark
    * Version (App [MainVerte.Android], Software [MainVerte.Core], Database [Sqlite])
* Persist the selected theme
* Apply theme changes consistently across the application

Keep this release deliberately small.

---

## V0.5 — Gardener & Collections

### Gardener

* Add gardener profile

    * name
    * photo
    * description
* Require at least one gardener profile
* Show gardener creation flow when none exists
* Allow gardener profile editing

### Collections

* Create collections owned by a gardener
* Require at least one collection
* Create a default collection when appropriate
* Switch active collection
* Create collections
* Edit collections
* Delete collections
* Move specimens between collections
* Define behavior when deleting a non-empty collection

### Architecture

* Domain hierarchy:

`Gardener -> Collection -> Specimen`

* Give collections stable identifiers
* Make specimen ownership explicit through its collection
* Keep the model compatible with multiple gardeners even if the initial UI primarily targets one gardener

---

# Later

## Care expansion

* Fertilizing
* Repotting
* Pruning
* Treatments
* Custom care events
* Recurring care reminders

## Measurements

* Substrate moisture
* Pot weight
* Temperature
* Relative humidity
* PPFD / light exposure
* Plant dimensions
* Historical charts

## Photos

* Add photos to journal events
* Specimen photo timeline
* Compare plant evolution over time

## Smart Caring

* Species-based care recommendations
* Specimen-specific recommendations
* Environmental context
* Seasonal adaptation
* Care history analysis
* Detection of unusual trends
* Reminder suggestions rather than fixed calendar-only schedules

## Data management

* Full collection export/import
* Backup/restore
* Database migrations
* Data format versioning

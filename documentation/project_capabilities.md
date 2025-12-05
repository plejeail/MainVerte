# MainVerte capabilities

This document defines the problems this app aims to solve and how.

## Application capabilities - global use cases

### Manage collection

Problem: manage the collection of the gardener

This capability covers adding, editing, organizing and removing specimens (plants) in a gardener's collection.

- System behaviour
    - The system persists specimens (a.k.a. specimens, plants, or records) with a stable identifier and structured metadata: canonical species name, display name, location, pot/container, acquisition date, source, size/age estimates, substrate, tags, and optional photos.
    - Creating a specimen automatically creates a system Journal for that specimen to aggregate subsequent system and user Events. The system Journal is flagged as system-managed and cannot be deleted by gardeners.
    - The system supports imports (CSV, JSON, common plant app formats) and will attempt best-effort mapping to specimen fields. Imports must be idempotent when re-applied and provide a manual merge/resolve UI for duplicates.
    - All specimen mutations (create/update/delete/merge) are recorded as timestamped Events in the specimen's Journal so history is preserved.

- Gardener behaviour
    - Gardeners can add new specimens by entering required fields and optionally attaching photos. Minimal required fields should be species (or placeholder), display name, and location.
    - Gardeners can edit specimen metadata (rename, change species, move between locations, update pot/size, add notes/tags, manage photos) and see a revision timestamp and author for each change.
    - Gardeners can delete specimens; deletions must require explicit confirmation and/or offer an archive/soft-delete alternative that preserves the Journal and photos for potential restore. If a hard-delete is allowed, warn about irreversible loss and remove any gardener-owned media.
    - Gardeners can merge duplicate specimens; merging must present a clear field-by-field preview and preserve the union of photos and Journals (merging Events where appropriate) and log the merge as an Event.
    - Gardeners can bulk-edit selected specimens (change location, add/remove tags, bulk-assign care rules) and bulk-import collections from supported formats.

- UI expectations
    - Provide a primary "Collection" or "My Plants" screen listing specimens with thumbnail, display name, species, next scheduled Task (if any), and location. Support sorting and quick filters (by location, species, tags, overdue tasks, favorites).
    - Implement a clear, form-based "Add specimen" flow with inline validation and photo attachments. Offer common quick-presets (e.g., recently added species, common locations) and an optional guided import wizard for bulk uploads.
    - The specimen detail view should expose: main photo carousel, key metadata (species, location, pot), care summary (next tasks, last watered), link to Journals/Events, edit and delete actions, and share/export actions.
    - Provide undo for recent deletes/edits where feasible and a restore flow for archived specimens.

- Data & API notes
    - Core schema suggestions (SQLite):
        - specimens (id, species_text, display_name, location_id, pot_description, acquisition_date INTEGER, source TEXT, size_estimate TEXT, is_archived INTEGER, created_by, created_at INTEGER, updated_at INTEGER)
        - specimen_photos (id, specimen_id, file_uri, order_index, created_at INTEGER)
        - specimen_tags (specimen_id, tag)
        - specimen_journals (id, specimen_id, title, is_system INTEGER, created_by, created_at INTEGER)
    - Store timestamps as UTC epoch seconds. Use prepared statements and explicit parameter binding for all queries and mutations.
    - All create/update/delete operations should emit a Journal Event recorded in `journal_events` describing the mutation (type = 'specimen_created' | 'specimen_updated' | 'specimen_deleted' | 'specimen_merged').

- Edge cases & behaviours to define
    - Duplicate detection: define heuristics for import and quick-merge suggestions (name/species/location/photo similarity) and make merging a manual, safe operation.
    - Media handling: large image uploads should be downscaled on-device; store URIs and keep file lifecycle policies (when deleting a specimen, decide whether to remove media files or leave them for archival recovery).
    - Offline first: allow specimen creation and edits offline and reconcile conflicts on sync; prefer last-writer-wins with audit Events and provide a conflict-resolution UI for critical fields.
    - Deletion policy: prefer soft-delete/archive by default to avoid accidental data loss; provide an admin/hard-delete for power users with clear warnings.

- Acceptance criteria
    - A gardener can add a new specimen with required fields and photos; the specimen appears in the Collection list and its system Journal is created.
    - A gardener can edit specimen metadata and photos; changes are persisted and recorded in the specimen Journal with timestamps and author info.
    - A gardener can archive or delete a specimen; archive hides the specimen from default lists but preserves its Journal and media and supports restore.
    - The app supports bulk import of specimens with a preview and duplicate-resolution step; imported specimens appear in the collection with correct mappings.


### Organize day-to-day collection care events

Problem: remember to perform tasks at the right time when there is a lot of plants

The app should plan and manage care Tasks for gardeners based on their collection, species profiles, user preferences and additional data (sensors, weather, import sources). Tasks are scheduled actions the gardener should perform (water, fertilize, repot, prune, inspect, etc.).

- System behaviour
    - The system generates recommended Tasks from species profiles, plant size/age, substrate, recent Events, and configured reminders. Recommendations can be one-off or recurring.
    - System Tasks are flagged as system-generated. Creating, rescheduling or completing system Tasks must create a corresponding Event (record) in the specimen's system Journal so history is preserved.
    - The system supports recurring Tasks with explicit recurrence rules (daily/weekly/monthly/custom) and will materialize future Task instances as needed (either on-the-fly or via a background scheduler).

- Gardener behaviour
    - Gardeners can create, edit, reschedule, snooze, cancel and delete their own Tasks. They can mark a Task as done and optionally attach notes or photos when completing it.
    - Gardeners can create one-off or recurring Tasks. For recurring Tasks, each occurrence should be tracked (instance) so completion history is preserved.
    - When a gardener marks a Task done, the app records a Task completion Event (timestamped, with optional photo/note) and, if the Task is recurring, computes the next occurrence according to its rule.
    - Gardeners can restore canceled tasks.

- UI expectations
    - Provide a dedicated "Care" screen listing upcoming and overdue Tasks, with quick actions: Mark done, Snooze, Reschedule, Edit, Delete.
    - Allow per-specimen task lists on the specimen view and a global calendar/agenda view across the collection.
    - Show Task origin (System / Gardener), recurrence info, next due date, and history link that opens the Event in the specimen Journal.
    - Support bulk actions (mark selected done, postpone selected) and filters (by species, location, due date, overdue only, origin).

- Data & API notes
    - Core concepts: Task (definition), TaskInstance (scheduled occurrence), TaskCompletionEvent (an Event linked to completion), and TaskRule (recurrence specification).
    - Minimal schema suggestions (SQLite):
        - tasks (id, specimen_id, title, description, origin TEXT, is_system INTEGER, rule TEXT NULL, created_by, created_at, updated_at)
        - task_instances (id, task_id, due_at INTEGER, status TEXT, scheduled_at INTEGER, created_at INTEGER)
        - task_completion_events (id, task_instance_id, event_id) — links a completed instance to an Event stored in `journal_events`.
    - When a Task is completed, create a `journal_events` row (type = 'task_completed') and link it via `task_completion_events` so the specimen Journal contains a canonical record.
    - Use raw SQL migrations with `PRAGMA foreign_keys = ON;` and `BEGIN IMMEDIATE; ... COMMIT;` per AGENTS.md.

- Edge cases & behaviours to define
    - Timezones: store timestamps as UTC epoch seconds and present localized dates in UI.
    - Missed and overdue tasks: define how the scheduler materializes past instances (alert vs auto-reschedule).
    - Concurrent modifications: if a Task is updated while a background scheduler is materializing instances, ensure operations are transactional and idempotent.
    - Recurrence rule complexity: prefer simple, explicit rule representations (RFC 5545 subset or a compact JSON) to avoid heavy parsing.

- Acceptance criteria
    - The system generates recommended Tasks for specimens with configured care rules and they appear in the Care screen.
    - A gardener can create, edit, delete, snooze and mark Tasks done; marking done creates a timestamped Event linked to the specimen's Journal.
    - Recurring Tasks produce distinct instances; completing an instance does not erase history and schedules the next occurrence per the rule.
    - The specimen view shows both system and gardener Tasks; Task completions are visible in the specimen Journal and correctly labeled with their origin.

### Keep journals

Problem: follow day after day the evolution of something. Be able to

The app must provide a journaling feature to record and follow the evolution of specimens (growth, flowering, disease, notes, photos, etc.). Journals are ordered collections of Events associated with a specimen.

- System behaviour
    - The system automatically creates one default (system) Journal per specimen. This system Journal aggregates the specimen's full history (care tasks, lifecycle changes, system-generated alerts, imports, etc.).
    - System Journals are flagged as system-managed and cannot be deleted by gardeners.

- Gardener behaviour
    - Gardeners can create, edit (title/description/visibility) and delete their own Journals.
    - Gardeners can add new Events to any Journal they own. An Event may include text, metadata (type), a timestamp, and an optional photo URI.
    - Gardeners can include Events that originate from the system by linking those Events into their Journal (Events may belong to multiple Journals).
    - Deleting a gardener Journal removes the association between that Journal and its Events; it does not delete Events that belong to the system Journal. Gardener-created Events are deleted with the Journal unless the Event was linked to another Journal.

- UI expectations
    - Specimen view shows the default system Journal (full history) and a list of gardener Journals for that specimen.
    - Provide clear affordances: "Create Journal", "Add entry", "Add photo", "Link system entry" and controls to edit/delete gardener Journals.
    - Mark entries with their source (System / Gardener) and support chronological sorting and an explicit sequence order.

- Data & API notes
    - A Journal is an ordered collection of events. Events are time-stamped records that can be linked to one or more Journals.
    - Persist Journals and Event-to-Journal mappings in the database. System Journals should be protected (is_system flag).
    - All events are associated to at least one journal.

- Acceptance criteria
    - When a specimen is created/imported, a system Journal is created and contains subsequent system events.
    - A gardener can create, rename, and delete their own Journals; deletion of a gardener Journal does not remove the specimen's system Journal.
    - Gardeners can add Events (with optional photo) to their Journals and can link existing system Events into their Journals.
    - The specimen view shows the system Journal by default and lists gardener Journals, with events correctly ordered and source-labelled.

### Search knowledge base
The app should surface a curated knowledge base (species wiki and care guides) and provide a fast search experience so gardeners can find authoritative care information for known species and general plant care topics.

- System behaviour
    - The system maintains a collection of knowledge base articles (species pages, care how-tos, troubleshooting guides). Articles may be curated, imported from trusted sources, or contributed by the project team.
    - The system builds and maintains a full-text index (preferably SQLite FTS5) over article titles, summaries, bodies, and tags to support quick searches and relevance ranking.
    - The system associates articles to species profiles when applicable (article ⇄ species mapping) and keeps metadata (source, license, last_updated, version).
    - Support offline caching of favorites or recently-viewed articles for mobile scenarios.

- Gardener behaviour
    - Gardeners can search the KB with free-text queries, filter by category (species, diagnostics, care, pests), and open article pages with structured care information (watering, light, substrate, notes, images).
    - Gardeners can bookmark/save articles for offline use, add local notes to an article (not modifying canonical content), and report issues or suggest edits back to the project team.
    - From a specimen page, gardeners can use "Find care info" to surface species-specific articles and quick-care cards.

- UI expectations
    - Provide a prominent search box with autosuggest and fuzzy matching; show top results grouped by type (Species pages, Guides, Alerts/Troubleshooting).
    - Article pages must show: title, canonical species name(s), summary, structured care fields (water/light/fertilizer), images, author/source, last updated, and related articles.
    - Provide offline indicator, bookmark action, share/export, and a lightweight printable view for field use.

- Edge cases & behaviours to define
    - Multi-language support: articles may exist in multiple languages; search should prioritize user's preferred language but fallback gracefully.
    - Conflicting sources or deprecated guidance: surface source/license and last_updated prominently; mark deprecated or community-submitted content clearly.
    - Large media and offline storage: limit offline cache size and provide settings to control media download.
    - Licensing: ensure imported content respects source licenses and attribution is preserved.

- Acceptance criteria
    - Free-text searches return relevant results within a reasonable latency for mobile devices; species pages appear for species-specific queries.
    - Article pages present structured care guidance (watering/light/fertilizer) for known species and allow bookmarking/offline saving.
    - From a specimen, "Find care info" surfaces at least one relevant article when the species is known and displays it in the article viewer.
    - Full-text index updates when new/updated articles are added and bookmarks/offline copies are retrievable when offline.

## UX workflow

Below are step-by-step UX workflows for the most important gardener goals. Each workflow lists the user's intent, a typical UI path, and the expected outcome.

- Gardeners want to add a new specimen to their collection
    * UI path: Collection screen → + / Add specimen → fill required fields (species, display name, location) → attach photos (optional) → Save
    * Outcome: specimen appears in Collection list, system Journal is created, creation Event recorded.

- Gardeners want to move a specimen to their cemetery (archive/retire)
    * UI path: Specimen detail → Actions → Archive / Move to cemetery → confirm
    * UI path: Cemetery → add specimen → select specimen from list → confirm
    * Outcome: specimen is flagged archived/retired and hidden from default lists; Journal and media preserved and available from Cemetery/Archived view.

- Gardeners want to remove a specimen from their collection (soft-delete)
    * UI path: Specimen detail → Actions → Archive → confirm (explicit warning for permanent delete)
    * Outcome: soft-delete moves to Archived/Cemetery.

- Gardeners want to restore a deleted specimen
    * UI path: Settings → Archived → select specimen → Restore
    * Outcome: specimen is moved back into active Collection and a restore Event is recorded.

- Gardeners want to edit a specimen in their collection
    * UI path: Specimen detail → Edit → change fields → Save
    * Outcome: changes persist, update timestamp and author recorded, Journal Event created describing the mutation.

- Gardeners want to view/consult a specimen
    * UI path: Collection → tap specimen → Specimen detail (photo carousel, key metadata, care summary, Journals link, quick actions)
    * Outcome: gardener can read metadata, view recent Events and Tasks, and open edit/export actions.

- Gardeners want to get basic knowledge about a species
    * UI path: Species screen → optional → filter by tag/name → species detail
    * Outcome: species care card or relevant article is shown; user may bookmark or save offline.

- Gardeners want to view the list of all tasks that need to be done
    * UI path: Main nav → Care / Tasks → Agenda or Calendar view → filter/sort (overdue, by location, by species)
    * Outcome: upcoming and overdue tasks shown with quick actions (Mark done, Snooze, Reschedule).

- Gardeners want to create a task (one-off or recurring)
    * UI path: Care / Tasks → + Add task → select specimen (optional) → title, schedule, recurrence → Save
    * Outcome: Task appears in Care list and a Task creation Event is recorded in the specimen Journal.

- Gardeners want to mark a task done, cancel, snooze, or report a task
    * UI path: Care list or Specimen → Task → Mark done / Snooze / Cancel / Report → optionally attach note/photo → confirm
    * Outcome: completion Event recorded in Journal (linked via task_completion_events), next recurrence scheduled for recurring tasks.

- Gardeners want to create a journal
    * UI path: Specimen detail → Journals → + New Journal → title / visibility → Save
    * Outcome: new gardener Journal is created and listed alongside the system Journal.

- Gardeners want to add, edit, or delete a journal entry (custom Event)
    * UI path (add): Journal → + Add entry → text / photos / timestamp → Save
    * UI path (edit): Journal → select entry → Edit → Save
    * UI path (delete): Journal → select entry → Delete → confirm
    * Outcome: Events are created/updated/deleted; deletions of gardener Journals remove only gardener-created associations (system Events remain intact).

- Gardeners want to view/consult a journal
    * UI path: Specimen detail → Journals → open Journal → scroll chronological list → open entry
    * Outcome: Events shown with source labels (System / Gardener) and media inline.

- Gardeners want to export a specimen profile or a journal
    * UI path: Specimen detail or Journal → Export / Share → choose format (PDF, JSON, CSV) → share via OS share sheet or save
    * Outcome: exported file generated with selected content and metadata.

- Gardeners want to import specimens in bulk (preview + duplicate resolution)
    * UI path: Collection → Import → choose file (CSV/JSON) → mapping preview → duplicate detection → resolve duplicates (merge/skip/create) → Import
    * Outcome: specimens imported idempotently; merge operations create Events and preserve Journals/media.

- Gardeners want to export specimens in bulk (CSV/JSON)
    * UI path: Collection → Select specimens or All → Export → choose format → Confirm → download/share
    * Outcome: export file created with selected specimens and metadata.

- Gardeners want to merge duplicate specimens with a field-by-field preview
    * UI path: Collection → select suspected duplicates or Import preview → Merge → review side-by-side fields and photos → confirm merge
    * Outcome: unified specimen created, Journals merged (Events preserved or merged sensibly), a merge Event recorded.

- Gardeners want to bulk-edit selected specimens (location, tags, care rules)
    * UI path: Collection → multi-select → Bulk actions → Edit fields (location, tags, rules) → Apply
    * Outcome: selected specimens updated transactionally; Events recorded per specimen or as a bulk Event summary.

- Gardeners want to manage tags and locations
    * UI path (tags): Settings / Tags or Collection → Tag manager → Create / Rename / Merge / Delete → apply to specimens
    * UI path (locations): Settings / Locations → Create / Rename / Reparent / Delete → reassign specimens when needed
    * Outcome: tags and locations curated; UI filters reflect changes and specimens are reassigned as requested.

- Gardeners want to share a specimen or journal (export/share)
    * UI path: Specimen or Journal → Share / Export → choose format and destination (share sheet, export file) → send
    * Outcome: recipient receives exported content; share action is recorded in activity log if desired.

- Gardeners want an onboarding flow for first-time setup and presets
    * UI path: First launch / Settings → Onboarding → create first collection, choose units, import sample data or presets → finish
    * Outcome: guided setup reduces friction; sample specimens or templates added optionally.

- Gardeners want to manage app settings (units, timezone, language, notifications, alert toggles)
    * UI path: Settings → Preferences → adjust units, timezone, language, notification preferences → Save
    * Outcome: app behaviour and displays update immediately or after restart as appropriate.

- Gardeners want to receive and manage notifications and reminders
    * UI path: Settings → Notifications → configure reminders and quiet hours; Care list → tap notification → open task
    * Outcome: scheduled reminders fire per user preference; notification actions map to quick task operations.

- Gardeners want to resolve sync conflicts with a UI
    * UI path: Settings / Sync → Conflicts → list conflicts → open conflict detail → view side-by-side values, provenance and timestamps → choose resolution (keep local, keep remote, merge fields, create duplicate) → apply
    * Outcome: conflict resolution is recorded as an Event; chosen resolution is propagated to sync targets and can be audited.

- Gardeners want to sync and back up data and restore backups
    * UI path: Settings → Sync & Backup → Manual Sync / Auto Sync toggle / Create backup / Restore backup → confirm
    * Outcome: backups are created and restorations rehydrate data while recording restore Events; sync status shown in UI.

- Gardeners want accessibility and localization support
    * UI path: Settings → Accessibility / Language → enable large text, high contrast, change language → apply
    * Outcome: UI adapts to accessibility settings and language; content fallbacks used where translations are missing.

- Gardeners want to reset modified system alerts
    * UI path: Settings → Alerts → System alerts → Reset to defaults or re-enable → confirm
    * Outcome: system alerts restored to default thresholds and behaviors; changes recorded in audit log.

- Gardeners want to get information about the app and the developers
    * UI path: Settings → About → view version, licenses, contact / support links
    * Outcome: user can contact devs or read legal/licensing info.

## Later evolutions
- Gardener wants to get a report on his strengths and weaknesses + advices to improve
- Gardeners wants to identify a species
- Gardeners want to sync and back up their data and restore backups a server
- Gardeners want to manage custom locations
- Gardeners want to merge duplicate specimens with a field-by-field preview
- Gardeners want to bulk-edit selected specimens (location, tags, rules)
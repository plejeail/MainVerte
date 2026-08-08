# MainVerte capabilities

This document defines the problems this app aims to solve and how.
It is co-canonical with `Docs/domain_definitions.md`.

## Terminology alignment

This document uses canonical terms from `Docs/domain_definitions.md`: `Collection`, `CollectionState`, `Specimen`, `Species`, `Journal`, `Event`, `Task`, `CareRule`, `TaskInstance`, `TaskCompletion`, `Alert`, `Notification`, `KnowledgeArticle`, `Advice`, `Source`, `ImportBatch`, `ExportArtifact`, `Device`, `DeviceGroup`, `DeviceReading`, and `DeviceCommand`.

## Application capabilities – global use cases

### 1. Manage the living collection

Manage specimens and collections as the central domain of the app.

Includes:

* create, edit, archive, restore, delete, and merge specimens
* manage specimen identity, species, location, container, substrate, tags, photos, acquisition data
* support import/export
* preserve provenance through domain events

Core entities:

* Collection
* Specimen
* SpecimenPhoto
* SpecimenTag
* ImportBatch
* ExportArtifact

MVP bootstrap:

* database initialization creates one default gardener and one associated default collection
* a post-MVP registration workflow will create a gardener and collection whenever none exists, and will enforce that every gardener retains at least one collection

### 2. Record horticultural history

Maintain a complete, auditable history of each specimen.

Includes:

* system journal automatically created for every specimen
* gardener-owned journals
* chronological event timeline
* care actions, mutations, measurements, observations, device actions, diagnostics, imports and exports recorded as events when relevant

Core rule:

* each Event belongs to exactly one Journal
* deleting a Journal deletes its Events
* the gardener cannot delete system Journals

Core entities:

* Journal
* Event

### 3. Plan and execute care

Help the gardener perform the right action at the right time.

Includes:

* manual tasks
* recommended tasks
* recurring task rules
* task instances
* completion history
* snooze, reschedule, cancel, restore
* global and per-specimen care agenda

Core entities:

* Task
* CareRule
* TaskInstance
* TaskCompletionEvent

### 4. Observe and measure specimens

Capture objective and subjective specimen state independently of care actions.

Includes:

* manual measurements
* imported measurements
* computed measurements
* visual observations
* notes, photos, symptoms, phenological stages
* historical trends and comparisons

Measurement examples:

* PPFD
* DLI
* temperature
* relative humidity
* VPD
* pot weight
* water volume
* EC
* pH
* height
* leaf count
* flower count
* fruit count

Core entities:

* Measurement
* Observation

Design rule:

* Measurements are quantitative time-series records.
* Observations are descriptive specimen-state records.
* Both may emit Events, Alerts, Advice, or Task recommendations, but they are not themselves generic Events.

### 5. Monitor devices and environments

Represent sensors and actuators without confusing them with specimens.

Includes:

* device groups
* sensors
* actuators
* readings
* commands
* linked plant contexts
* stale/offline device state
* trend views
* actuator audit trail

Core rules:

* a sensor belongs to exactly one DeviceGroup
* a Device is not linked directly to a Specimen
* a DeviceGroup may be linked to one or more Specimens
* specimen deletion does not implicitly delete devices or readings

Core entities:

* DeviceGroup
* Device
* DeviceReading
* DeviceCommand
* SpecimenDeviceGroupLink

### 6. Detect alerts and notify the gardener

Convert important conditions into visible, actionable signals.

Includes:

* overdue care
* abnormal measurements
* sensor stale/offline state
* health risks
* pest/disease suspicion
* environmental threshold breaches
* notification creation
* task recommendations derived from alerts

Core entities:

* Alert
* AlertRule
* Notification

Design rule:

* Alerts explain a condition.
* Notifications deliver that condition to the gardener.
* Tasks propose or require an action.

### 7. Search horticultural knowledge

Provide reliable species and care knowledge.

Includes:

* species profiles
* curated articles
* source metadata
* license metadata
* bookmarks
* offline cache
* contextual advice in specimen views

Core entities:

* Species
* KnowledgeArticle
* Source
* Bookmark
* Advice

Design rule:

* Knowledge must remain source-aware.
* Conflicting or deprecated guidance must be visible, not silently merged.

### 8. Diagnose and predict specimen evolution

Use specimen history, measurements, observations, device readings, care events, and species knowledge to help decide what happens next.

Includes:

* diagnostic hypotheses
* supporting evidence
* confidence levels
* gardener feedback
* risk estimation
* next watering estimate
* flowering probability
* fruit maturation estimate
* growth projection

Core entities:

* DiagnosticReview
* DiagnosticHypothesis
* Prediction
* Evidence

Design rule:

* The app must not produce unexplained advice.
* Every diagnosis or prediction should expose its evidence.

## UX workflow

### Daily Garden Review

Goal:
Determine what requires attention today.

Workflow:

* Open Dashboard
* Review collection overview
* Review active alerts
* Review upcoming care actions
* Review recommendations
* Review environmental anomalies
* Decide priorities

Outcome:

* Daily gardening plan established.

---

### Review the garden

Goal:
Understand the current state of the entire collection.

Workflow:

* Open Dashboard
* Review collection health
* Review specimens requiring attention
* Review environmental conditions
* Review ongoing recovery cases
* Open a specimen if deeper investigation is required

Outcome:

* Gardener understands collection-wide priorities.

---

### Understand a specimen

Goal:
Understand the current state of a specimen.

Workflow:

* Open Collection
* Select specimen
* Review specimen summary

  * health status
  * active alerts
  * latest observations
  * latest measurements
  * upcoming care actions
  * recommendations
* Drill down into details if necessary

Outcome:

* Gardener understands the current situation.

---

### Onboard a new specimen

Goal:
Introduce a newly acquired specimen into the collection.

Workflow:

* Add specimen
* Take photos
* Select species
* Record acquisition information
* Record current substrate
* Record current container
* Record initial condition
* Record baseline measurements

Outcome:

* Specimen baseline established.

---

### Assess a newly acquired specimen

Goal:
Evaluate the condition of a newly acquired specimen.

Workflow:

* Open specimen
* Record arrival observations
* Record damage
* Record substrate condition
* Record moisture state
* Review risks
* Decide:

  * keep as-is
  * quarantine
  * repot
  * treat

Outcome:

* Arrival assessment completed.

---

### Record new information

Goal:
Capture information about a specimen.

Workflow:

* Open specimen
* Add record
* Select record type

  * Observation
  * Measurement
  * Care Action
  * Photo
* Complete required fields
* Save

Outcome:

* Information becomes part of specimen history.

---

### Perform care

Goal:
Execute a care action.

Workflow:

* Open Dashboard, Care, Alert, or Specimen
* Select action
* Execute action
* Record result

  * optional notes
  * optional measurements
  * optional photos

Outcome:

* Care action is completed and recorded.

---

### Water a specimen

Goal:
Determine whether watering is necessary and record the result.

Workflow:

* Open specimen
* Review evidence

  * watering history
  * pot weight
  * substrate condition
  * measurements
  * recommendations
* Decide whether to water
* Record watering
* Record water volume

Outcome:

* Watering decision is documented.

---

### Repot a specimen

Goal:
Move a specimen to a new growing environment.

Workflow:

* Open specimen
* Start repotting operation
* Record old container
* Record new container
* Record old substrate
* Record new substrate
* Add photos if desired
* Save

Outcome:

* Repotting history is preserved.

---

### Recover a specimen

Goal:
Manage a declining or damaged specimen.

Workflow:

* Open specimen
* Mark as a recovery case
* Record symptoms
* Review evidence
* Create a recovery plan
* Track progress

Outcome:

* Recovery protocol is established and monitored.

---

### Propagate a specimen

Goal:
Create a new specimen from an existing specimen.

Workflow:

* Start propagation
* Select a parent specimen
* Select a propagation type

  * cutting
  * leaf
  * division
  * seed
  * offset
  * air layering
* Record propagation setup
* Track progress

Outcome:

* Propagation lineage is recorded.

---

### Plan future care

Goal:
Schedule recurring or future actions.

Workflow:

* Open Care
* Create or edit care plan
* Configure recurrence
* Configure reminders
* Save

Outcome:

* Future care actions are scheduled.

---

### Investigate an alert

Goal:
Understand why MainVerte is requesting attention.

Workflow:

* Open Alert
* Review triggering conditions
* Review supporting evidence
* Review impacted specimens
* Review recommended actions
* Dismiss or act

Outcome:

* Alert is understood and resolved.

---

### Verify a recommendation

Goal:
Decide whether a recommendation should be followed.

Workflow:

* Open recommendation
* Review evidence
* Review assumptions
* Review supporting history
* Accept
* Reject
* Postpone

Outcome:

* Recommendation disposition is recorded.

---

### Diagnose a problem

Goal:
Understand why a specimen is thriving or declining.

Workflow:

* Open specimen
* Start a diagnostic review
* Review hypotheses
* Review supporting evidence
* Review confidence levels
* Accept, reject, or investigate further

Outcome:

* Gardener obtains actionable explanations.

---

### Investigate a decline

Goal:
Perform a manual investigation into a specimen problem.

Workflow:

* Open specimen
* Review timeline
* Review measurements
* Review observations
* Review recent care actions
* Review environmental conditions
* Build hypotheses

Outcome:

* Investigation findings are documented.

---

### Follow specimen evolution

Goal:
Understand how a specimen evolved over time.

Workflow:

* Open specimen
* Open history
* Select timeframe
* Review

  * measurements
  * observations
  * photos
  * care actions
  * alerts
* Compare periods

Outcome:

* Gardener understands long-term evolution.

---

### Review a measurement

Goal:
Understand a specific measurement trend.

Workflow:

* Open measurement history
* Select metric
* Review graph
* Review trends
* Review anomalies
* Review related events

Outcome:

* Measurement interpretation is completed.

---

### Compare specimens

Goal:
Compare specimens across time or conditions.

Workflow:

* Select specimens
* Select metrics
* Compare measurements
* Compare observations
* Compare growth and health indicators

Outcome:

* Meaningful differences become visible.

---

### Review a species

Goal:
Understand a species and its requirements.

Workflow:

* Open species profile
* Review care requirements
* Review common issues
* Review environmental targets
* Review collection specimens of the species

Outcome:

* Species understanding is improved.

---

### Review collection trends

Goal:
Understand long-term collection performance.

Workflow:

* Open Dashboard
* Select timeframe
* Review:

  * watering frequency
  * growth
  * flowering
  * fruiting
  * mortality
  * alert frequency
  * recovery success rate

Outcome:

* Collection-level insights are obtained.

---

### Monitor an environment

Goal:
Understand environmental conditions affecting specimens.

Workflow:

* Open Sensors
* Select environment or device group
* Review current values
* Review trends
* Review active alerts
* Review linked specimens

Outcome:

* Gardener understands environmental conditions.

---

### Control an environment

Goal:
Act on connected equipment.

Workflow:

* Open Sensors
* Select actuator
* Choose action
* Confirm
* Review execution status

Outcome:

* Requested action is executed and recorded.

---

### Learn about a species

Goal:
Find reliable horticultural knowledge.

Workflow:

* Open Knowledge
* Search species or topic
* Review sources
* Bookmark content if useful

Outcome:

* Gardener acquires relevant knowledge.

---

### Manage the collection

Goal:
Maintain collection organization.

Workflow:

* Add specimen
* Edit specimen
* Archive specimen
* Restore specimen
* Merge duplicates
* Delete specimen

Outcome:

* Collection remains accurate and organized.

---

### Import or export data

Goal:
Move data into or out of MainVerte.

Workflow:

* Choose import or export
* Configure scope
* Review mapping or output
* Confirm

Outcome:

* Data is transferred successfully.

---

### Configure MainVerte

Goal:
Customize application behavior.

Workflow:

* Open Settings
* Configure:

  * notifications
  * backups
  * synchronization
  * units
  * locations
  * tags
* Save

Outcome:

* Application behavior matches gardener preferences.

## Co-canonical maintenance rule

`Docs/domain_concept.md` and `Docs/domain_definitions.md` are both normative.
Any domain evolution must update both files in the same pull request.
Divergence between these files is a specification error.

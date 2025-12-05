# MainVerte project - Definitions

**Advice**: A practical guidance or information on a specific topic.

**Alert**: A system-detected condition or rule-trigger related to a specimen or species (for example: high temperature, low moisture, disease pattern, missed watering). Alerts:
- generate events/tasks from a trigger (temporal triggers, sensor thresholds, etc),
- modify the severity level of existing tasks or events (for example escalate a task to warning or danger),
- emit Notifications to the gardener.

**Alert threshold**: Values used to check alert state. An alert threshold has two values that raise different levels of alarm: one warning threshold and one danger threshold. An alert may have one warning/danger tuple or 12 (one for each month).

**Cemetery**: A collection of specimen that are not owned anymore by the gardeners.

**Collection**: A list of specimens.

**Event**: a time-stamped journal entry. An event is always associated one or more journals. A list of non-exhaustive events:
- *care*: watering, fertilizing, repotting, pruning, cleaning
- *alert*: danger alert has been triggered
- *lifecycle*: specimen acquisition, propagation, flowering, death
- *meta*: specimen profile manual update

**Gardener**: The end user of the app.

**Journal**: A journal is an time-ordered set of events. A single event may belong to multiple journals when relevant.

**Notification**: A message to the gardener. It has a severity level: Info, Warning or Danger.

**Species**: The template for plant specimens of the same kind. If a species profile is updated, all its specimens inherit those changes unless explicitly overridden.

**Specimen**: living individual plant profile. Contains the plant’s personalized copy of data (its name, its pot, its current location, etc.) and links to its history of events.

**Task**: an event that symbolise a user action. It can be marked as done, or canceled.



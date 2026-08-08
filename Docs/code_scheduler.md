# Care scheduler

`CareScheduler` is responsible for scheduling and processing periodic care rules.

The scheduler does **not** create one platform timer per rule. Instead, it keeps track of the earliest `NextTrigger` stored in the database and asks the platform to wake the application only for that date.

## Scheduling model

Each `CareRule` contains a `NextTrigger` date and a trigger interval.

The scheduler maintains:

```csharp
_nextTriggerDate
```

which represents the next date for which the platform wake-up is currently planned.

The database remains the source of truth. The scheduler only caches the earliest trigger date in order to avoid unnecessary queries and platform rescheduling.

## Initialization

`InitializeAsync()` resets the cached trigger date and queries the database for the earliest scheduled rule.

The platform scheduler is then configured for this date.

## Adding, removing and updating rules

When a rule is added, the scheduler only replans if the new rule occurs before the currently scheduled trigger.

When a rule is removed, replanning is required only if the removed rule could have been responsible for the currently scheduled wake-up.

Updating a rule always replans, since its new `NextTrigger` may move either before or after the currently scheduled date.

## Replanning

`ReplanNextTriggerAsync()` queries the database for the minimum `NextTrigger` among all care rules:

```text
next = MIN(CareRule.NextTrigger)
```

If a trigger exists, `_nextTriggerDate` is updated and the platform is asked to schedule the next wake-up through:

```csharp
Platform.UpdateSchedulerTriggerTime(...)
```

Only one platform wake-up therefore needs to exist at any given time, regardless of the number of specimens or care rules.

## Processing

When the platform wakes the application, `ProcessCareAsync(now)` retrieves every rule whose `NextTrigger` is less than or equal to `now`.

For each rule:

1. The trigger date that caused the wake-up is saved.
2. The rule condition is evaluated.
3. If the rule is triggered, a `CareEvent` is published.
4. `NextTrigger` is advanced by its interval until it lies strictly after `now`.
5. The updated rule is persisted.

Advancing in a loop is important because the application may have missed several scheduled checks while it was suspended:

```csharp
do {
    rule.NextTrigger = rule.NextTrigger.AddSeconds(rule.TriggerInterval);
} while (rule.NextTrigger <= now);
```

This prevents a backlog of obsolete wake-ups.

After all due rules have been processed, the scheduler queries the database again and schedules the next earliest trigger.

## Care events

When a rule triggers, the scheduler publishes a `CareEvent` containing the relevant rule and specimen information, including the original `TriggeredAt` date.

The platform or application layer is responsible for deciding how this event is presented to the user, for example through the in-app alert list or a system notification.

## Main invariant

After initialization or completion of a scheduler operation:

```text
_nextTriggerDate = earliest pending CareRule.NextTrigger
```

and the platform scheduler should be configured to wake the application for that date.

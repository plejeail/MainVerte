# Coroutines for SQLite jobs

`Database` in `core/src/database.h` directly supports coroutine jobs through
`submit(job, dispatch)`. It does not expose SQLite in the public Core API. Jobs
remain synchronous functions executed serially by the database worker.
`submit(job)` remains available for jobs that do not need a coroutine result.

Inside an owning coroutine, persistence code can use:

```cpp
auto result = co_await database.submit([](sqlite3* handle) {
    // Execute an operation and return an owned value or explicit error result.
    return sqlite3_changes(handle);
}, dispatch);
if (!result) {
    // Database::Error::NotRunning: submission was refused.
    co_return;
}
const int changes = *result;
```

The caller supplies its coroutine return type and owns its frame. Database
does not create detached coroutines. Jobs can return values, move-only values,
`void`, or their own error results. The outer `std::expected` only represents
queue acceptance; a job's SQL/business error result is preserved inside it.
Jobs must not throw, retain the SQLite handle, or return SQLite-owned pointers.

## Resumption and lifetime

Each operation receives its own `dispatch`, a nonempty callable accepting
`mv::MoveOnlyFunction<void()>`. The worker calls it to enqueue a completion
on the original caller thread. It must publish preceding worker writes through
synchronization, must not execute the completion inline, and must not throw or
discard completions for live coroutines. Create, await, resume and destroy a
coroutine on that same caller thread.

The Qt adapter `qt_coroutine_dispatch(application)` in
`app_qt/src/coroutine_dispatch.h` enqueues completions on the application's main
thread with `Qt::QueuedConnection`. Core has no Qt dependency. App/platform code
supplies the dispatcher to persistence code; domain code keeps SQLite internal.

The operation owns its job, dispatcher and shared result storage. Destroying a
suspended coroutine detaches its continuation. Its accepted job still executes,
including any database writes, but its eventual completion does not resume the
destroyed frame. Captured references must therefore remain alive independently
of the coroutine; prefer capturing owned data. Cancellation does not roll back
an accepted write.

`Database` must remain alive until submission finishes. Its destructor drains
accepted jobs and joins the worker, without waiting for coroutine resumption.
Already queued results can resume afterwards, but those coroutines must not
use the destroyed database again.

Before stopping the Qt event loop, stop producers and either finish waiting
coroutines while their dispatch queue is running, or destroy their owning tasks
on the main thread. Then drain/destroy the database before destroying the
application/dispatcher target. A callback abandoned after its coroutine has
already been destroyed is safe. Waiting until `app.exec()` has returned to finish
live coroutines would leave their Qt completions unprocessed.

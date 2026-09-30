# Worker pools and groups

WorkerPool owns physical CPU threads. WorkerGroup is a copyable workload handle
sharing that capacity; a group does not create another set of threads. Separate
physical pools are still independently constructible. The default pool has one
worker so an EngineContext need not reserve machine-wide capacity accidentally.

Jobs own their callables and return futures. Callback exceptions reach those
futures, without stopping another job. FIFO selection within a group does not
promise completion order when its concurrency is greater than one. A group's
`max_concurrency` enforces its running-job limit; zero uses the pool's capacity.
Eligible groups currently receive fair round-robin selection; weighted priority
and native CPU policy are the next checkpoint.

`group.close()` stops acceptance without cancelling ordinary accepted work.
`group.drain()` requires closure, waits for pending/running work and capture
release, and leaves other groups available. `pool.close()` closes all groups;
`shutdown()` also joins. Destruction drains and joins. Saved group handles reject
safely after the pool disappears. Destroying the pool from one of its own jobs is
invalid; explicit self-join and same-pool drain reject before blocking.

Do not block a worker on child futures from the same saturated pool. Dependency
scheduling/work stealing is separate work. No worker is forcibly terminated.
Thread-start failure wakes and joins every thread already created. Scheduler
locks protect queue/accounting changes; user work and its captured destructors
run outside them. Submitted jobs retain their group even if its handle is dropped.

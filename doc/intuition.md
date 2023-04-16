# intuition behind the design of Zombie

## basic concept & interface
Zombie allows evicting a computed value and freeing the memory it uses.
When evicted values are needed later, they are recomputed.

A `Zombie<T>` represents a value that may live in memory,
or have been freed and require recomputation.
The user may request for a value of type `T` from a `Zombie<T>`.
When the value is in memory, it is returned directly.
Otherwise, the value is recomputed.


## a naive implementation
To recompute a value after it is freed,
the computation used to produce the value must be recorded (as a function).
So every zombie should be associated with its computation and an optional value.

To enable automatic eviction of computed values,
zombies are stored in a large table.
Entries in the table store a computation and an optional value.
Users of the zombies hold keys into the table.


## inefficiency of the naive implementation: nesting zombies
Suppose we have a zombie defined using the following function:

    pair<T, Zombie<U>> f() {
        T t = ...;
        Zombie<U> u = ...;
        return { t, u };
    }

This kind of pattern is very common when we zombify recursive data structures such as lists and trees.
During every recomputation of `f`, the `u`'es are identical.
So it would be desirable to share `u` between different recomputations of `f`.
However, in the naive implementation, every recomputation of `f` will generate a fresh `u` with a different key.
So there is no sharing between recomputations of `f`, and duplicated `u`s may pile up in the table.

To solve this inefficiency, zombies are not identified by a unique symbol, but a logical creation time.
A global clock is maintained and every zombie operation advances the clock by one unit,
so different zombies will have different creation time.
During the recompution of a zombie, we temporarily set the clock to the zombie's creation time.
So during different recomputations of a zombie, the nested zombies it generate will have the same key (creation time) and get shared.

The logical time approach can solve another inefficiency of the naive implementation.
In the naive implementation, while the value of a zombie can be freed, the associated computation and table entry cannot.
However, in the example above, the computation and table entry of `u` can be safely freed,
provided that the computation of `f` is not freed.
Since the computation of `u` is included in `f`,
and we can trigger recomputation of `u` by recomputing `f`.
In general, all zombies nested in another zombie can be safely freed.

To identify the nesting relation of zombies,
we annotate every zombie with not only its creation time, but also its *finish time*.
If the time interval of a zombie `x` is included in the time interval of another zombie `y`,
we can conclude that `x` is nested in `y`,
and freeing `x` is safe so long as `y` is not freed.


## tracking dependency with a monadic interface
Consider the following chain of computation, where `A`, `B` and `C` are data:

     A -> B -> C

Suppose that both `A`, `B` and `C` may be used later,
but we are lack of memory and only one of them can stay in memory,
then keeping `B` is the best choice,
because at most one recomputation is needed to obtain any of `A`, `B` and `C`.
If we keep `A`/`C` in memory instead, requesting `C`/`B` will require two recomputations.

To implement thin kind of heuristic,
the depenency information between computations is necessary.
So zombie uses a monadic API to keep track of dependencies efficiently.

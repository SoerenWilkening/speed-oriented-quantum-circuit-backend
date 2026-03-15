"""Per-variable history graph for automatic uncomputation.

Each qint/qbool carries a HistoryGraph that records the operations
(sequence_ptr, qubit_mapping) that produced it.  Children (intermediate
temporaries consumed during creation) are tracked via weakrefs so they
can be garbage-collected independently when no longer referenced
elsewhere.

The graph supports reverse iteration (for uncomputing in LIFO order)
and a discard() method (for measurement, where inverse gates are
meaningless because qubits have collapsed).
"""

import weakref


class HistoryGraph:
    """Lightweight history of operations that produced a quantum variable.

    Attributes
    ----------
    entries : list[tuple]
        List of (sequence_ptr, qubit_mapping) tuples in insertion order.
    children : list[weakref.ref]
        Weakrefs to child variables consumed during creation.
    """

    __slots__ = ("entries", "children")

    def __init__(self):
        self.entries = []
        self.children = []

    # ------------------------------------------------------------------
    # Recording
    # ------------------------------------------------------------------

    def append(self, sequence_ptr, qubit_mapping):
        """Record an operation entry.

        Parameters
        ----------
        sequence_ptr : int
            Pointer (as Python int) to the C ``sequence_t`` that was
            executed.
        qubit_mapping : tuple[int, ...]
            Qubit indices passed to ``run_instruction``.
        """
        self.entries.append((sequence_ptr, qubit_mapping))

    def add_child(self, child):
        """Register *child* as a weakref dependency.

        Parameters
        ----------
        child : object
            A qint/qbool whose lifetime is tracked via weakref.
        """
        self.children.append(weakref.ref(child))

    # ------------------------------------------------------------------
    # Iteration
    # ------------------------------------------------------------------

    def reversed_entries(self):
        """Yield entries in reverse (LIFO) order.

        Yields
        ------
        tuple
            (sequence_ptr, qubit_mapping) from newest to oldest.
        """
        return reversed(self.entries)

    # ------------------------------------------------------------------
    # Child access
    # ------------------------------------------------------------------

    def live_children(self):
        """Return list of children that are still alive.

        Returns
        -------
        list
            Dereferenced child objects (dead weakrefs are skipped).
        """
        alive = []
        for ref in self.children:
            obj = ref()
            if obj is not None:
                alive.append(obj)
        return alive

    # ------------------------------------------------------------------
    # Uncomputation
    # ------------------------------------------------------------------

    def uncompute(self, run_fn):
        """Uncompute all entries in reverse order, then cascade to children.

        Iterates entries newest-to-oldest, calling *run_fn* for each entry
        that has a real sequence pointer (non-zero).  After all entries are
        processed, cascades to orphaned children (alive weakrefs whose
        referent has a history with entries).

        Parameters
        ----------
        run_fn : callable(sequence_ptr, qubit_mapping)
            Callback that executes ``run_instruction(seq, mapping,
            invert=True)`` on the active circuit.  Provided by the
            Cython caller.
        """
        # 1. Reverse-iterate entries and invoke inverted run_instruction
        for seq_ptr, qubit_mapping in self.reversed_entries():
            if seq_ptr != 0:
                run_fn(seq_ptr, qubit_mapping)

        # 2. Clear own entries (idempotent: prevents double-uncompute)
        self.entries.clear()

        # 3. Cascade to orphaned children via weakrefs
        for ref in self.children:
            child = ref()
            if child is None:
                continue
            child_history = getattr(child, "history", None)
            if child_history is not None and child_history:
                child_history.uncompute(run_fn)

        # 4. Clear children list
        self.children.clear()

    # ------------------------------------------------------------------
    # Lifecycle
    # ------------------------------------------------------------------

    def discard(self):
        """Clear all entries and children.

        Called on measurement — qubits have collapsed so inverse gates
        are physically meaningless.
        """
        self.entries.clear()
        self.children.clear()

    # ------------------------------------------------------------------
    # Queries
    # ------------------------------------------------------------------

    def __len__(self):
        return len(self.entries)

    def __bool__(self):
        return len(self.entries) > 0

"""Quantum backtracking tree encoding (Montanaro 2015).

Provides QWalkTree for encoding backtracking search trees into quantum registers.
The tree uses a one-hot height register (max_depth+1 qubits) and per-level branch
registers. Local diffusion operators, walk operators, and detection are built on
top of this encoding in later modules.

References
----------
    A. Montanaro, "Quantum speedup of backtracking algorithms",
    Theory of Computing, 2018 (arXiv:1509.02374).
"""

import math

from ._gates import emit_x
from .qint import qint


class TreeNode:
    """Wraps tree registers for predicate evaluation.

    A lightweight state object passed to predicate callables. Provides
    access to the node's depth (via one-hot height register) and the
    branch values at each level.

    Parameters
    ----------
    height_register : qint
        One-hot height register (max_depth+1 qubits).
    branch_registers : list[qint]
        Per-level branch registers.
    max_depth : int
        Maximum tree depth.
    """

    __slots__ = ("_height_register", "_branch_registers", "_max_depth")

    def __init__(self, height_register, branch_registers, max_depth):
        self._height_register = height_register
        self._branch_registers = branch_registers
        self._max_depth = max_depth

    @property
    def depth(self):
        """One-hot height register as qint (max_depth+1 qubits).

        The qubit at position h[k] is |1> when the node is at depth k.
        """
        return self._height_register

    @property
    def branch_values(self):
        """List of qint branch registers, one per depth level."""
        return list(self._branch_registers)


class QWalkTree:
    """Quantum backtracking tree with one-hot height encoding.

    Encodes a backtracking search tree into quantum registers following
    Montanaro's scheme: a one-hot height register indicates which depth
    level the current node occupies, and per-level branch registers
    store which child was taken at each level.

    Parameters
    ----------
    max_depth : int
        Maximum depth of the tree (inclusive). A tree with max_depth=d
        has d+1 levels (0 through d).
    branching : int or list[int]
        Branching degree. If int, uniform branching at all levels.
        If list, per-level branching (length must equal max_depth).
    predicate : callable, optional
        Accept/reject predicate function. Receives a TreeNode and must
        return (is_accept, is_reject) as a tuple of two qbools.
        Validated at construction if provided.
    max_qubits : int, optional
        Maximum qubit budget for simulation (default 17). The tree can
        be constructed regardless of size; the budget is checked only
        when simulation is requested.

    Attributes
    ----------
    height_register : qint
        One-hot height register (max_depth+1 qubits).
    branch_registers : list[qint]
        Per-level branch registers.
    max_depth : int
        Maximum tree depth.
    branching : list[int]
        Normalized branching degrees (one per level).
    max_qubits : int
        Simulation qubit budget.
    node : TreeNode
        TreeNode accessor for predicate evaluation.
    total_qubits : int
        Total qubits used by the tree encoding.

    Examples
    --------
    >>> import quantum_language as ql
    >>> c = ql.circuit()
    >>> tree = ql.QWalkTree(max_depth=2, branching=2)
    >>> tree.total_qubits
    5
    >>> tree.height_register.width
    3
    """

    def __init__(self, max_depth, branching, predicate=None, *, max_qubits=17):
        self.max_depth = max_depth
        self.max_qubits = max_qubits
        self._predicate = predicate
        self._predicate_is_compiled = False

        # Normalize branching to per-level list
        if isinstance(branching, int):
            self.branching = [branching] * max_depth
        elif isinstance(branching, list | tuple):
            if len(branching) != max_depth:
                raise ValueError(
                    f"branching list length ({len(branching)}) must equal max_depth ({max_depth})"
                )
            self.branching = list(branching)
        else:
            raise ValueError(f"branching must be int or list, got {type(branching).__name__}")

        # Register allocation (eager, at construction)
        # Height register: one-hot encoding, max_depth+1 qubits
        self.height_register = qint(0, width=max_depth + 1)

        # Branch registers: one qint per depth level
        # Width = ceil(log2(branching[i])), minimum 1 qubit
        self.branch_registers = []
        for i in range(max_depth):
            b = self.branching[i]
            if b < 1:
                raise ValueError(f"branching degree must be >= 1, got {b} at level {i}")
            width = max(1, math.ceil(math.log2(b))) if b > 1 else 1
            self.branch_registers.append(qint(0, width=width))

        # Root state preparation: set height qubit for max_depth to |1>
        # In one-hot encoding, root is at depth max_depth (top of tree)
        # The MSB of the height register (qubits[63]) corresponds to bit max_depth
        root_qubit = self.height_register.qubits[63]
        emit_x(root_qubit)

        # Precompute diffusion angles for all depth levels
        self._setup_diffusion()

        # Predicate validation (if provided) -- called at scope depth 0
        # to avoid LIFO scope interference with returned qbools
        if self._predicate is not None:
            self._validate_predicate()

    @property
    def total_qubits(self):
        """Total qubits used by the tree encoding.

        Computed as height register width + sum of branch register widths.
        """
        height_qubits = self.max_depth + 1
        branch_qubits = sum(max(1, math.ceil(math.log2(b))) if b > 1 else 1 for b in self.branching)
        return height_qubits + branch_qubits

    @property
    def node(self):
        """TreeNode accessor for predicate evaluation."""
        return TreeNode(self.height_register, self.branch_registers, self.max_depth)

    def _check_qubit_budget(self):
        """Check if total qubits exceed the simulation budget.

        Raises
        ------
        ValueError
            If total_qubits exceeds max_qubits.
        """
        total = self.total_qubits
        if total > self.max_qubits:
            raise ValueError(
                f"Tree requires {total} qubits but simulation budget is "
                f"{self.max_qubits} qubits. Reduce tree size or increase "
                f"max_qubits parameter."
            )

    def _setup_diffusion(self):
        """Precompute diffusion rotation angles for all depth levels.

        Stores per-level angle data in ``self._diffusion_data`` (list indexed
        by level_idx 0..max_depth-1) and the special root angle in
        ``self._root_phi``.

        Angles follow Montanaro 2015 Section 2:
        - Parent-children split: phi = 2*arctan(sqrt(d))
        - Cascade angles: theta_k = 2*arctan(sqrt(1/(d-1-k))) for k=0..d-2
        - Root special case: phi_root = 2*arctan(sqrt(n*d_root))
        """
        self._diffusion_data = []

        for level_idx in range(self.max_depth):
            d = self.branching[level_idx]
            depth = self.max_depth - level_idx

            # Parent-children split: phi = 2*arctan(sqrt(d))
            phi = 2.0 * math.atan(math.sqrt(d))

            # Child cascade angles: theta_k = 2*arctan(sqrt(1/(d-1-k)))
            cascade_angles = []
            for k in range(max(0, d - 1)):
                remaining = d - 1 - k
                if remaining > 0:
                    theta = 2.0 * math.atan(math.sqrt(1.0 / remaining))
                else:
                    theta = 0.0
                cascade_angles.append(theta)

            self._diffusion_data.append(
                {
                    "phi": phi,
                    "cascade": cascade_angles,
                    "d": d,
                    "depth": depth,
                    "level_idx": level_idx,
                }
            )

        # Root angle: phi_root = 2*arctan(sqrt(n*d_root))
        d_root = self.branching[0]  # root is at level_idx=0
        n = self.max_depth
        self._root_phi = 2.0 * math.atan(math.sqrt(n * d_root))

    def _height_qubit(self, depth):
        """Get physical qubit index for height register bit at given depth.

        In one-hot encoding, h[depth] = |1> means the node is at depth
        ``depth``.  Root is at depth=max_depth, leaves at depth=0.

        Parameters
        ----------
        depth : int
            Depth level (0..max_depth).

        Returns
        -------
        int
            Physical qubit index for h[depth].
        """
        width = self.max_depth + 1
        return int(self.height_register.qubits[64 - width + depth])

    def diffusion_info(self, depth):
        """Return diffusion angle data for inspection at a given depth.

        Parameters
        ----------
        depth : int
            Depth level (0..max_depth).

        Returns
        -------
        dict
            Angle data including 'phi', 'cascade_angles', 'd', 'depth',
            'is_root', and optionally 'phi_root' or 'is_leaf'.

        Raises
        ------
        ValueError
            If depth is out of range.
        """
        if depth < 0 or depth > self.max_depth:
            raise ValueError(f"depth must be 0..{self.max_depth}, got {depth}")

        # Leaf: no children, no diffusion
        if depth == 0:
            return {"depth": 0, "d": 0, "is_leaf": True}

        level_idx = self.max_depth - depth
        data = self._diffusion_data[level_idx]
        info = {
            "phi": data["phi"],
            "cascade_angles": list(data["cascade"]),
            "d": data["d"],
            "depth": depth,
        }

        if depth == self.max_depth:
            info["is_root"] = True
            info["phi_root"] = self._root_phi
        else:
            info["is_root"] = False

        return info

    def _validate_predicate(self):
        """Validate predicate mutual exclusion on root state.

        Calls the predicate on the current (root) state and checks that
        both is_accept and is_reject are not simultaneously True.

        Raises
        ------
        ValueError
            If mutual exclusion is violated (both accept and reject are True).
        TypeError
            If predicate returns wrong type or format.
        """
        from .compile import CompiledFunc

        node = TreeNode(self.height_register, self.branch_registers, self.max_depth)
        result = self._predicate(node)

        if not isinstance(result, tuple | list) or len(result) != 2:
            raise TypeError(
                "Predicate must return (is_accept, is_reject) tuple of two qbools, "
                f"got {type(result).__name__}"
                + (f" of length {len(result)}" if isinstance(result, tuple | list) else "")
            )

        is_accept, is_reject = result

        # Store accept/reject qbools for later use by walk operators
        self._accept = is_accept
        self._reject = is_reject

        # Check if predicate is a compiled function
        if isinstance(self._predicate, CompiledFunc):
            self._predicate_is_compiled = True

        # Mutual exclusion check -- at construction, root is computational
        # basis state so qbool values are deterministic. Use .measure()
        # to access the classical initialization value (cdef field).
        accept_val = is_accept.measure()
        reject_val = is_reject.measure()
        if accept_val and reject_val:
            raise ValueError(
                "Predicate mutual exclusion violated: both accept and reject "
                "returned True for the root state. A node cannot be "
                "simultaneously accepted and rejected."
            )

    def uncompute_predicate(self):
        """Uncompute predicate qbools.

        For @ql.compile predicates, calls .adjoint() to replay the inverse
        gate sequence. For raw callables, this is a no-op (user manages
        cleanup).

        Note: Uses adjoint (standalone inverse replay) rather than inverse
        (which requires matching forward-call qubit tracking). The predicate
        receives a TreeNode wrapper, not raw qint arguments, so the
        inverse proxy cannot match inputs.
        """
        if self._predicate is None:
            return
        if self._predicate_is_compiled:
            node = TreeNode(self.height_register, self.branch_registers, self.max_depth)
            self._predicate.adjoint(node)

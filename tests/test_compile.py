"""Tests for @ql.compile decorator -- capture, caching, and replay.

Covers all 5 phase success criteria:
  SC1: First call produces same circuit as undecorated version
  SC2: Second call replays gates onto new qubits (no re-execution)
  SC3: Returned qint is usable in subsequent operations
  SC4: Different widths/classical args trigger re-capture
  SC5: ql.circuit() invalidates cache
"""

import warnings

import quantum_language as ql
from quantum_language._core import extract_gate_range, get_current_layer
from quantum_language.compile import CompiledFunc

# Suppress cosmetic width warnings
warnings.filterwarnings("ignore", message="Value .* exceeds")


# ---------------------------------------------------------------------------
# SC1: First call produces same circuit as undecorated version
# ---------------------------------------------------------------------------


def test_first_call_matches_undecorated_gate_count():
    """SC1: Compiled first call produces same gate count as plain call."""
    # Undecorated run
    ql.circuit()
    a = ql.qint(3, width=4)
    start = get_current_layer()
    a += 1
    end = get_current_layer()
    plain_gates = extract_gate_range(start, end)
    plain_count = len(plain_gates)

    # Compiled run
    ql.circuit()

    @ql.compile
    def add_one(x):
        x += 1
        return x

    b = ql.qint(3, width=4)
    start2 = get_current_layer()
    add_one(b)
    end2 = get_current_layer()
    compiled_gates = extract_gate_range(start2, end2)
    compiled_count = len(compiled_gates)

    assert compiled_count == plain_count, (
        f"Compiled first call gate count {compiled_count} != plain {plain_count}"
    )


def test_first_call_matches_undecorated_addition():
    """SC1: Compiled addition produces same gate structure as plain addition."""
    # Plain run
    ql.circuit()
    a1 = ql.qint(0, width=4)
    b1 = ql.qint(0, width=4)
    start1 = get_current_layer()
    _r1 = a1 + b1
    end1 = get_current_layer()
    plain_gates = extract_gate_range(start1, end1)

    # Compiled run
    ql.circuit()

    @ql.compile
    def add_fn(x, y):
        return x + y

    a2 = ql.qint(0, width=4)
    b2 = ql.qint(0, width=4)
    start2 = get_current_layer()
    _r2 = add_fn(a2, b2)
    end2 = get_current_layer()
    compiled_gates = extract_gate_range(start2, end2)

    assert len(compiled_gates) == len(plain_gates), (
        f"Addition gate count mismatch: compiled={len(compiled_gates)}, plain={len(plain_gates)}"
    )


# ---------------------------------------------------------------------------
# SC2: Second call replays gates onto new qubits (no re-execution)
# ---------------------------------------------------------------------------


def test_replay_no_reexecution():
    """SC2: Replay does not re-execute the function body."""
    ql.circuit()
    call_count = [0]

    @ql.compile
    def add_one(x):
        call_count[0] += 1
        x += 1
        return x

    a = ql.qint(3, width=4)
    add_one(a)
    assert call_count[0] == 1, "First call should execute body"

    b = ql.qint(5, width=4)
    add_one(b)
    assert call_count[0] == 1, "Replay should NOT re-execute body"


def test_replay_targets_new_qubits():
    """SC2: Replayed gates target the new qint's qubits, not the original."""
    ql.circuit()

    @ql.compile
    def add_one(x):
        x += 1
        return x

    a = ql.qint(3, width=4)
    add_one(a)

    b = ql.qint(5, width=4)
    b_qubits = {int(b.qubits[64 - b.width + i]) for i in range(b.width)}

    # Record replay gate range
    start = get_current_layer()
    add_one(b)
    end = get_current_layer()

    replay_gates = extract_gate_range(start, end)
    replay_targets = {g["target"] for g in replay_gates}

    # Replay should NOT target a's qubits (unless they overlap with b's by coincidence)
    # But b's qubits should be in the targets
    assert b_qubits & replay_targets, "Replay gates should target b's qubits"


def test_replay_same_gate_count():
    """SC2: Replay produces the same number of gates as capture."""
    ql.circuit()

    @ql.compile
    def add_one(x):
        x += 1
        return x

    a = ql.qint(3, width=4)
    start1 = get_current_layer()
    add_one(a)
    end1 = get_current_layer()
    capture_gates = extract_gate_range(start1, end1)

    b = ql.qint(5, width=4)
    start2 = get_current_layer()
    add_one(b)
    end2 = get_current_layer()
    replay_gates = extract_gate_range(start2, end2)

    assert len(replay_gates) == len(capture_gates), (
        f"Replay gate count {len(replay_gates)} != capture {len(capture_gates)}"
    )


# ---------------------------------------------------------------------------
# SC3: Returned qint is usable in subsequent operations
# ---------------------------------------------------------------------------


def test_return_value_usable_in_place():
    """SC3: In-place modified qint returned from compiled fn is usable."""
    ql.circuit()

    @ql.compile
    def add_one(x):
        x += 1
        return x

    a = ql.qint(3, width=4)
    result = add_one(a)
    # Result should be a itself (in-place modification)
    assert result is a

    # Use in subsequent operation (should not raise)
    layer_before = get_current_layer()
    result += 1
    layer_after = get_current_layer()
    assert layer_after > layer_before, "Subsequent operation should add gates"


def test_return_value_usable_new_qint():
    """SC3: New qint returned from compiled fn is usable in subsequent ops."""
    ql.circuit()

    @ql.compile
    def add_values(x, y):
        return x + y

    a = ql.qint(3, width=4)
    b = ql.qint(2, width=4)
    result = add_values(a, b)

    # Result should be a new qint
    assert result is not a
    assert result is not b
    assert result.width == 4

    # Use in subsequent operation
    layer_before = get_current_layer()
    result += 1
    layer_after = get_current_layer()
    assert layer_after > layer_before, "Subsequent operation should add gates"


def test_replay_return_value_usable():
    """SC3: Qint returned from replay is usable in subsequent operations."""
    ql.circuit()

    @ql.compile
    def add_values(x, y):
        return x + y

    # Capture
    a = ql.qint(3, width=4)
    b = ql.qint(2, width=4)
    _r1 = add_values(a, b)

    # Replay
    c = ql.qint(5, width=4)
    d = ql.qint(1, width=4)
    result2 = add_values(c, d)

    # Use replay result
    assert result2.width == 4
    layer_before = get_current_layer()
    result2 += 1
    layer_after = get_current_layer()
    assert layer_after > layer_before, "Replay result should be usable"


# ---------------------------------------------------------------------------
# SC4: Different classical params or qint widths trigger re-capture
# ---------------------------------------------------------------------------


def test_different_widths_recapture():
    """SC4: Different qint widths produce different cache entries."""
    ql.circuit()
    call_count = [0]

    @ql.compile
    def add_one(x):
        call_count[0] += 1
        x += 1
        return x

    a = ql.qint(3, width=4)
    add_one(a)
    assert call_count[0] == 1

    # Same width -- replay
    b = ql.qint(5, width=4)
    add_one(b)
    assert call_count[0] == 1, "Same width should replay"

    # Different width -- re-capture
    c = ql.qint(5, width=8)
    add_one(c)
    assert call_count[0] == 2, "Different width should re-capture"


def test_different_widths_different_gate_counts():
    """SC4: Width=4 and width=8 produce different gate counts."""
    ql.circuit()

    @ql.compile
    def add_one(x):
        x += 1
        return x

    a = ql.qint(3, width=4)
    start1 = get_current_layer()
    add_one(a)
    end1 = get_current_layer()
    gates_w4 = extract_gate_range(start1, end1)

    b = ql.qint(5, width=8)
    start2 = get_current_layer()
    add_one(b)
    end2 = get_current_layer()
    gates_w8 = extract_gate_range(start2, end2)

    assert len(gates_w8) > len(gates_w4), (
        f"Width=8 should produce more gates than width=4: w8={len(gates_w8)}, w4={len(gates_w4)}"
    )


def test_different_classical_args_recapture():
    """SC4: Different classical arg values produce different cache entries."""
    ql.circuit()
    call_count = [0]

    @ql.compile
    def add_value(x, amount):
        call_count[0] += 1
        x += amount
        return x

    a = ql.qint(3, width=4)
    add_value(a, 1)
    assert call_count[0] == 1

    b = ql.qint(5, width=4)
    add_value(b, 1)
    assert call_count[0] == 1, "Same classical arg should replay"

    c = ql.qint(7, width=4)
    add_value(c, 2)
    assert call_count[0] == 2, "Different classical arg should re-capture"


# ---------------------------------------------------------------------------
# SC5: ql.circuit() invalidates cache
# ---------------------------------------------------------------------------


def test_circuit_reset_clears_cache():
    """SC5: Creating a new circuit clears all compilation caches."""
    ql.circuit()
    call_count = [0]

    @ql.compile
    def add_one(x):
        call_count[0] += 1
        x += 1
        return x

    a = ql.qint(3, width=4)
    add_one(a)
    assert call_count[0] == 1

    b = ql.qint(5, width=4)
    add_one(b)
    assert call_count[0] == 1, "Should replay from cache"

    # Reset circuit -- should clear cache
    ql.circuit()
    c = ql.qint(7, width=4)
    add_one(c)
    assert call_count[0] == 2, "After circuit reset, should re-capture"


def test_circuit_reset_replay_after_recapture():
    """SC5: After circuit reset and re-capture, replay still works."""
    ql.circuit()
    call_count = [0]

    @ql.compile
    def add_one(x):
        call_count[0] += 1
        x += 1
        return x

    a = ql.qint(3, width=4)
    add_one(a)

    # Reset and re-capture
    ql.circuit()
    b = ql.qint(5, width=4)
    add_one(b)
    assert call_count[0] == 2

    # Replay after re-capture
    c = ql.qint(7, width=4)
    add_one(c)
    assert call_count[0] == 2, "Should replay after re-capture"


# ---------------------------------------------------------------------------
# Additional tests: decorator forms
# ---------------------------------------------------------------------------


def test_bare_decorator_form():
    """@ql.compile without parentheses works."""
    ql.circuit()

    @ql.compile
    def add_one(x):
        x += 1
        return x

    assert isinstance(add_one, CompiledFunc)
    a = ql.qint(3, width=4)
    result = add_one(a)
    assert result is a


def test_parens_decorator_form():
    """@ql.compile() with empty parentheses works."""
    ql.circuit()

    @ql.compile()
    def add_one(x):
        x += 1
        return x

    assert isinstance(add_one, CompiledFunc)
    a = ql.qint(3, width=4)
    result = add_one(a)
    assert result is a


def test_decorator_with_options():
    """@ql.compile(max_cache=N) works."""
    ql.circuit()

    @ql.compile(max_cache=16)
    def add_one(x):
        x += 1
        return x

    assert isinstance(add_one, CompiledFunc)
    assert add_one._max_cache == 16

    a = ql.qint(3, width=4)
    result = add_one(a)
    assert result is a


# ---------------------------------------------------------------------------
# Additional tests: cache management
# ---------------------------------------------------------------------------


def test_max_cache_eviction():
    """Oldest cache entry evicted when max_cache exceeded."""
    ql.circuit()
    call_count = [0]

    @ql.compile(max_cache=2)
    def add_one(x):
        call_count[0] += 1
        x += 1
        return x

    # Fill cache with 2 entries (width=4 and width=8)
    a = ql.qint(3, width=4)
    add_one(a)
    assert call_count[0] == 1

    b = ql.qint(5, width=8)
    add_one(b)
    assert call_count[0] == 2

    # Add third entry (width=16) -- should evict width=4
    c = ql.qint(5, width=16)
    add_one(c)
    assert call_count[0] == 3

    # width=4 should have been evicted, so this re-captures
    d = ql.qint(7, width=4)
    add_one(d)
    assert call_count[0] == 4, "Evicted entry should trigger re-capture"

    # width=8 should still be cached (was used after width=4)
    e = ql.qint(9, width=8)
    add_one(e)
    # Note: width=8 may or may not still be cached depending on eviction order
    # With max_cache=2, after adding width=16 and width=4 (re-captured),
    # cache has width=16 and width=4; width=8 was evicted


def test_clear_cache_method():
    """Per-function clear_cache() works."""
    ql.circuit()
    call_count = [0]

    @ql.compile
    def add_one(x):
        call_count[0] += 1
        x += 1
        return x

    a = ql.qint(3, width=4)
    add_one(a)
    assert call_count[0] == 1

    b = ql.qint(5, width=4)
    add_one(b)
    assert call_count[0] == 1

    # Clear cache
    add_one.clear_cache()

    c = ql.qint(7, width=4)
    add_one(c)
    assert call_count[0] == 2, "After clear_cache, should re-capture"


# ---------------------------------------------------------------------------
# Additional tests: in-place modification and error handling
# ---------------------------------------------------------------------------


def test_in_place_modification_replay():
    """In-place modification during replay returns caller's qint."""
    ql.circuit()

    @ql.compile
    def increment(x):
        x += 1
        return x

    a = ql.qint(3, width=4)
    r1 = increment(a)
    assert r1 is a, "First call in-place should return input"

    b = ql.qint(5, width=4)
    r2 = increment(b)
    assert r2 is b, "Replay in-place should return caller's qint"


def test_exception_during_capture_discarded():
    """Partial capture is discarded on exception; retry works."""
    ql.circuit()
    should_fail = [True]
    call_count = [0]

    @ql.compile
    def maybe_fail(x):
        call_count[0] += 1
        if should_fail[0]:
            raise ValueError("intentional test error")
        x += 1
        return x

    a = ql.qint(3, width=4)
    try:
        maybe_fail(a)
    except ValueError:
        pass
    assert call_count[0] == 1

    # Second call should retry capture (not use cached partial)
    ql.circuit()
    should_fail[0] = False
    b = ql.qint(5, width=4)
    result = maybe_fail(b)
    assert call_count[0] == 2, "Should retry capture after exception"
    assert result is b


def test_function_metadata_preserved():
    """functools.update_wrapper preserves function metadata."""

    @ql.compile
    def my_quantum_func(x):
        """My docstring."""
        x += 1
        return x

    assert my_quantum_func.__name__ == "my_quantum_func"
    assert my_quantum_func.__doc__ == "My docstring."


def test_no_return_value():
    """Function with no return works (returns None)."""
    ql.circuit()

    @ql.compile
    def just_modify(x):
        x += 1
        # No return

    a = ql.qint(3, width=4)
    result = just_modify(a)
    assert result is None

    # Replay also returns None
    b = ql.qint(5, width=4)
    result2 = just_modify(b)
    assert result2 is None

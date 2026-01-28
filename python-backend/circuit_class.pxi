# circuit_class.pxi - Circuit class for quantum circuit management
# This file is included by quantum_language.pyx
# Do not import directly

cdef class circuit:
	def __init__(self):
		"""Initialize quantum circuit.

		Creates a new quantum circuit instance. The circuit tracks gate
		operations and qubit allocations for quantum computations.

		Examples
		--------
		>>> c = circuit()
		>>> a = qint(5)
		>>> b = qint(3)
		>>> result = a + b
		"""
		global _circuit_initialized, _circuit, _num_qubits
		if not _circuit_initialized:
			_circuit = init_circuit()
		_circuit_initialized = True

	def add_qubits(self, qubits):
		"""Allocate additional qubits to the circuit.

		Parameters
		----------
		qubits : int
			Number of qubits to allocate.

		Examples
		--------
		>>> c = circuit()
		>>> c.add_qubits(5)
		"""
		global _num_qubits
		_num_qubits += qubits

	def visualize(self):
		"""Print circuit visualization to stdout.

		Shows horizontal layout with qubit indices, layer numbers, and gate
		symbols. Multi-qubit gates are connected with vertical lines.

		Notes
		-----
		Gate symbols: H (Hadamard), X/Y/Z (Pauli), P (Phase), @ (control), + (target)

		Examples
		--------
		>>> c = circuit()
		>>> a = qint(5, width=4)
		>>> b = qint(3, width=4)
		>>> _ = a + b
		>>> c.visualize()
		     0        5        10
		q0   ---H--@-------P--...
		q1   ------+--H--------...
		...
		"""
		circuit_visualize(<circuit_t*>_circuit)

	@property
	def gate_count(self):
		"""Total number of gates in circuit.

		Returns
		-------
		int
			Total gate count.

		Examples
		--------
		>>> c = circuit()
		>>> a = qint(5)
		>>> b = qint(3)
		>>> c = a + b
		>>> circuit().gate_count
		42
		"""
		return circuit_gate_count(<circuit_s*>_circuit)

	@property
	def depth(self):
		"""Circuit depth (number of layers).

		Each layer can execute in parallel on quantum hardware.

		Returns
		-------
		int
			Number of layers.

		Examples
		--------
		>>> c = circuit()
		>>> a = qint(5)
		>>> circuit().depth
		12
		"""
		return circuit_depth(<circuit_s*>_circuit)

	@property
	def qubit_count(self):
		"""Number of qubits used in circuit.

		Returns
		-------
		int
			Number of qubits.

		Examples
		--------
		>>> c = circuit()
		>>> a = qint(5, width=8)
		>>> circuit().qubit_count
		8
		"""
		return circuit_qubit_count(<circuit_s*>_circuit)

	@property
	def gate_counts(self):
		"""Breakdown of gate types in circuit.

		Returns
		-------
		dict
			Gate type to count mapping with keys 'X', 'Y', 'Z', 'H', 'P',
			'CNOT', 'CCX', 'other'.

		Examples
		--------
		>>> c = circuit()
		>>> a = qint(5)
		>>> circuit().gate_counts
		{'X': 3, 'H': 8, 'CNOT': 12, ...}
		"""
		cdef gate_counts_t counts = circuit_gate_counts(<circuit_s*>_circuit)
		return {
			'X': counts.x_gates,
			'Y': counts.y_gates,
			'Z': counts.z_gates,
			'H': counts.h_gates,
			'P': counts.p_gates,
			'CNOT': counts.cx_gates,
			'CCX': counts.ccx_gates,
			'other': counts.other_gates
		}

	@property
	def available_passes(self):
		"""List of available optimization passes.

		Returns
		-------
		list of str
			Pass names that can be used with optimize().

		Examples
		--------
		>>> c = circuit()
		>>> c.available_passes
		['merge', 'cancel_inverse']
		"""
		return AVAILABLE_PASSES.copy()

	def optimize(self, passes=None):
		"""Optimize circuit in-place and return before/after statistics.

		Modifies the current circuit by applying optimization passes.

		Parameters
		----------
		passes : list of str, optional
			Pass names to apply. Available: 'merge', 'cancel_inverse'.
			If None, applies all passes.

		Returns
		-------
		dict
			Statistics comparison with 'before' and 'after' dicts, each
			containing gate_count, depth, qubit_count, gate_counts.

		Raises
		------
		ValueError
			If any pass name is not in available_passes.
		RuntimeError
			If optimization fails.

		Examples
		--------
		>>> c = circuit()
		>>> a = qint(5, width=8)
		>>> # ... operations ...
		>>> stats = c.optimize()  # Run all passes
		>>> print(f"Reduced gates: {stats['before']['gate_count']} -> {stats['after']['gate_count']}")

		>>> stats = c.optimize(passes=['cancel_inverse'])  # Specific pass
		"""
		global _circuit

		cdef circuit_s *opt_circ

		# Capture stats BEFORE optimization
		before_stats = {
			'gate_count': self.gate_count,
			'depth': self.depth,
			'qubit_count': self.qubit_count,
			'gate_counts': self.gate_counts.copy()
		}

		# Validate pass names if provided
		if passes is not None:
			for p in passes:
				if p not in AVAILABLE_PASSES:
					raise ValueError(f"Unknown pass '{p}'. Available: {AVAILABLE_PASSES}")

		# Run optimization - creates optimized copy
		opt_circ = circuit_optimize(<circuit_s*>_circuit)

		if opt_circ == NULL:
			raise RuntimeError("Optimization failed")

		# Free the old circuit and replace with optimized one
		# Note: circuit_optimize creates a new circuit, we need to swap
		free_circuit(<circuit_t*>_circuit)
		_circuit = <circuit_t*>opt_circ

		# Capture stats AFTER optimization
		after_stats = {
			'gate_count': self.gate_count,
			'depth': self.depth,
			'qubit_count': self.qubit_count,
			'gate_counts': self.gate_counts.copy()
		}

		return {
			'before': before_stats,
			'after': after_stats
		}

	def can_optimize(self):
		"""Check if optimization would have any effect.

		Returns
		-------
		bool
			True if optimization would reduce circuit size.

		Examples
		--------
		>>> c = circuit()
		>>> if c.can_optimize():
		...     stats = c.optimize()
		"""
		return circuit_can_optimize(<circuit_s*>_circuit) != 0

	# def __str__(self):
	# 	print_circuit(_circuit)
	# 	return ""

	def __dealloc__(self):
		pass


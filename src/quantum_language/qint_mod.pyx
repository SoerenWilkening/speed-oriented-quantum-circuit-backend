"""Quantum integer with modular arithmetic.

Phase 91: Rewired to C-level modular reduction (fixes BUG-MOD-REDUCE).
All modular operations now use toffoli_mod_reduce from ToffoliModReduce.c
instead of the broken Python _reduce_mod which leaked orphan qubits.
"""

from libc.stdint cimport int64_t

from quantum_language.qint cimport qint

# C-level imports
from quantum_language._core cimport (
	circuit_t, circuit_s,
	toffoli_mod_reduce,
	toffoli_cmod_reduce,
	toffoli_mod_add_cq,
	toffoli_cmod_add_cq,
)

# Python-level imports for global state access
from quantum_language._core import (
	_get_controlled,
	_get_list_of_controls,
	_get_circuit, _get_circuit_initialized,
)


cdef class qint_mod(qint):
	"""Quantum integer with modular arithmetic.

	Modulus N is classical (Python int), known at circuit generation time.
	All operations automatically reduce result mod N.

	Parameters
	----------
	value : int, optional
		Initial value (reduced mod N classically before encoding).
	N : int
		Modulus (required, must be positive).
	width : int, optional
		Bit width (default: N.bit_length()).

	Attributes
	----------
	modulus : int
		The classical modulus N (read-only property).
	width : int
		Bit width (inherited from qint).

	Examples
	--------
	>>> x = qint_mod(5, N=17)
	>>> y = qint_mod(15, N=17)
	>>> z = x + y  # z = 20 mod 17 = 3
	>>> z.modulus
	17

	Notes
	-----
	Used for modular exponentiation in Shor's algorithm and other
	number-theoretic quantum algorithms.

	Phase 91: All modular reduction now uses C-level toffoli_mod_reduce
	from ToffoliModReduce.c, eliminating orphan qubit leaks from the
	old Python-level _reduce_mod (BUG-MOD-REDUCE).
	"""
	# Attribute declarations are in qint_mod.pxd

	def __init__(self, value=0, N=None, width=None):
		"""Create quantum integer with classical modulus N.

		Parameters
		----------
		value : int, optional
			Initial value (reduced mod N before encoding, default 0).
		N : int
			Modulus (required, must be positive).
		width : int, optional
			Bit width (default: N.bit_length()).

		Raises
		------
		ValueError
			If N is None, non-positive, or not an integer.

		Examples
		--------
		>>> x = qint_mod(25, N=17)  # Creates qint with value 25 % 17 = 8
		>>> x.modulus
		17
		"""
		if N is None or not isinstance(N, int) or N <= 0:
			raise ValueError("Modulus N must be positive integer")

		# Default width: enough bits to represent N
		if width is None:
			width = N.bit_length()

		# Reduce value mod N classically (before quantum encoding)
		reduced_value = value % N

		# Initialize as regular qint
		super().__init__(reduced_value, width=width)

		# Store modulus
		self._modulus = N

	@property
	def modulus(self):
		"""Get the modulus N (read-only).

		Returns
		-------
		int
			The classical modulus N.

		Examples
		--------
		>>> x = qint_mod(5, N=17)
		>>> x.modulus
		17
		"""
		return self._modulus

	def __repr__(self):
		return f"qint_mod(modulus={self._modulus}, width={self.bits})"

	cdef _reduce_mod_c(self, qint value):
		"""Reduce value mod N using C-level toffoli_mod_reduce.

		Phase 91: Replaces the old Python _reduce_mod which leaked qubits.

		For values in [0, 2N-2] (e.g., from addition of two values < N),
		a single call to toffoli_mod_reduce suffices.

		For larger values (e.g., from multiplication), multiple reduction
		passes are needed: ceil(log2(max_val / N)) iterations.

		Parameters
		----------
		value : qint
			Quantum register to reduce in-place.

		Returns
		-------
		qint
			The same register, now containing value mod N.
		"""
		cdef circuit_t *_circ = <circuit_t*><unsigned long long>_get_circuit()
		cdef int n = value.bits
		cdef int offset = 64 - n
		cdef unsigned int value_qa[64]
		cdef int i

		# Extract physical qubits (LSB-first for C convention)
		# qubits[64-bits] = LSB (bit 0), qubits[63] = MSB
		for i in range(n):
			value_qa[i] = value.qubits[offset + i]

		# Determine how many reduction passes are needed
		# For addition: input < 2N, so 1 pass suffices
		# For multiplication by c: input < c*N, need ceil(log2(c)) passes
		cdef int max_val = 1 << n
		cdef int max_subtractions = (max_val // self._modulus) + 1
		cdef int iterations = max_subtractions.bit_length()

		for i in range(iterations):
			toffoli_mod_reduce(_circ, value_qa, n, <int64_t>self._modulus)

		return value

	cdef qint_mod _wrap_result(self, qint result):
		"""Wrap plain qint result into qint_mod with same modulus."""
		# Create new qint_mod using existing qubits
		cdef qint_mod wrapped = qint_mod.__new__(qint_mod)

		# Manually copy qint fields (avoiding __init__ which would allocate qubits)
		wrapped.counter = result.counter
		wrapped.bits = result.bits
		wrapped.value = result.value
		wrapped.qubits = result.qubits
		wrapped.allocated_qubits = result.allocated_qubits
		wrapped.allocated_start = result.allocated_start

		# Set modulus (cdef attribute)
		wrapped._modulus = self._modulus

		return wrapped

	def __add__(self, other):
		"""Modular addition: (self + other) mod N

		Parameters
		----------
		other : qint_mod or int
			Value to add.

		Returns
		-------
		qint_mod
			Result reduced mod N.

		Raises
		------
		ValueError
			If other is qint_mod with different modulus.

		Examples
		--------
		>>> x = qint_mod(15, N=17)
		>>> y = qint_mod(5, N=17)
		>>> z = x + y  # (15 + 5) mod 17 = 3

		Notes
		-----
		Phase 91: Uses C-level toffoli_mod_reduce instead of Python _reduce_mod.
		"""
		# Check modulus compatibility
		if isinstance(other, qint_mod):
			if (<qint_mod>other)._modulus != self._modulus:
				raise ValueError(
					f"Moduli must match: {self._modulus} != {(<qint_mod>other)._modulus}"
				)

		# Perform regular addition (using parent class)
		sum_val = qint.__add__(self, other)

		# Reduce mod N using C-level function
		reduced = self._reduce_mod_c(sum_val)

		# Wrap result
		return self._wrap_result(reduced)

	def __sub__(self, other):
		"""Modular subtraction: (self - other) mod N

		Handles negative results by adding N to ensure [0, N) range.

		Parameters
		----------
		other : qint_mod or int
			Value to subtract.

		Returns
		-------
		qint_mod
			Result reduced mod N.

		Raises
		------
		ValueError
			If other is qint_mod with different modulus.

		Examples
		--------
		>>> x = qint_mod(5, N=17)
		>>> y = qint_mod(10, N=17)
		>>> z = x - y  # (5 - 10) mod 17 = 12

		Notes
		-----
		Phase 91: Uses C-level toffoli_mod_reduce instead of Python _reduce_mod.
		The subtraction result is an n-bit unsigned value that wraps around.
		For inputs < N, (self - other) mod 2^n is equivalent to
		(self - other + 2^n) mod N when the result is negative in signed
		interpretation. The C-level mod_reduce handles this by doing
		multiple passes of conditional subtraction of N.
		"""
		if isinstance(other, qint_mod):
			if (<qint_mod>other)._modulus != self._modulus:
				raise ValueError(
					f"Moduli must match: {self._modulus} != {(<qint_mod>other)._modulus}"
				)

		# Perform regular subtraction
		# In unsigned n-bit arithmetic: if self < other, result wraps to 2^n + (self - other)
		# which is > N. The reduce_mod_c will subtract N enough times to get to [0, N).
		diff = qint.__sub__(self, other)

		# For subtraction: if self >= other, diff is in [0, N) already (both < N).
		# If self < other, diff wraps to [2^n - N + 1, 2^n - 1].
		# We need to add N to get the correct modular result.
		# Instead of Python-level comparison (which leaks qbools), we add N
		# unconditionally and then reduce. Since the input is in [0, 2^n),
		# after adding N the max value is 2^n + N - 1, and the C-level
		# reduce_mod_c handles multiple passes.
		diff += self._modulus

		# Reduce mod N using C-level function
		reduced = self._reduce_mod_c(diff)

		return self._wrap_result(reduced)

	def __mul__(self, other):
		"""Modular multiplication: (self * other) mod N

		Parameters
		----------
		other : qint_mod or int
			Value to multiply by.

		Returns
		-------
		qint_mod
			Result reduced mod N.

		Raises
		------
		NotImplementedError
			If other is qint_mod (qint_mod * qint_mod not yet supported).
		ValueError
			If other is qint_mod with different modulus.

		Examples
		--------
		>>> x = qint_mod(5, N=17)
		>>> z = x * 7  # (5 * 7) mod 17 = 1

		Notes
		-----
		Phase 91: Uses C-level toffoli_mod_reduce instead of Python _reduce_mod.
		"""
		# Check for qint_mod * qint_mod (not supported - causes C-layer segfault)
		if isinstance(other, qint_mod):
			raise NotImplementedError(
				"qint_mod * qint_mod multiplication is not yet supported. "
				"Use qint_mod * int instead (e.g., x * 5 instead of x * y)."
			)

		# Perform regular multiplication (qint_mod * int)
		product = qint.__mul__(self, other)

		# Reduce mod N (product may be much larger than N)
		reduced = self._reduce_mod_c(product)

		return self._wrap_result(reduced)

	def __iadd__(self, other):
		"""In-place modular addition: self += other mod N

		Parameters
		----------
		other : qint_mod or int
			Value to add.

		Returns
		-------
		qint_mod
			Self (with swapped qubit references).

		Examples
		--------
		>>> x = qint_mod(15, N=17)
		>>> x += 5  # (15 + 5) mod 17 = 3
		"""
		result = self + other
		cdef qint_mod result_mod = <qint_mod>result
		self.qubits, result_mod.qubits = result_mod.qubits, self.qubits
		self.allocated_start, result_mod.allocated_start = result_mod.allocated_start, self.allocated_start
		self.bits = result_mod.bits
		return self

	def __isub__(self, other):
		"""In-place modular subtraction: self -= other mod N

		Parameters
		----------
		other : qint_mod or int
			Value to subtract.

		Returns
		-------
		qint_mod
			Self (with swapped qubit references).

		Examples
		--------
		>>> x = qint_mod(5, N=17)
		>>> x -= 10  # (5 - 10) mod 17 = 12
		"""
		result = self - other
		cdef qint_mod result_mod = <qint_mod>result
		self.qubits, result_mod.qubits = result_mod.qubits, self.qubits
		self.allocated_start, result_mod.allocated_start = result_mod.allocated_start, self.allocated_start
		self.bits = result_mod.bits
		return self

	def __imul__(self, other):
		"""In-place modular multiplication: self *= other mod N

		Parameters
		----------
		other : qint_mod or int
			Value to multiply by.

		Returns
		-------
		qint_mod
			Self (with swapped qubit references).

		Examples
		--------
		>>> x = qint_mod(5, N=17)
		>>> x *= 7  # (5 * 7) mod 17 = 1
		"""
		result = self * other
		cdef qint_mod result_mod = <qint_mod>result
		self.qubits, result_mod.qubits = result_mod.qubits, self.qubits
		self.allocated_start, result_mod.allocated_start = result_mod.allocated_start, self.allocated_start
		self.bits = result_mod.bits
		return self

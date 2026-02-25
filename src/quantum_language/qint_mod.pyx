"""Quantum integer with modular arithmetic.

Phase 92: All modular operations dispatch directly to C-level Beauregard
modular primitives (toffoli_mod_add_cq, toffoli_mod_sub_cq, etc.),
eliminating the broken add+reduce pattern from Phase 91 that leaked
persistent comparison ancillae.

Supported operations:
  - Addition: qint_mod + int (CQ), qint_mod + qint_mod (QQ)
  - Subtraction: qint_mod - int (CQ), qint_mod - qint_mod (QQ)
  - Multiplication: qint_mod * int (CQ), qint_mod * qint_mod (QQ)
  - Negation: -qint_mod
  - Controlled: all operations work inside with-block controlled context
  - Type infection: qint_mod op qint returns qint_mod

References:
  Beauregard (2003) "Circuit for Shor's algorithm using 2n+3 qubits"
"""

from libc.stdint cimport int64_t

from quantum_language.qint cimport qint

# C-level imports
from quantum_language._core cimport (
	circuit_t, circuit_s,
	toffoli_mod_add_cq,
	toffoli_cmod_add_cq,
	toffoli_mod_sub_cq,
	toffoli_cmod_sub_cq,
	toffoli_mod_add_qq,
	toffoli_cmod_add_qq,
	toffoli_mod_sub_qq,
	toffoli_cmod_sub_qq,
	toffoli_mod_mul_cq,
	toffoli_cmod_mul_cq,
	toffoli_mod_mul_qq,
	toffoli_cmod_mul_qq,
)

# Python-level imports for global state access
from quantum_language._core import (
	_get_controlled,
	_get_control_bool,
	_get_list_of_controls,
	_get_circuit, _get_circuit_initialized,
)


cdef class qint_mod(qint):
	"""Quantum integer with modular arithmetic.

	Modulus N is classical (Python int), known at circuit generation time.
	All operations automatically produce results mod N via Beauregard
	8-step modular addition sequences (Phase 92).

	Parameters
	----------
	value : int, optional
		Initial value (reduced mod N classically before encoding).
	N : int
		Modulus (required, must be >= 2).
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
	Phase 92: All modular operations dispatch to C-level Beauregard
	primitives. Clean ancilla uncomputation -- no persistent ancilla leaks.
	"""
	# Attribute declarations are in qint_mod.pxd

	def __init__(self, value=0, N=None, width=None):
		"""Create quantum integer with classical modulus N.

		Parameters
		----------
		value : int, optional
			Initial value (reduced mod N before encoding, default 0).
		N : int
			Modulus (required, must be >= 2).
		width : int, optional
			Bit width (default: N.bit_length()).

		Raises
		------
		ValueError
			If N is None, < 2, or not an integer.

		Examples
		--------
		>>> x = qint_mod(25, N=17)  # Creates qint with value 25 % 17 = 8
		>>> x.modulus
		17
		"""
		if N is None or not isinstance(N, int) or N < 2:
			raise ValueError("Modulus N must be an integer >= 2")

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

	cdef qint_mod _wrap_result(self, qint result):
		"""Wrap plain qint result into qint_mod with same modulus."""
		cdef qint_mod wrapped = qint_mod.__new__(qint_mod)

		wrapped.counter = result.counter
		wrapped.bits = result.bits
		wrapped.value = result.value
		wrapped.qubits = result.qubits
		wrapped.allocated_qubits = result.allocated_qubits
		wrapped.allocated_start = result.allocated_start

		wrapped._modulus = self._modulus

		return wrapped

	cdef void _extract_qubits(self, unsigned int *qa):
		"""Extract physical qubit indices into C array (LSB-first)."""
		cdef int n = self.bits
		cdef int offset = 64 - n
		cdef int i
		for i in range(n):
			qa[i] = self.qubits[offset + i]

	def __add__(self, other):
		"""Modular addition: (self + other) mod N

		Parameters
		----------
		other : qint_mod, qint, or int
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
		"""
		cdef circuit_t *_circ = <circuit_t*><unsigned long long>_get_circuit()
		cdef int n = self.bits
		cdef unsigned int value_qa[64]
		cdef unsigned int result_qa[64]
		cdef unsigned int other_qa[64]
		cdef int i
		cdef bint _controlled = _get_controlled()
		cdef unsigned int ctrl_qubit = 0
		cdef int other_bits
		cdef int other_offset

		# Check modulus compatibility
		if isinstance(other, qint_mod):
			if (<qint_mod>other)._modulus != self._modulus:
				raise ValueError(
					f"Moduli must match: {self._modulus} != {(<qint_mod>other)._modulus}"
				)

		# Extract self qubits
		self._extract_qubits(value_qa)

		# Allocate result register and copy self to it
		result = qint(width=n)
		result ^= self

		# Extract result qubits
		cdef int result_offset = 64 - n
		for i in range(n):
			result_qa[i] = result.qubits[result_offset + i]

		# Get control qubit if in controlled context
		if _controlled:
			ctrl_qubit = (<qint>_get_control_bool()).qubits[63]

		if type(other) == int:
			# CQ modular addition
			if _controlled:
				toffoli_cmod_add_cq(_circ, result_qa, n,
				                    <int64_t>(other % self._modulus), <int64_t>self._modulus,
				                    ctrl_qubit)
			else:
				toffoli_mod_add_cq(_circ, result_qa, n,
				                   <int64_t>(other % self._modulus), <int64_t>self._modulus)
		elif isinstance(other, qint):
			# QQ modular addition (works for both qint_mod and plain qint)
			other_bits = (<qint>other).bits
			other_offset = 64 - other_bits
			for i in range(other_bits):
				other_qa[i] = (<qint>other).qubits[other_offset + i]

			if _controlled:
				toffoli_cmod_add_qq(_circ, result_qa, n, other_qa, other_bits,
				                    <int64_t>self._modulus, ctrl_qubit)
			else:
				toffoli_mod_add_qq(_circ, result_qa, n, other_qa, other_bits,
				                   <int64_t>self._modulus)
		else:
			raise TypeError(f"Unsupported operand type for +: {type(other)}")

		return self._wrap_result(result)

	def __radd__(self, other):
		"""Reverse addition: other + self (for int + qint_mod)."""
		return self.__add__(other)

	def __sub__(self, other):
		"""Modular subtraction: (self - other) mod N

		Parameters
		----------
		other : qint_mod, qint, or int
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
		"""
		cdef circuit_t *_circ = <circuit_t*><unsigned long long>_get_circuit()
		cdef int n = self.bits
		cdef unsigned int value_qa[64]
		cdef unsigned int result_qa[64]
		cdef unsigned int other_qa[64]
		cdef int i
		cdef bint _controlled = _get_controlled()
		cdef unsigned int ctrl_qubit = 0
		cdef int other_bits_sub
		cdef int other_offset_sub

		if isinstance(other, qint_mod):
			if (<qint_mod>other)._modulus != self._modulus:
				raise ValueError(
					f"Moduli must match: {self._modulus} != {(<qint_mod>other)._modulus}"
				)

		self._extract_qubits(value_qa)

		# Allocate result register and copy self to it
		result = qint(width=n)
		result ^= self

		cdef int result_offset_sub = 64 - n
		for i in range(n):
			result_qa[i] = result.qubits[result_offset_sub + i]

		if _controlled:
			ctrl_qubit = (<qint>_get_control_bool()).qubits[63]

		if type(other) == int:
			# CQ modular subtraction
			if _controlled:
				toffoli_cmod_sub_cq(_circ, result_qa, n,
				                    <int64_t>(other % self._modulus), <int64_t>self._modulus,
				                    ctrl_qubit)
			else:
				toffoli_mod_sub_cq(_circ, result_qa, n,
				                   <int64_t>(other % self._modulus), <int64_t>self._modulus)
		elif isinstance(other, qint):
			# QQ modular subtraction
			other_bits_sub = (<qint>other).bits
			other_offset_sub = 64 - other_bits_sub
			for i in range(other_bits_sub):
				other_qa[i] = (<qint>other).qubits[other_offset_sub + i]

			if _controlled:
				toffoli_cmod_sub_qq(_circ, result_qa, n, other_qa, other_bits_sub,
				                    <int64_t>self._modulus, ctrl_qubit)
			else:
				toffoli_mod_sub_qq(_circ, result_qa, n, other_qa, other_bits_sub,
				                   <int64_t>self._modulus)
		else:
			raise TypeError(f"Unsupported operand type for -: {type(other)}")

		return self._wrap_result(result)

	def __mul__(self, other):
		"""Modular multiplication: (self * other) mod N

		Parameters
		----------
		other : qint_mod, qint, or int
			Value to multiply by.

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
		>>> z = x * 7  # (5 * 7) mod 17 = 1
		>>> x = qint_mod(3, N=7)
		>>> y = qint_mod(4, N=7)
		>>> z = x * y  # (3 * 4) mod 7 = 5
		"""
		cdef circuit_t *_circ = <circuit_t*><unsigned long long>_get_circuit()
		cdef int n = self.bits
		cdef unsigned int value_qa[64]
		cdef unsigned int result_qa[64]
		cdef unsigned int other_qa[64]
		cdef int i
		cdef bint _controlled = _get_controlled()
		cdef unsigned int ctrl_qubit = 0
		cdef int64_t mul_val
		cdef int other_bits_mul
		cdef int other_offset_mul

		if isinstance(other, qint_mod):
			if (<qint_mod>other)._modulus != self._modulus:
				raise ValueError(
					f"Moduli must match: {self._modulus} != {(<qint_mod>other)._modulus}"
				)

		self._extract_qubits(value_qa)

		# Allocate result register (init |0>)
		result = qint(width=n)

		cdef int result_offset_mul = 64 - n
		for i in range(n):
			result_qa[i] = result.qubits[result_offset_mul + i]

		if _controlled:
			ctrl_qubit = (<qint>_get_control_bool()).qubits[63]

		if type(other) == int:
			# CQ modular multiplication: result = self * other mod N
			mul_val = <int64_t>(other % self._modulus)
			if mul_val < 0:
				mul_val += self._modulus
			if _controlled:
				toffoli_cmod_mul_cq(_circ, value_qa, n, result_qa, n,
				                    mul_val, <int64_t>self._modulus, ctrl_qubit)
			else:
				toffoli_mod_mul_cq(_circ, value_qa, n, result_qa, n,
				                   mul_val, <int64_t>self._modulus)
		elif isinstance(other, qint):
			# QQ modular multiplication: result = self * other mod N
			other_bits_mul = (<qint>other).bits
			other_offset_mul = 64 - other_bits_mul
			for i in range(other_bits_mul):
				other_qa[i] = (<qint>other).qubits[other_offset_mul + i]

			if _controlled:
				toffoli_cmod_mul_qq(_circ, value_qa, n, other_qa, other_bits_mul,
				                    result_qa, n, <int64_t>self._modulus, ctrl_qubit)
			else:
				toffoli_mod_mul_qq(_circ, value_qa, n, other_qa, other_bits_mul,
				                   result_qa, n, <int64_t>self._modulus)
		else:
			raise TypeError(f"Unsupported operand type for *: {type(other)}")

		return self._wrap_result(result)

	def __rmul__(self, other):
		"""Reverse multiplication: other * self (for int * qint_mod)."""
		return self.__mul__(other)

	def __neg__(self):
		"""Modular negation: (-self) mod N = (N - self) mod N

		Returns
		-------
		qint_mod
			New qint_mod with value (N - self) mod N.

		Examples
		--------
		>>> x = qint_mod(5, N=17)
		>>> y = -x  # (17 - 5) mod 17 = 12
		>>> x = qint_mod(0, N=7)
		>>> y = -x  # (7 - 0) mod 7 = 0
		"""
		cdef circuit_t *_circ = <circuit_t*><unsigned long long>_get_circuit()
		cdef int n = self.bits
		cdef unsigned int value_qa[64]
		cdef unsigned int result_qa[64]
		cdef int i

		self._extract_qubits(value_qa)

		# Allocate result register (init |0>)
		result = qint(width=n)

		cdef int result_offset_neg = 64 - n
		for i in range(n):
			result_qa[i] = result.qubits[result_offset_neg + i]

		# result = (0 - self) mod N = (N - self) mod N
		toffoli_mod_sub_qq(_circ, result_qa, n, value_qa, n, <int64_t>self._modulus)

		return self._wrap_result(result)

	def __iadd__(self, other):
		"""In-place modular addition: self += other mod N

		Parameters
		----------
		other : qint_mod, qint, or int
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
		other : qint_mod, qint, or int
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
		other : qint_mod, qint, or int
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

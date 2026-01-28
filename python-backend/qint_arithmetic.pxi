# qint_arithmetic.pxi - Arithmetic operations for qint class
# This file is included by quantum_language.pyx
# Do not import directly

	cdef addition_inplace(self, other: qint | int, invert: int  = False):
		global _controlled, _control_bool, qubit_array
		cdef sequence_t *seq
		cdef unsigned int[:] arr
		cdef int result_bits
		cdef int other_bits
		cdef int self_offset
		cdef int other_offset

		start = 0

		# Extract only the used qubits (right-aligned in 64-element array)
		# self.qubits[64-self.bits:64] contains the actual qubit indices
		self_offset = 64 - self.bits
		qubit_array[:self.bits] = self.qubits[self_offset:64]
		start += self.bits

		if type(other) == int:
			# value is a classical integer
			if _controlled:
				# Control qubit from qbool (last element)
				qubit_array[start: start + 1] = (<qbool> _control_bool).qubits[63:64]
				qubit_array[start + 1: start + 1 + NUMANCILLY] = ancilla
				seq = cCQ_add(self.bits, other)
			else:
				qubit_array[start: start + NUMANCILLY] = ancilla
				seq = CQ_add(self.bits, other)


			arr = qubit_array
			run_instruction(seq, &arr[0], invert, _circuit)

			return self
		if not isinstance(other, qint):
			raise ValueError()


		# other type is qint as well - determine result width
		other_bits = (<qint> other).bits
		result_bits = max(self.bits, other_bits)
		other_offset = 64 - other_bits

		# Extract used qubits from other
		qubit_array[start: start + other_bits] = (<qint> other).qubits[other_offset:64]
		start += other_bits


		if _controlled:
			# Control qubit from qbool (last element)
			qubit_array[start: start + 1] = (<qbool> _control_bool).qubits[63:64]
			qubit_array[start + 1: start + 1 + NUMANCILLY] = ancilla
			seq = cQQ_add(result_bits)
		else:
			qubit_array[start: start + NUMANCILLY] = ancilla
			seq = QQ_add(result_bits)

		arr = qubit_array
		run_instruction(seq, &arr[0], invert, _circuit)
		return self

	def __add__(self, other: qint | int):
		"""Add quantum integers: self + other

		Result width is max(self.width, other.width). Overflow wraps (modular).

		Parameters
		----------
		other : qint or int
			Value to add.

		Returns
		-------
		qint
			New quantum integer containing sum.

		Examples
		--------
		>>> a = qint(5, width=8)
		>>> b = qint(3, width=8)
		>>> c = a + b
		>>> c.width
		8
		"""
		# out of place addition - result width is max of operands
		if type(other) == qint:
			result_width = max(self.bits, (<qint>other).bits)
		else:
			result_width = self.bits
		a = qint(value = self.value, width = result_width)
		a += other
		return a

	def __radd__(self, other: qint | int):
		"""Reverse addition: other + self (for int + qint).

		Parameters
		----------
		other : int
			Classical value to add.

		Returns
		-------
		qint
			New quantum integer containing sum.

		Examples
		--------
		>>> a = qint(5, width=8)
		>>> b = 3 + a  # Uses __radd__
		>>> b.width
		8
		"""
		# out of place addition - result width is max of operands
		if type(other) == qint:
			result_width = max(self.bits, (<qint>other).bits)
		else:
			result_width = self.bits
		a = qint(value = self.value, width = result_width)
		a += other
		return a

	def __iadd__(self, other: qint | int):
		"""In-place addition: self += other

		Parameters
		----------
		other : qint or int
			Value to add.

		Returns
		-------
		qint
			Self (modified in-place via quantum gates).

		Examples
		--------
		>>> a = qint(5, width=8)
		>>> a += 3
		>>> # a now represents |5+3> = |8>
		"""
		# in place addition
		return self.addition_inplace(other)

	def __sub__(self, other: qint | int):
		"""Subtract quantum integers: self - other

		Result width is max(self.width, other.width). Underflow wraps (modular).

		Parameters
		----------
		other : qint or int
			Value to subtract.

		Returns
		-------
		qint
			New quantum integer containing difference.

		Examples
		--------
		>>> a = qint(5, width=8)
		>>> b = qint(3, width=8)
		>>> c = a - b
		>>> c.width
		8
		"""
		# out of place subtraction - result width is max of operands
		if type(other) == qint:
			result_width = max(self.bits, (<qint>other).bits)
		else:
			result_width = self.bits
		a = qint(value = self.value, width = result_width)
		a -= other
		return a

	def __isub__(self, other: qint | int):
		"""In-place subtraction: self -= other

		Parameters
		----------
		other : qint or int
			Value to subtract.

		Returns
		-------
		qint
			Self (modified in-place via quantum gates).

		Examples
		--------
		>>> a = qint(5, width=8)
		>>> a -= 3
		>>> # a now represents |5-3> = |2>
		"""
		# in place addition
		return self.addition_inplace(other, invert = True)


	cdef multiplication_inplace(self, other: qint | int, ret: qint):
			global _controlled, _control_bool, qubit_array
			cdef sequence_t *seq
			cdef unsigned int[:] arr
			cdef int result_bits
			cdef int other_bits
			cdef int self_offset
			cdef int ret_offset
			cdef int other_offset

			start = 0

			# Determine result width (must match ret's width)
			result_bits = (<qint>ret).bits

			# Multiplication layout: ret (accumulator) at position 0, self at position result_bits
			# Extract only used qubits (right-aligned in 64-element array)
			self_offset = 64 - self.bits
			ret_offset = 64 - result_bits

			# ret qubits at position 0
			qubit_array[:result_bits] = (<qint>ret).qubits[ret_offset:64]
			# self qubits at position result_bits
			qubit_array[result_bits: result_bits + self.bits] = self.qubits[self_offset:64]
			start = result_bits + self.bits

			if type(other) == int:
				# Classical-quantum multiplication
				if _controlled:
					# Control qubit from qbool (last element)
					qubit_array[start: start + 1] = (<qbool> _control_bool).qubits[63:64]
					qubit_array[start + 1: start + 1 + NUMANCILLY] = ancilla
					seq = cCQ_mul(result_bits, other)  # Pass bits parameter
				else:
					qubit_array[start: start + NUMANCILLY] = ancilla
					seq = CQ_mul(result_bits, other)  # Pass bits parameter

				if seq == NULL:
					raise RuntimeError(f"Multiplication circuit generation failed for width {result_bits}")

				arr = qubit_array
				run_instruction(seq, &arr[0], False, _circuit)
				return ret

			if not isinstance(other, qint):
				raise TypeError("Multiplication requires qint or int")

			# Quantum-quantum multiplication
			other_bits = (<qint> other).bits
			other_offset = 64 - other_bits

			# other qubits at position start
			qubit_array[start: start + other_bits] = (<qint> other).qubits[other_offset:64]
			start += other_bits

			if _controlled:
				qubit_array[start: start + 1] = (<qbool> _control_bool).qubits[63:64]
				qubit_array[start + 1: start + 1 + NUMANCILLY] = ancilla
				seq = cQQ_mul(result_bits)  # Pass bits parameter
			else:
				qubit_array[start: start + NUMANCILLY] = ancilla
				seq = QQ_mul(result_bits)  # Pass bits parameter

			if seq == NULL:
				raise RuntimeError(f"Multiplication circuit generation failed for width {result_bits}")

			arr = qubit_array
			run_instruction(seq, &arr[0], False, _circuit)
			return ret

	def __mul__(self, other):
		"""Multiply quantum integers.

		Result width is max(self.width, other.width) per CONTEXT.md.
		Overflow wraps silently (modular arithmetic).

		Args:
			other: qint or int to multiply by

		Returns:
			New qint containing product

		Examples:
			>>> a = qint(3, width=8)
			>>> b = qint(4, width=16)
			>>> c = a * b
			>>> c.width
			16
		"""
		# Determine result width
		if isinstance(other, qint):  # Includes qint subclasses like qint_mod
			result_width = max(self.bits, (<qint>other).bits)
		elif type(other) == int:
			result_width = self.bits
		else:
			raise TypeError("Multiplication requires qint or int")

		# Allocate result with correct width
		result = qint(width=result_width)

		# Perform multiplication into result
		self.multiplication_inplace(other, result)

		return result

	def __rmul__(self, other):
		"""Reverse multiplication: other * self (for int * qint).

		Args:
			other: int to multiply by (qint * qint uses __mul__)

		Returns:
			New qint containing product

		Examples:
			>>> a = qint(5, width=8)
			>>> b = 3 * a  # Uses __rmul__
			>>> b.width
			8
		"""
		# For int * qint, result width is qint's width
		if type(other) == int:
			result_width = self.bits
		else:
			# qint * qint should use __mul__, not __rmul__
			result_width = max(self.bits, (<qint>other).bits)

		result = qint(width=result_width)
		self.multiplication_inplace(other, result)
		return result

	def __imul__(self, other):
		"""In-place multiplication: self *= other

		Note: Due to quantum mechanics, in-place multiplication allocates
		new qubits for the result and swaps qubit references.

		Args:
			other: qint or int to multiply by

		Returns:
			self (with swapped qubit references)

		Examples:
			>>> a = qint(3, width=8)
			>>> a *= 4
			>>> # a now references new qubits containing 3*4
		"""
		# Perform out-of-place multiplication
		result = self * other

		# Swap qubit arrays (like __iand__ pattern from Phase 6)
		cdef qint result_qint = <qint>result

		# Swap qubit references
		self.qubits, result_qint.qubits = result_qint.qubits, self.qubits
		self.allocated_start, result_qint.allocated_start = result_qint.allocated_start, self.allocated_start
		self.bits = result_qint.bits

		return self

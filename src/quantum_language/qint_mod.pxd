from libc.stdint cimport int64_t

from quantum_language.qint cimport qint

cdef class qint_mod(qint):
	cdef int64_t _modulus

	# cdef method declarations
	cdef qint_mod _wrap_result(self, qint result)
	cdef void _extract_qubits(self, unsigned int *qa)

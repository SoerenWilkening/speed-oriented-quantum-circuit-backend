from quantum_language.qint cimport qint

cdef class qint_mod(qint):
	cdef int _modulus

	# cdef method declarations
	cdef _reduce_mod_c(self, qint value)
	cdef qint_mod _wrap_result(self, qint result)

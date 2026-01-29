from quantum_language.qint cimport qint

cdef class qint_mod(qint):
	cdef int _modulus

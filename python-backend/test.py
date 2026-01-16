import quantum_language as ql

A = ql.qint()
B = ql.qint()
c = ql.qbool()
d = ql.qbool()

# with c:
# A = 3 * A

# A += 3
# print(A[0])
A < 3


# print(A)

# with c:
# 	C = 5 + B
# 	B += 3
# 	A += B + C
# 	with d:
# 		3 + B
# with ~c:
# 	pass


A.print_circuit()
# A.print_circuit()
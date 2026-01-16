import quantum_language as ql

A = ql.qint()
B = ql.qint()
c = ql.qbool()
d = ql.qbool()

print(A)

with c:
	A += 3
C = 3 * A

H = ql.qint()


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
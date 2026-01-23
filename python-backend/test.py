import quantum_language as ql

count_cross_in_row = ql.qint(bits = 2)
count_circle_in_row = ql.qint(bits = 2)
count_cross_in_line = ql.qint(bits = 2)
count_circle_in_line = ql.qint(bits = 2)

count_cross_wins = ql.qint(bits = 4)

count_cross_in_diagonally1 = ql.qint(bits = 2)
count_cross_in_diagonally2 = ql.qint(bits = 2)
count_circle_in_diagonally1 = ql.qint(bits = 2)
count_circle_in_diagonally2 = ql.qint(bits = 2)

CROSS = 0
CIRCLE = 1

assigned = ql.array((3, 3), dtype=ql.qbool)
unoccupied = ql.array((3, 3), dtype=ql.qbool)
for i in range(3):
	for j in range(3):
		unoccupied[i][j] = ~unoccupied[i][j] # set to 1 to make squares uninitialized


# def detect_win():
# global count_cross_in_diagonally1, count_cross_in_line, count_cross_in_row, count_cross_in_diagonally2, count_cross_wins
for row in range(3):
	for line in range(3):
		# crosses in line
		with ~assigned[row][line] & ~unoccupied[row][line]:
			count_cross_in_line += 1
		# crosses in row
		with ~assigned[line][row] & ~unoccupied[line][row]:
			count_cross_in_row += 1

	# increase counter for cross wins. only if counter is 0, cross didnt win
	with ~(count_cross_in_line < 3):
		count_cross_wins += 1
	with ~(count_cross_in_row < 3):
		count_cross_wins += 1

	# uncompute to free up the registers again
	for line in range(3):
		with ~assigned[row][line] & ~unoccupied[row][line]:
			count_cross_in_line -= 1

		with ~assigned[line][row] & ~unoccupied[line][row]:
			count_cross_in_row -= 1


	# compute , if diagonally 3 are connected
	with ~assigned[row][row] & ~unoccupied[row][row]:
		count_cross_in_diagonally1 += 1

	with ~assigned[2 - row][row] & ~unoccupied[2 - row][row]:
		count_cross_in_diagonally2 += 1

with ~(count_circle_in_diagonally1 < 3):
	count_cross_wins += 1
with ~(count_circle_in_diagonally2 < 3):
	count_cross_wins += 1

# uncompute
for row in range(3):
	with ~assigned[row][row] & ~unoccupied[row][row]:
		count_cross_in_diagonally1 += 1

	with ~assigned[2 - row][row] & ~unoccupied[2 - row][row]:
		count_cross_in_diagonally2 += 1


cross_win = ~(count_cross_wins < 1)



# detect_win()

count_circle_in_row.print_circuit()

# 30 32, 37

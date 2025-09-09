from qutip import tensor, basis, Qobj
import os
from time import time
import numpy as np

# from qutip_qip.qasm import read_qasm
# qc = read_qasm("../../circuit.qasm")
# print(qc)

import qiskit.qasm3
from qiskit import QuantumCircuit, transpile

from qiskit import Aer
from qiskit import execute

# Use the Aer simulator
simulator = Aer.get_backend('aer_simulator')

read = qiskit.qasm3.load("/Users/sorenwilkening/Desktop/UNI/Promotion/Projects/Quantum Programming Language/Quantum_Assembly/circuit.qasm")
n = len(read.qubits)
t1 = time()
circuit = QuantumCircuit(n)

circuit.append(read, range(n))
circuit.measure_all()
circuit = transpile(circuit)
print(time() - t1)
print(circuit.num_qubits)
job = execute(circuit, backend=simulator, shots=1024)
result = job.result().get_counts()
for i in result:
    print(result[i], end = " ")
    i = i[::-1]
    print(i[0], i[1], end = " ")
    i = i[2:]
    print(i[:8], int(i[:8], 2), i[8:16], int(i[8:16], 2))
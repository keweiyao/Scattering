#!/usr/bin/env python3

import matplotlib.pyplot as plt
import numpy as np
import h5py


f = h5py.File('./table.h5', 'r')

for process,c in zip(['Qq2Qq', 'Qg2Qg'], ['r','g','b']):
	a = f[process+'/xsection/scalar'].attrs
	ss = np.linspace(a['low-0'], a['high-0'], a['shape-0'])
	T = np.linspace(a['low-1'], a['high-1'], a['shape-1'])
	Y = f[process+'/xsection/scalar/0'].value
	plt.plot(ss, Y, label=process, color=c)
#plt.semilogy()
plt.show()

for process,c in zip(['Qq2Qq', 'Qg2Qg', ], ['r','g','b']):
	a = f[process+'/rate/scalar'].attrs
	E = np.linspace(a['low-0'], a['high-0'], a['shape-0'])
	T = np.linspace(a['low-1'], a['high-1'], a['shape-1'])
	Y = f[process+'/rate/scalar/0'].value
	plt.plot(E, Y, label=process, color=c)
#plt.semilogy()
plt.show()

for process,c in zip(['Qq2Qq', 'Qg2Qg', ], ['r','g','b']):
	a = f[process+'/rate/vector'].attrs
	E = np.linspace(a['low-0'], a['high-0'], a['shape-0'])
	pz = (E**2-1.3**2)**0.5
	T = np.linspace(a['low-1'], a['high-1'], a['shape-1'])
	Y = f[process+'/rate/vector/3'].value
	plt.plot(E, -Y[:,10]/pz, label=process, color=c)
#plt.semilogy()
plt.show()

for process,c in zip(['Qq2Qq', 'Qg2Qg', ], ['r','g','b']):
	a = f[process+'/rate/scalar'].attrs
	E = np.linspace(a['low-0'], a['high-0'], a['shape-0'])
	T = np.linspace(a['low-1'], a['high-1'], a['shape-1'])
	R = f[process+'/rate/scalar/0'].value
	Rz = f[process+'/rate/vector/3'].value
	Rxx = f[process+'/rate/tensor/5'].value
	Rzz = f[process+'/rate/tensor/15'].value
	Rzz = Rzz - Rz**2/R
	plt.plot(E, Rxx, label=process, color=c)
#plt.semilogy()
plt.show()
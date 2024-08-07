# generate a 2D ld sequence of samples
# from:
#   https://extremelearning.com.au/unreasonable-effectiveness-of-quasirandom-sequences/

from math import *
import numpy as np

# Using the above nested radical formula for g=phi_d 
# or you could just hard-code it. 
# phi(1) = 1.6180339887498948482 
# phi(2) = 1.32471795724474602596 
def phi(d): 
  x=2.0000 
  for i in range(10): 
    x = pow(1+x,1/(d+1)) 
  return x

# Number of dimensions. 
d=2 

# number of required points 
n=32

g = phi(d)
alpha = np.zeros(d)
seed = 0.5 # any real number
for j in range(d):
  # 1 / g^1, 1 / g^2, 1 / g^3, ...
  alpha[j] = pow(1 / g, j + 1) % 1

u = np.zeros((n, d))

# This number can be any real number. 
# Common default setting is typically seed=0
# But seed = 0.5 might be marginally better. 
for i in range(n): 
  u[i] = (seed + alpha*(i+1)) % 1

print(u)

# generate gaussian samples too
z = np.zeros(u.shape)
for i, (u1, u2) in enumerate(u):
    z1 = sqrt(-2 * log(u1)) * cos(2 * pi * u2)
    z2 = sqrt(-2 * log(u1)) * sin(2 * pi * u2)
    z[i] = [z1, z2]

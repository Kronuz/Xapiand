from mpl_toolkits.mplot3d import Axes3D
from mpl_toolkits.mplot3d.art3d import Poly3DCollection
import matplotlib.pyplot as plt


ax = Axes3D(plt.figure())


# Original Points
x1 = 0.999998;
y1 = -0.00174533;
z1 = 0;
x = [0.999998];
y = [-0.00174533];
z = [0]
ax.plot3D(x, y, z, 'ro', lw = 2.0, ms = 6);
x = [0.99985];
y = [0];
z = [0.0173356]
ax.plot3D(x, y, z, 'ro', lw = 2.0, ms = 6);
x = [0.999998];
y = [0.00174533];
z = [0]
ax.plot3D(x, y, z, 'ro', lw = 2.0, ms = 6);
x = [0.99985];
y = [0];
z = [-0.0173356]
ax.plot3D(x, y, z, 'ro', lw = 2.0, ms = 6);
# Points for the hull convex
x = [0.999850, 0.999998, 0.999850, 0.999998, 0.99985];
y = [0.000000, 0.001745, 0.000000, -0.001745, 0];
z = [-0.017336, 0.000000, 0.017336, 0.000000, -0.0173356]
ax.plot3D(x, y, z, '-', lw = 2.0, ms = 12, mfc = 'white', mec = 'black');
ax.set_xlabel('x')
ax.set_ylabel('y')
ax.set_zlabel('z')
plt.show()
plt.ion()

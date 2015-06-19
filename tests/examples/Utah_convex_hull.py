from mpl_toolkits.mplot3d import Axes3D
from mpl_toolkits.mplot3d.art3d import Poly3DCollection
import matplotlib.pyplot as plt


ax = Axes3D(plt.figure())


# Original Points
x1 = -0.261268;
y1 = -0.75674;
z1 = 0.599236;
x = [-0.261268];
y = [-0.75674];
z = [0.599236]
ax.plot3D(x, y, z, 'ro', lw = 2.0, ms = 6);
x = [-0.326228];
y = [-0.731089];
z = [0.599236]
ax.plot3D(x, y, z, 'ro', lw = 2.0, ms = 6);
x = [-0.308427];
y = [-0.691195];
z = [0.653547]
ax.plot3D(x, y, z, 'ro', lw = 2.0, ms = 6);
x = [-0.303736];
y = [-0.680682];
z = [0.666646]
ax.plot3D(x, y, z, 'ro', lw = 2.0, ms = 6);
x = [-0.271367];
y = [-0.706567];
z = [0.653547]
ax.plot3D(x, y, z, 'ro', lw = 2.0, ms = 6);
x = [-0.26724];
y = [-0.695821];
z = [0.666646]
ax.plot3D(x, y, z, 'ro', lw = 2.0, ms = 6);
x = [-0.247011];
y = [-0.715445];
z = [0.653547]
ax.plot3D(x, y, z, 'ro', lw = 2.0, ms = 6);
# Points for the hull convex
x = [-0.247011, -0.267240, -0.303736, -0.326228, -0.261268, -0.247011];
y = [-0.715445, -0.695821, -0.680682, -0.731089, -0.756740, -0.715445];
z = [0.653547, 0.666646, 0.666646, 0.599236, 0.599236, 0.653547]
ax.plot3D(x, y, z, '-', lw = 2.0, ms = 12, mfc = 'white', mec = 'black');
ax.set_xlabel('x')
ax.set_ylabel('y')
ax.set_zlabel('z')
plt.show()
plt.ion()

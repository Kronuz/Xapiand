from mpl_toolkits.mplot3d import Axes3D
from mpl_toolkits.mplot3d.art3d import Poly3DCollection
import matplotlib.pyplot as plt


ax = Axes3D(plt.figure())


# Original Points
x1 = -0.167132;
y1 = -0.782932;
z1 = 0.599236;
x = [-0.167132];
y = [-0.782932];
z = [0.599236]
ax.plot3D(x, y, z, 'ro', lw = 2.0, ms = 6);
x = [-0.267895];
y = [-0.754419];
z = [0.599236]
ax.plot3D(x, y, z, 'ro', lw = 2.0, ms = 6);
x = [-0.253276];
y = [-0.713252];
z = [0.653547]
ax.plot3D(x, y, z, 'ro', lw = 2.0, ms = 6);
x = [-0.158012];
y = [-0.740209];
z = [0.653547]
ax.plot3D(x, y, z, 'ro', lw = 2.0, ms = 6);
# Points for the hull convex
x = [-0.158012, -0.253276, -0.267895, -0.167132, -0.158012];
y = [-0.740209, -0.713252, -0.754419, -0.782932, -0.740209];
z = [0.653547, 0.653547, 0.599236, 0.599236, 0.653547]
ax.plot3D(x, y, z, '-', lw = 2.0, ms = 12, mfc = 'white', mec = 'black');
ax.set_xlabel('x')
ax.set_ylabel('y')
ax.set_zlabel('z')
plt.show()
plt.ion()

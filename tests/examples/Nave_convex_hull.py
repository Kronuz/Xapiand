from mpl_toolkits.mplot3d import Axes3D
from mpl_toolkits.mplot3d.art3d import Poly3DCollection
import matplotlib.pyplot as plt


ax = Axes3D(plt.figure())


# Original Points
x1 = 0.982247;
y1 = -0.143294;
z1 = -0.121066;
x = [0.982247];
y = [-0.143294];
z = [-0.121066]
ax.plot3D(x, y, z, 'ro', lw = 2.0, ms = 6);
x = [0.988231];
y = [-0.138887];
z = [0.0641021]
ax.plot3D(x, y, z, 'ro', lw = 2.0, ms = 6);
x = [0.981077];
y = [-0.0686036];
z = [0.181056]
ax.plot3D(x, y, z, 'ro', lw = 2.0, ms = 6);
x = [0.986496];
y = [-0.0517001];
z = [0.155413]
ax.plot3D(x, y, z, 'ro', lw = 2.0, ms = 6);
x = [0.984406];
y = [-0.0343762];
z = [0.17252]
ax.plot3D(x, y, z, 'ro', lw = 2.0, ms = 6);
x = [0.997766];
y = [-0.0645226];
z = [-0.0173356]
ax.plot3D(x, y, z, 'ro', lw = 2.0, ms = 6);
x = [0.998008];
y = [0.0174203];
z = [0.0606414]
ax.plot3D(x, y, z, 'ro', lw = 2.0, ms = 6);
x = [0.988075];
y = [0.0951408];
z = [0.121066]
ax.plot3D(x, y, z, 'ro', lw = 2.0, ms = 6);
x = [0.99452];
y = [0.104528];
z = [0.00173364]
ax.plot3D(x, y, z, 'ro', lw = 2.0, ms = 6);
x = [0.997414];
y = [0.069746];
z = [-0.0173356]
ax.plot3D(x, y, z, 'ro', lw = 2.0, ms = 6);
x = [0.925603];
y = [0.336892];
z = [-0.17252]
ax.plot3D(x, y, z, 'ro', lw = 2.0, ms = 6);
x = [0.984091];
y = [0.0860968];
z = [-0.155413]
ax.plot3D(x, y, z, 'ro', lw = 2.0, ms = 6);
x = [0.985006];
y = [0];
z = [-0.17252]
ax.plot3D(x, y, z, 'ro', lw = 2.0, ms = 6);
# Points for the hull convex
x = [0.985006, 0.925603, 0.988075, 0.984406, 0.981077, 0.988231, 0.982247, 0.985006];
y = [0.000000, 0.336892, 0.095141, -0.034376, -0.068604, -0.138887, -0.143294, 0];
z = [-0.172520, -0.172520, 0.121066, 0.172520, 0.181056, 0.064102, -0.121066, -0.17252]
ax.plot3D(x, y, z, '-', lw = 2.0, ms = 12, mfc = 'white', mec = 'black');
ax.set_xlabel('x')
ax.set_ylabel('y')
ax.set_zlabel('z')
plt.show()
plt.ion()

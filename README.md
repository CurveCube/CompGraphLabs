# CompGraphLabs
The repository for laboratory work on computer graphics.
Team members:
- Daniil Baev
- Artur Fayzrakhmanov
- Grigory Chevykalov

Lab1:
Note: resource markers are displayed only in the debug build; they are removed from release build as well as other debugging information.

Lab2:
Note: rotate the camera while holding down the RIGHT mouse button (the left button is used by imgui);
the point the camera is looking at movement is performed using WSADQE (or arrows (WSAD) and RIGHT shift/ctrl (Q/E)) buttons; QE - movement along the y axis; WSAD - movement in the xz plane depending on the camera direction; the mouse wheel allows you to zoom in/out of the camera in the viewing direction (the zoom in is limited by a distance of 1 from the point the camera is looking at).

Lab6:
Note: it is assumed that the vertices are described by at least a position and a normal; sparse accessors are not supported; image loading is supported only for URIs.

Lab7:
Note: shadows are processed only for a directional light source; transparent objects are treated as having an alpha cutoff of 0.5 when generating shadows.
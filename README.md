# FWater - Visual Demo
This code uses the famous "two-buffer trick" to create a water-like visual effect. The idea is you have two buffers, which each store a heightmap. 
The pseudo-code is
* Every frame, for a given pixel P, compute the average of that pixel's neighbors in the other buffer. Invert that average. That (times some dampening factor) is P's new value. 
* Swap the roles of the buffers.

The concept is simple, yet it creates a rippling kind of effect due to the wave-like values that the heights will have and the way they oscillate up and down. The waves will bounce off "walls", too, depending on the implementation; this one allows that.

While a sensible implementation on modern computers will perform okay, this kind of technique is best suited to an implementation that can do many computations in parallel (e.g., a compute shader). Allegedly, the ifort compiler is supposed to help out here. Anecdotally, the performance has been good enough on this sample. It could be interesting to stress test it more.


## Example

![Example image](https://raw.githubusercontent.com/clandrew/fwater/master/Images/Effect.gif "Example image.")

Use the mouse to cause motion in the heightmap.

## Build
Visual Studio 2019 with Intel Parallel Studio XE was used to build this.

This program is organized as a Visual Studio solution with two projects.
* Window - A Win32 executible written in C++, that has a window and a Direct3D12-compatible swap chain.
* Compute - A DLL written in Fortran (2008-compatible) that implements the algorithm described above.

The setup has Window responsible for all the UI and user-facing elements; window, mouse input, etc, while the "heavy lifting" happens in Compute. 

There's an entrypoint in Compute which draws to a Direct3D surface.

## Related Links

[The Water Effect Explained](https://www.gamedev.net/articles/programming/graphics/the-water-effect-explained-r915)

[Coding Requirements for Sharing Procedures in DLLs](https://software.intel.com/en-us/node/535306)

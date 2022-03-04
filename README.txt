List of all files you submitted:

matrix4.h
asst3.cpp

Note the platform you used for development (Windows, OS X, ...):

Windows

Provide instructions on how to compile and run your code, especially if you used a nonstandard Makefile, or you are one of those hackers who insists on doing things differently.

Open on Visual Studio. Switch to solution view (not folder view). Press the green start arrow (debugging button).

Indicate if you met all problem set requirements (more importantly, let us know where your bugs are and what you did to try to eliminate the bugs; we want to give you as much partial credit as we can).

I've met all of the requirements. There are no bugs.

Provide some overview of the code design. Don't go into details; just give us the big picture.

1. Made second cube: g_ObjectRbt[2]. Drew that cube in drawStuff().
2. Implemented transFact and linFact per the definitions in lecture
3. Defined 3 new global variables: currentView, currentObj, and currentMode
4. Added a few if-statements on how the number assigned to currentView (0, 1, or 2) changes what the eyeRbt is assigned to. Added that the key 'v' is connected to that.
5. If the currentView is NOT the sky, don't do anything. Otherwise, I used lecture notes to define different A for the O = A * M * A^(-1) * O depending on what the object (O) is.
6. If the object is sky, then we need to define mx and my as seperate rotation matrices in the two diff directions, then A is computed from there.
7. The translation/rotation matrices may have inverted signs for dx and dy if the active object is the sky. I added that too.

Let us know how to run the program; what are the hot keys, mouse button usage, and so on? Describe steps or sequences of steps the TF should take to test and evaluate your code (especially if your implmenentation strays from the assignment specification).

The same as the assignment speciation/staff solution. Use 'v', 'm' and 'o' alongside left mouseclick, right mouseclick and left mouseclick + space bar.

Finally, did you implement anything above and beyond the problem set? If so, document it in order for the TFs to test it and evaluate it.

Nope.

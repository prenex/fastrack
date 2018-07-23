# fastrack
A fast tracking algorithm with minimal dependencies using c++14 and possibly supporting a wide range of architectures.

Why?
====

* Because I kind of dislike that I have to set up opencv and all the stuff just for tracking the simplest markers
* Because it seems that all trackers start with a lot of processing phases and I want to experiment how far I can get with a single scan over the whole frame
* Because I had some simple state-machine-based and also a simple color-based-tracking idea that utilizes proper parenthesing using specified chroma-filtered colors.

How?
====

0. The algorithm should work with simple calls providing pixel (magnitude) data. It should enable exchanging underlying algorithms using template parameters.
1. I plan to find a simple way to get camera frames on linux so that I can experiment with my webcam. Input however should be independent from the algorithm, just I will try to use native YUYV color spaces.
2. I plan to print pictures with "circle-like" cylinders that have 4 colors or other indications that are easy to parse with state and stack machines.
3. A marker is at least two cilinders of these colored things in the same rotation - in the colored version. Otherwise always has a simmetry: to be able to get parsed from any angles.
4. So because it is a circle at least one scanline will pass through the opposite (colored / marked) sides creating a readable pattern we search for (opening colors and closing colors or open/close strips).
5. I will try to find all and every data for telling which colors are the best for this. First I thought about purple but now I think again after reading about why chroma-keying is usually a green box. We need at least 2, optimally at least 4 colors though. If that fails I will try to look for non-color based algorithms that share the same properties to an usable level.
6. When in a scanline, we will collect each and every pattern that we look for (like a big vector on the right side of the picture vertically telling where the centers of these are). To support more than one marker, we could enlarge this data structure to be able to hold multiple found elements for each scanline (for example 64-512). 
7. WHILE we are processing all scanlines, we run through this vertical "vector" and find out the horizontal centers of the found markers. The x coordinates are already well calculated so what we need to do is to "unify" these vertically with a reasonably small delta value. We could have done this after all scanlines completed, but we are using a completely "online" algorithm: an always updated "fast-forward-list" is holding all currently known "Marker Centers" for the current scanline. We can expand or close these down and collect results from closed marker center suspicions that match our patterns.
8. The y coordinate is obtained by the middle index of the above unification or the horizontal marker centers so we get 2D marker coordinates and meta-data about precision etc.
9. Thus we get all (x,y) pairs for all these circular markers in the current camera frame and all the parts of the algorithms are completely "online" through processing the frame!

I think I should provide ways to only stop here and let the end-user process this data as they want:

* If you put a lot of markers like this on a big wall and all you want is to render a wireframe of it (with no 3D calculations at all actually) then just use this 2D data and do so!
* If you want pose estimation, then create 3-4 of these circle markers on a paper and let us track them for you. You can get camera pose estimates by triangulating (for example you print 4 circular markes to four corners of a rectangle you know). There will be example application for this.
* If you have heavy lens distortion, you can take care of that by known methods - however for 2D marking purposes maybe you do not need that at all.
* If you want to do clever openCV stuff you can do it, but you are not necessitated to link in whole big libraries like that.
* Put 4 of the markers around your TV screen and point a camera towards it: without 3D calculations it is easy to get a mouse coordinate for emulating a "light-gun".
* Put a lot of markers in an indoor area for precise indoor positioning / spatial mapping using pnp (perspective n-point) algorithms.

Will it work?
=============

It is a very good question. This algorithm family is just a weird idea so it might or might not work. If it would work it might be awsome for you because of its simplicity. From now on I expect this will get finished to some degree as I will give this to my company who has similar research interests. This is still mostly implemented in my spare time, but the algorithms might be used for specific purposes. The general core will get backports here, but anything Hololens-specific stuff will likely go into private git repositories of my company and not shared here.

When you start to see visible results, then you might be happy to use this in your products if you tell who made it possible. Still... I do not advise using any of this stuff in production code ;-)

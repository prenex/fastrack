# fastrack
A fast tracking algorithm with minimal dependencies using c++14 and possibly supporting a wide range of architectures.

Why?
====

* Because I kind of dislike that I have to set up opencv and all the stuff just for tracking the simplest markers
* Because it seems that all trackers start with a lot of processing phases and I want to experiment how far I can get with a single scan over the whole frame
* Because I have some color-based-tracking idea that utilizes proper parenthesing using specified chroma-filtered colors.

How?
====

1.) I plan to find a simple way to get camera frames on linux so that I can experiment with my webcam. Input however should be independent from the algorithm, just I will try to use YUYV.
2.) I plan to print pictures with "circle-like" cylinders that have 4 colors - each in its opposite side different, creating 4 quarters.
3.) A marker is at least two cilinders of these colored things in the same rotation.
4.) Because it is a circle at least one scanline will pass through the opposite colored sides creating a readable pattern we search for (two opening colors and two closing colors).
5.) I will try to find all and every data for telling which colors are the best for this. First I thought about purple but now I think again after reading about why chroma-keying is usually a green box. We need at least 2, optimally at least 4 colors though..
6.) When in a scanline, we will collect each and every pattern that we look for (like a big vector on the right side of the picture vertically telling where the centers of these are). To support more than one marker, we could enlarge this data structure to be able to hold multiple found elements for each scanline (for example 64).
7.) After processing all scanlines, we run through this vertical "vector" and find out the horizontal centers of the found markers. The x coordinates are already well calculated so what we need to do is to "unify" these vertically with a reasonably small delta value.
8.) The y coordinate is obtained by the middle index of the above unification
9.) Thus we get all (x,y) pairs for all these circular markers in the current camera frame.

I think I should provide ways to only stop here and let the end-user process this data as they want:
* If you put a lot of markers like this on a big wall and all you want is to render a wireframe of it (with no 3D calculations at all actually) then just use this 2D data and do so!
* If you want pose estimation, then create 3-4 of these circle markers on a paper and let us track them for you. You can get camera pose estimates by triangulating (for example you print 4 circular markes to four corners of a rectangle you know).
* If you have heavy lens distortion, you can take care of that by known methods - however for 2D marking purposes maybe you do not need that at all.
* If you want to do clever openCV stuff you can do it, but you are not necessitated to link in whole big libraries like that...

What if I screw up?
===================

It is a very good question. This algorithm is just a weird idea so it might or might not work. If it would work it might be awsome for you because of its simplicity, but who knows if I just screw up or become bored to work on this.
If you start to see visible results, then you might be happy to use this in your products if you tell who made it possible. Until then I do not advise using any of these in production code ;-)

/****
**  reductionTesting2D
**
** 	Code written by Rahul Jain.
** 	If you have any questions regarding this code, please email me at jain10@illinois.edu
**
****/

This program tests the section reduction functionality for array sections in Charm++ and smp.


/*******
**
** How to run this program
**
*******/
The program takes in three parameters, in the following order:
arrayDimesionX, arrayDimensionY and vectorSize respectively.

arrayDimesionX -- x dimension of the 2D chare array.
arrayDimesionY -- y dimension of the 2D chare array.
vectorSize -- Each element of the 2D chare array, has a vector of doubles over which reduction is performed.
			  This variable defines the size of the vector. Each vector element is initialized to its index value.
reductionTesting2D  -- executable

So, for example, if you want to create a 20x10 chare array with a vector size of 5, you could do the following:
	%./charmrun +n4 +ppn8 ./reductionTesting2D 20 10 5





********
???????/
???
??? What this program does ?
???
??????/
********

Here's a control flow of this program:

Input from user(arrayDimensions X and Y, vectorSize)
				|
				|
				V
Create an 2D chare array (Test2D) of the given dimension.
Each chare element has a vector of size vectorSize, each element of which is
initialized to it's (the element's) index.
				|
				|
				V
Create an array of Section Proxies.
//Random things that I decided, in order create the section proxies.
Number of array section proxies that will be created = max(arrayDimesionX, arrayDimensionY) = N.
				|
				|
				V
Each section proxy does a Multicast (calls Test2D::compute(msg)).
A message is sent to the compute entry, which essentially helps to identify which sectoion Proxy
are we dealing with, on the receiving end.
				|
				|
				V
The compute entry, essentially reduces the vector elements and callbacks to Main::reportSum.


#include "pch.h"
#include <iostream>
#include <opencv2/opencv.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <math.h>       

using namespace std;
using namespace cv;
using namespace std::this_thread;
using namespace std::chrono_literals;

// prototype functions
Mat mapBasePixels(int scale, Mat imageIn, Mat imageOut);
int linInterp(int scale, int position, int leftValue, int rightValue);
int cubicInterp(int scale, int position, int farLeftValue, int leftValue, int rightValue, int farRightValue);

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////Transform Functions////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//Scale image without interpolation, leaving blank space between pixels. 
Mat simpleScale(int scale, Mat image) {
	Mat scaledImage(Size(image.cols*scale, image.rows*scale), CV_8UC3);

	scaledImage = mapBasePixels(scale, image, scaledImage);

	return scaledImage;
}
//Scale image via linear interpolating between known pixels
Mat linearScale(int scale, Mat image) {
	Mat scaledImage(Size(image.cols*scale, image.rows*scale), CV_8UC3); // Create scaled image to output

	unsigned char * q0 = image.ptr(0, 0);	// Create pointers to be used for interpolation. q0 is the known point "behind" the pixel position
	unsigned char * q1 = image.ptr(0, 0);	// q1 is the known point "in front" of the pixel position
	unsigned char * p;						// Create pointer for pixels in the new image. 

	scaledImage = mapBasePixels(scale, image, scaledImage);

	//Logic to construct vertical nodal lines via linear interpolation
	for (int x = 0; x < scaledImage.cols; x++) {			// Cycle through rows of NEW image
		for (int y = 0; y < scaledImage.rows - scale; y++) {  // Cycle through columns of NEW image
			if (x%scale == 0 and y%scale == 0) {		// If we hit a "node", aka a pixel that maps directly to new image from original
				q0 = scaledImage.ptr(y, x);	// "Back" pnt = current pnt
				q1 = scaledImage.ptr(y + scale, x);	// "Forward" pt = next pnt
			}
			if (x%scale == 0 and y%scale != 0) {	// If we land on a vertical "node line"
				p = scaledImage.ptr(y, x);				// Direct pointer to current position in output image
				for (int i = 0; i < 3; i++) {
					p[i] = linInterp(scale, y, q0[i], q1[i]);	//Linearly interpolate between points above and below
				}
			}
		}
	}

	q0 = image.ptr(0, 0); // Reset back and forward pointers, just in case
	q1 = image.ptr(0, 0);

	//Logic to fill in horizontal points between vertical nodal lines via linear interpolation
	for (int y = 0; y < scaledImage.rows; y++) {			// Cycle through vertical values of scaled image
		for (int x = 0; x < scaledImage.cols - scale; x++) {  // Cycle through horizontal values of scaled imag
			if (x%scale == 0) {						// If along a vertical node line
				q0 = scaledImage.ptr(y, x);			// Left point = current point
				q1 = scaledImage.ptr(y, x + scale);			// Right point = point on next nodal line
			}
			else if (x%scale != 0) {					// If not on a node line
				p = scaledImage.ptr(y, x);			// Pick out current points
				for (int i = 0; i < 3; i++) {
					p[i] = linInterp(scale, x, q0[i], q1[i]);	//Linearly interpolate between points above and below
				}
			}
		}
	}
	return scaledImage; // Return the output image
}
//Scale image via cubic interpolating between known pixels
Mat cubicScale(int scale, Mat image) {
	Mat scaledImage(Size(image.cols*scale, image.rows*scale), CV_8UC3); // Create scaled image to output

	unsigned char * q0 = image.ptr(0, 0);	// Create pointers to be used for interpolation. q0 is the known pnt 2 "behind" the pixel position
	unsigned char * q1 = image.ptr(0, 0);	// q1 is the known pnt "behind" the pixel position
	unsigned char * q2 = image.ptr(0, 0);	// q2 is the known point "in front" of the pixel position
	unsigned char * q3 = image.ptr(0, 0);	// q3 is the known pnt 2 "in front" of the pixel position
	unsigned char * p;						// Create pointer for pixels in the new image. 

	scaledImage = mapBasePixels(scale, image, scaledImage);

	//Logic to construct vertical nodal lines via bicubic interpolation
	for (int x = scale; x < scaledImage.cols - scale; x++) {						// Cycle through rows of NEW image
		for (int y = scale; y < scaledImage.rows - 2 * scale; y++) {		// Cycle through columns of NEW image
			if (x%scale == 0 and y%scale == 0) {						// If we hit a "node", aka a pixel that maps directly to new image from original
				q0 = scaledImage.ptr(y - scale, x);						// "Back 2" pnt = behind current pnt
				q1 = scaledImage.ptr(y, x);				// "Back" pt = 1 ahead of current pnt
				q2 = scaledImage.ptr(y + scale, x);			// "Forward" pt = 2 ahead of current pnt
				q3 = scaledImage.ptr(y + 2 * scale, x);			// "Forward 2" pt = 3 ahead of current pnt
			}
			else if (x%scale == 0 and y%scale != 0) {	// If we land on a vertical "node line"
				p = scaledImage.ptr(y, x);				// Direct pointer to current position in output image
				for (int i = 0; i < 3; i++) {
					p[i] = cubicInterp(scale, y, q0[i], q1[i], q2[i], q3[i]);
				}
			}
		}
	}

	q0 = q1 = q2 = q3 = image.ptr(0, 0);	// Create pointers to be used for interpolation. q0 is the known pnt 2 "behind" the pixel position

	//Logic to fill in horizontal points between vertical nodal lines via linear interpolation
	for (int y = scale; y < scaledImage.rows; y++) {
		for (int x = scale; x < scaledImage.cols - 2 * scale; x++) {
			if (x%scale == 0) {								// If along a vertical node line
				q0 = scaledImage.ptr(y, x - scale);			// "Back 2" pnt = behind current pnt
				q1 = scaledImage.ptr(y, x);					// "Back" pt = 1 ahead of current pnt
				q2 = scaledImage.ptr(y, x + scale);			// "Forward" pt = 2 ahead of current pnt
				q3 = scaledImage.ptr(y, x + 2 * scale);		// "Forward 2" pt = 3 ahead of current pnt
			}
			else if (x%scale != 0) {					// If not on a node line
				p = scaledImage.ptr(y, x);			// Pick out current points
				for (int i = 0; i < 3; i++) {
					p[i] = cubicInterp(scale, x, q0[i], q1[i], q2[i], q3[i]);
				}
			}
		}
	}
	return scaledImage; // Return the output image
}
//Rotate Image 90 degrees clockwise
Mat rotate90CW(Mat imageIn) {
	Mat imageOut(Size(imageIn.rows, imageIn.cols), CV_8UC3);

	int w = imageOut.cols - 1;

	unsigned char * q;
	unsigned char * p;

	for (int y = 0; y < imageIn.rows; y++) {
		for (int x = 0; x < imageIn.cols; x++) {
			q = imageIn.ptr(y, x);
			p = imageOut.ptr(x, w - y);
			p[0] = q[0];   // B
			p[1] = q[1];   // G
			p[2] = q[2];   // R


		}
	}
	return imageOut;
}
//Rotat Image 90 degrees counter clockwise
Mat rotate90CCW(Mat imageIn) {
	Mat imageOut(Size(imageIn.rows, imageIn.cols), CV_8UC3);

	int w = imageOut.rows - 1;

	unsigned char * q;
	unsigned char * p;

	for (int y = 0; y < imageIn.rows; y++) {
		for (int x = 0; x < imageIn.cols; x++) {
			q = imageIn.ptr(y, x);
			p = imageOut.ptr(w - x, y);
			p[0] = q[0];   // B
			p[1] = q[1];   // G
			p[2] = q[2];   // R


		}
	}

	return imageOut;
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////Supporting Functions///////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Map based pixels to new positions
Mat mapBasePixels(int scale, Mat imageIn, Mat imageOut) {

	unsigned char * q;
	unsigned char * p;

	for (int x = 0; x < imageIn.cols; x++) {
		for (int y = 0; y < imageIn.rows; y++) {
			q = imageIn.ptr(y, x);
			p = imageOut.ptr(y*scale, x*scale);
			p[0] = q[0];   // B
			p[1] = q[1];   // G
			p[2] = q[2];   // R
		}
	}
	return imageOut;
}
// Linearly interpolate between two points
int linInterp(int scale, int position, int leftValue, int rightValue) {
	int pixelValue;
	pixelValue = ((scale - position % scale)*leftValue + (position % scale) * rightValue) / scale;

	return pixelValue;
}
// Construct a cubic spline to interpolate between points
int cubicInterp(int scale, int position, int farLeftValue, int leftValue, int rightValue, int farRightValue) {
	int value;

	float posFlt = position % scale;
	value = leftValue + 0.5 * posFlt / scale * (rightValue - farLeftValue + posFlt / scale * (2.0*farLeftValue - 5.0*leftValue + 4.0*rightValue - farRightValue + posFlt / scale * (3.0*(leftValue - rightValue) + farRightValue - farLeftValue)));	//Linearly interpolate between points in front and behind
	if (value > 255) {
		value = 255;
	}
	else if (value < 0) {
		value = 0;
	}
	return value;
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////UI Functions////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int main()
{
	int scale;
	int selection;
	char input;
	bool keepGoin = true;

	Mat image;										// Container for input images
	Mat imageTransformed;							// Container for transformed images
	Mat dude = imread("dude.png");					// Load simple black and white "dude" image
	Mat lady = imread("lady.png");					// Load a more complex black and white "lady" image
	Mat dog = imread("dog.png");					// Load a colorful pup
	Mat cat = imread("cat.png");					// Load cat photo
	Mat bird = imread("bird.png");					// Load bird photo
	Mat imageBank[6] = { imageTransformed, dude, lady, dog, cat, bird };


	cout << "Select image..." << endl;
	cout << "(1) Dude" << endl;
	cout << "(2) Lady" << endl;
	std::cout << "(3) Dog" << endl;
	std::cout << "(4) Cat" << endl;
	std::cout << "(5) Bird" << endl;
	cin >> selection;

	image = imageBank[selection];


	while (keepGoin == true) {
		cout << "Select transform..." << endl;
		cout << "(1) Sparse" << endl;
		cout << "(2) Linear" << endl;
		cout << "(3) Cubic" << endl;
		cout << "(4) Rotate Clockwise" << endl;
		cout << "(5) Rotate Counter-Clockwise" << endl;
		cin >> selection;

		if (selection == 1 || selection == 2 || selection == 3) {
			cout << "Scale Factor?" << endl;
			cin >> scale;
		}


		if (selection == 1) {
			imageTransformed = simpleScale(scale, image);
		}
		else if (selection == 2) {
			imageTransformed = linearScale(scale, image);
		}
		else if (selection == 3) {
			imageTransformed = cubicScale(scale, image);
		}
		else if (selection == 4) {
			imageTransformed = rotate90CW(image);
		}
		else if (selection == 5) {
			imageTransformed = rotate90CCW(image);
		}

		imageBank[0] = imageTransformed;
		imshow("Input Image", image);
		imshow("Output Image", imageTransformed);

		cout << "Press any key to close and continue..." << endl;
		waitKey(0);
		destroyAllWindows();
		cout << "More transforms? (y/n)" << endl;
		cin >> input;
		if (input == 'n') {
			keepGoin = false;
		}
		else {
			cout << "Select image..." << endl;
			cout << "(0) Previous image" << endl;
			cout << "(1) Dude" << endl;
			cout << "(2) Lady" << endl;
			cout << "(3) Dog" << endl;
			cout << "(4) Cat" << endl;
			cout << "(5) Bird" << endl;
			cin >> selection;
		}
		image = imageBank[selection];
	}
	waitKey(0);
	return 0;
}
#include <assert.h>
#define CAMERACTRL_CPP
#include "cameractrl.h"

// ----------------------------------------------------------------------------
CameraCtrl::CameraCtrl( cv::Mat * img, SetExposureType SetExposure, SetGainType SetGain, SetDiodesType SetDiodes, bool debugOn )
{
	this->img			= img;
	this->SetExposure	= SetExposure;
	this->SetGain		= SetGain;
	this->SetDiodes		= SetDiodes;
	this->debugOn		= debugOn;

	KI					= 0.4;
	KP					= 0.1;

	histeresis			= 30;
	firstRun			= false;

	intError			= 0;
}
// ----------------------------------------------------------------------------
float CameraCtrl::PI( float error )
{

	/* previous version
	if( abs( error ) < histeresis )	
		error = 0;
	else
	{
		if( error > 0 ) error -= histeresis;
		else			error += histeresis;
	}*/

	if( error > histeresis )	
		error -= histeresis;
	else if( error > 0 )
		error  = 0.05;
	else if( error > -histeresis )
		error  = -0.05;
	else
		error += histeresis;

	float out	= KP*error + KI*intError;

	intError += error;
	if( intError > MAX_PI_ERROR/KI )
		intError = MAX_PI_ERROR/KI;

	if( intError < MIN_PI_ERROR/KI )
		intError = MIN_PI_ERROR/KI;

	if( out > MAX_PI_ERROR )
		out = MAX_PI_ERROR;

	if( out < MIN_PI_ERROR )
		out = MIN_PI_ERROR;

	if( debugOn )
		printf( "error=%3.1f, intError=%3.1f, out=%3.1f     \r", error, intError, out );
	return out;
}
// ----------------------------------------------------------------------------
int CameraCtrl::CalculateAverage( int x1, int y1, int x2, int y2 )
{
	///todo
	//char * ptr1 = img->imageData + y1*img->widthStep + x1;
	uchar * ptr1 = img->ptr() + y1*img->cols + x1;
	int avg		= 0;

	// check correctness of data
	assert( x2 > x1 && y2 > y1 );

	int width	= x2 - x1;
	int height	= y2 - y1;

	for( int h = 0; h < height; h++ )
	{
		uchar * ptr2 = ptr1;
		for( int w = 0; w < width; w++ )
			avg += (*ptr2++)&0x0FF;
		///todo
		//ptr1 += img->widthStep;
		ptr1 += img->cols;
	}

	return avg / (width * height );
}
// ----------------------------------------------------------------------------
void CameraCtrl::DebugRectangle( int xC, int yC, int width, int height, int value )
{
		//cvInitFont(&font, CV_FONT_HERSHEY_SIMPLEX, 1, 1 );
	char str[ 20 ];

	cv::rectangle(	*img, 
					cv::Point(xC-width/2,yC-height/2), 
					cv::Point(xC+width/2,yC+height/2),
					CV_RGB( 255, 255, 255 ));	
					
	sprintf( str, "%d", value );
	cv::putText( *img, str, cv::Point( xC-20, yC ), CV_FONT_HERSHEY_SIMPLEX, 1, CV_RGB( 255, 255, 255 ) );
}
// ----------------------------------------------------------------------------
void CameraCtrl::DebugInfo( float avg, float exposure, float gain, float diodes )
{
	//CvFont font;
	//cvInitFont(&font, CV_FONT_HERSHEY_SIMPLEX, 1, 1 );
	char str[ 100 ];

	sprintf( str, "A=%3.1f, E=%3.1f, G=%3.1f, D=%3.1f", avg, exposure, gain, diodes );
	printf( "A=%3.1f, E=%3.1f, G=%3.1f, D=%3.1f                           \n", avg, exposure, gain, diodes );
	cv::putText( *img, str, cv::Point( 0, img->rows - 20 ), CV_FONT_HERSHEY_SIMPLEX, 1, CV_RGB( 255, 255, 255 ) );

}
// ----------------------------------------------------------------------------
void CameraCtrl::Update()
{
	static float out;
	static float exposure;
	static float gain;
	static float diodes;
	static float t = 1;
	static float th = 1;
	
	// centers of ROIs
	const int XC			= img->cols / 2;
	const int YC			= img->rows / 2;

	const int XL			= XC - ROI_WIDTH;
	const int YL			= YC;

	const int XR			= XC + ROI_WIDTH;
	const int YR			= YC;

	const int XU			= XC;
	const int YU			= YC - ROI_HEIGHT;

	const int XD			= XC;
	const int YD			= YC + ROI_HEIGHT;

	// additional 4 ROIs
	const int XLU			= XC - ROI_WIDTH;
	const int YLU			= YC - ROI_HEIGHT;

	const int XRU			= XC + ROI_WIDTH;
	const int YRU			= YC - ROI_HEIGHT;

	const int XLD			= XC - ROI_WIDTH;
	const int YLD			= YC + ROI_HEIGHT;

	const int XRD			= XC + ROI_WIDTH;
	const int YRD			= YC + ROI_HEIGHT;


	const int WC			= 4;
	const int WL			= 2;
	const int WR			= 2;
	const int WD			= 2;
	const int WU			= 2;

	const int WLU			= 1;
	const int WRU			= 1;
	const int WLD			= 1;
	const int WRD			= 1;

	// calculate averages
	int avgC		= CalculateAverage( XC - ROI_WIDTH/2,  YC - ROI_HEIGHT/2,  XC + ROI_WIDTH / 2,  YC + ROI_HEIGHT / 2 );
	int avgL		= CalculateAverage( XL - ROI_WIDTH/2,  YL - ROI_HEIGHT/2,  XL + ROI_WIDTH / 2,  YL + ROI_HEIGHT / 2 );
	int avgR		= CalculateAverage( XR - ROI_WIDTH/2,  YR - ROI_HEIGHT/2,  XR + ROI_WIDTH / 2,  YR + ROI_HEIGHT / 2 );
	int avgD		= CalculateAverage( XD - ROI_WIDTH/2,  YD - ROI_HEIGHT/2,  XD + ROI_WIDTH / 2,  YD + ROI_HEIGHT / 2 );
	int avgU		= CalculateAverage( XU - ROI_WIDTH/2,  YU - ROI_HEIGHT/2,  XU + ROI_WIDTH / 2,  YU + ROI_HEIGHT / 2 );
	
	int avgLU		= CalculateAverage( XLU - ROI_WIDTH/2, YLU - ROI_HEIGHT/2, XLU + ROI_WIDTH / 2, YLU + ROI_HEIGHT / 2 );
	int avgRU		= CalculateAverage( XRU - ROI_WIDTH/2, YRU - ROI_HEIGHT/2, XRU + ROI_WIDTH / 2, YRU + ROI_HEIGHT / 2 );
	int avgLD		= CalculateAverage( XLD - ROI_WIDTH/2, YLD - ROI_HEIGHT/2, XLD + ROI_WIDTH / 2, YLD + ROI_HEIGHT / 2 );
	int avgRD		= CalculateAverage( XRD - ROI_WIDTH/2, YRD - ROI_HEIGHT/2, XRD + ROI_WIDTH / 2, YRD + ROI_HEIGHT / 2 );

	// divide by weights
	static float avgPrev;
	float avg	 = WC*avgC   + WL*avgL   + WR*avgR   + WD*avgD + WU*avgU;
	avg			+= WLU*avgLU + WRU*avgRU + WLD*avgLD + WRD*avgRD;
	avg			/= (WC + WL + WR + WD + WU + WLU + WRU + WLD + WRD );

	if( firstRun )
	{
		avgPrev = avg;
		firstRun = 0;
	}

	

	if( abs( avgPrev - avg ) > 70 )
	{
		printf( "Oscillation detected !!! \n" );
		t  = 0;
		th = 0;
	}

	t += 0.005;
	if( t > 1 ) t = 1;

	th += 0.002;
	if( th > 1 ) th = 1;

	const float KIOld = 0.075;
	const float KPOld = 0.05;
	const float KINew = KIOld / 100;
	const float KPNew = KPOld / 100;

	//KI = -KINew * t*(t-1)/0.25 + KIOld*(t-0.5)*(t-0.5)/0.25;
	//KP = -KPNew * t*(t-1)/0.25 + KPOld*(t-0.5)*(t-0.5)/0.25;
	KI = (1-t)*KINew + t*KIOld;
	KP = (1-t)*KPNew + t*KPOld;
	//printf( "t=%1.4f, th=%1.4f\n", t, th);

	const float histeresisOld = 20;
	const float histeresisNew = 40;//95;

	histeresis = (th < 1)*histeresisNew + (th>=1)*histeresisOld;
	


	avgPrev = avg;
	
	// ----- calculate PI ------------------------------------


	out = PI( SET_POINT - avg );
	
	if( out > 300 ) 
		out = 300;

	if( out > 200 )
	{
		gain     = 100;//out - 200;
		diodes   = 0;//out - 200;
		exposure = 100;
	}
	else if( out > 100 )
	{
		gain     = out - 100;
		diodes   = 0;
		exposure = 100;
	}
	else
	{
		gain      = 0;
		diodes    = 0;
		exposure  = out;
	}
	

/*
	if( out > 200 ) out = 200;
	if( out > 100 )
	{
		exposure	= 100;
		gain		= out - 100;
	}
	else
	{
		exposure	= out;
		gain		= 0;
	}
*/


	SetExposure( exposure );
	SetGain( gain );
	SetDiodes( diodes);

	// ----- print debug information -------------------------
	if( debugOn )
	{
		// debug info
		DebugInfo( avg, exposure, gain, diodes );

		// debug rectangles
		DebugRectangle( XC, YC, ROI_WIDTH, ROI_HEIGHT, avgC );
		DebugRectangle( XL, YL, ROI_WIDTH, ROI_HEIGHT, avgL );
		DebugRectangle( XR, YR, ROI_WIDTH, ROI_HEIGHT, avgR );
		DebugRectangle( XU, YU, ROI_WIDTH, ROI_HEIGHT, avgU );
		DebugRectangle( XD, YD, ROI_WIDTH, ROI_HEIGHT, avgD );
		// ----
		DebugRectangle( XLU, YLU, ROI_WIDTH, ROI_HEIGHT, avgLU );
		DebugRectangle( XRU, YRU, ROI_WIDTH, ROI_HEIGHT, avgRU );
		DebugRectangle( XLD, YLD, ROI_WIDTH, ROI_HEIGHT, avgLD );
		DebugRectangle( XRD, YRD, ROI_WIDTH, ROI_HEIGHT, avgRD );
	}
}

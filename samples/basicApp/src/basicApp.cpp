/*
 Copyright (C) 2012 Gabor Papp

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 3 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

#include "cinder/app/AppBasic.h"
#include "cinder/Camera.h"
#include "cinder/Cinder.h"
#include "cinder/gl/gl.h"
#include "cinder/params/Params.h"

#include "ciFaceshift.h"

using namespace ci;
using namespace ci::app;
using namespace std;

class basicApp : public AppBasic
{
	public:
		void prepareSettings( Settings *settings );
		void setup();

		void update();
		void draw();

	private:
		params::InterfaceGl mParams;

		double mTimestamp;
		bool mTrackingSuccessful;
		Quatf mHeadRotation;
		Vec3f mHeadPosition;
		Quatf mLeftEyeRotation;
		Quatf mRightEyeRotation;

		mndl::faceshift::ciFaceShift mFaceShift;
};

void basicApp::prepareSettings( Settings *settings )
{
	settings->setWindowSize( 800, 600 );
}

void basicApp::setup()
{
	gl::disableVerticalSync();

	mParams = params::InterfaceGl( "Parameters", Vec2i( 350, 550 ) );
	mParams.addParam( "Timestamp", &mTimestamp, "", true );
	mParams.addParam( "Tracking", &mTrackingSuccessful, "true='successful' false='failed'", true );
	mParams.addParam( "Rotation", &mHeadRotation, "", true );
	mParams.addParam( "Position", &mHeadPosition, "", true );
	mParams.addParam( "Left eye rotation", &mLeftEyeRotation, "", true );
	mParams.addParam( "Right eye rotation", &mRightEyeRotation, "", true );

	const vector< float >& weights = mFaceShift.getBlendshapeWeights();
	for ( size_t i = 0; i < weights.size(); ++i )
	{
		mParams.addParam( mFaceShift.getBlendshapeName( i ),
				const_cast< float * >( &weights[ i ] ), "group=Blendshapes", true );
	}

	mParams.setOptions( "", "refresh=.1" );

	mFaceShift.connect();
}

void basicApp::update()
{
	mTimestamp = mFaceShift.getTimestamp();
	mTrackingSuccessful = mFaceShift.isTrackingSuccessful();
	mHeadPosition = mFaceShift.getPosition();
	mHeadPosition *= .001; // millimetres to metres
	mHeadRotation = mFaceShift.getRotation();
	mLeftEyeRotation = mFaceShift.getLeftEyeRotation();
	mRightEyeRotation = mFaceShift.getRightEyeRotation();
}

void basicApp::draw()
{
	gl::clear( Color::black() );

	CameraPersp cam( getWindowWidth(), getWindowHeight(), 60.0f );
	cam.setPerspective( 60, getWindowAspectRatio(), 1, 1000 );
	cam.lookAt( Vec3f( 0, 0, -5 ), Vec3f::zero() );
	gl::setMatrices( cam );

	gl::setViewport( getWindowBounds() );

	gl::pushModelView();
	gl::translate( mHeadPosition );
	gl::rotate( mHeadRotation );
	gl::drawCoordinateFrame();
	gl::popModelView();

	params::InterfaceGl::draw();
}

CINDER_APP_BASIC( basicApp, RendererGl )


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
#include "cinder/TriMesh.h"

#include "ciFaceshift.h"

using namespace ci;
using namespace ci::app;
using namespace std;

class blendApp : public AppBasic
{
	public:
		void prepareSettings( Settings *settings );
		void setup();

		void update();
		void draw();

	private:
		Quatf mHeadRotation;
		Quatf mLeftEyeRotation;
		Quatf mRightEyeRotation;
		TriMesh mHeadMesh;

		mndl::faceshift::ciFaceShift mFaceShift;

		params::InterfaceGl mParams;
		float mFps;
};

void blendApp::prepareSettings( Settings *settings )
{
	settings->setWindowSize( 800, 600 );
}

void blendApp::setup()
{
	gl::disableVerticalSync();

	mParams = params::InterfaceGl( "Parameters", Vec2i( 200, 300 ) );
	mParams.addParam( "Fps", &mFps, "", false );

	mFaceShift.import( "export" );
	mFaceShift.connect();

	gl::enable( GL_CULL_FACE );
}

void blendApp::update()
{
	mHeadRotation = mFaceShift.getRotation();
	mLeftEyeRotation = mFaceShift.getLeftEyeRotation();
	mRightEyeRotation = mFaceShift.getRightEyeRotation();
	mHeadMesh = mFaceShift.getBlendMesh();

	mFps = getAverageFps();
}

void blendApp::draw()
{
	gl::clear( Color::black() );

	CameraPersp cam( getWindowWidth(), getWindowHeight(), 60.0f );
	cam.setPerspective( 60, getWindowAspectRatio(), 1, 1000 );
	cam.lookAt( Vec3f( 0, 0, -300 ), Vec3f::zero(), Vec3f( 0, -1, 0 ) );
	gl::setMatrices( cam );

	gl::setViewport( getWindowBounds() );

	gl::enableWireframe();
	gl::pushModelView();
	gl::rotate( mHeadRotation );
	gl::draw( mHeadMesh );
	gl::popModelView();
	gl::disableWireframe();

	params::InterfaceGl::draw();
}

CINDER_APP_BASIC( blendApp, RendererGl )


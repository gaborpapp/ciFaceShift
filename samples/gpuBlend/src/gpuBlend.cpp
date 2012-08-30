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
#include "cinder/Arcball.h"
#include "cinder/Camera.h"
#include "cinder/Cinder.h"
#include "cinder/ip/Flip.h"
#include "cinder/gl/gl.h"
#include "cinder/gl/GlslProg.h"
#include "cinder/gl/Texture.h"
#include "cinder/gl/Vbo.h"
#include "cinder/params/Params.h"
#include "cinder/TriMesh.h"

#include "ciFaceshift.h"
#include "Resources.h"

using namespace ci;
using namespace ci::app;
using namespace std;

class gpuBlendApp : public AppBasic
{
	public:
		void prepareSettings( Settings *settings );
		void setup();

		void resize( ResizeEvent event );
		void mouseDown( MouseEvent event );
		void mouseDrag( MouseEvent event );
		void mouseWheel ( MouseEvent event );
		void keyDown( KeyEvent event );

		void update();
		void draw();

	private:
		Quatf mHeadRotation;
		Quatf mLeftEyeRotation;
		Quatf mRightEyeRotation;

		Arcball mArcball;
		float mEyeDistance;

		gl::VboMesh mVboMesh;
		gl::Texture mBlendshapeTexture;
		gl::GlslProg mShader;
		void setupVbo();

		mndl::faceshift::ciFaceShift mFaceShift;

		params::InterfaceGl mParams;
		float mFps;
		float mBlend;
};

void gpuBlendApp::prepareSettings( Settings *settings )
{
	settings->setWindowSize( 800, 600 );
}

void gpuBlendApp::setup()
{
	gl::disableVerticalSync();

	try
	{
		mShader = gl::GlslProg( loadResource( RES_BLEND_VERT ),
								loadResource( RES_BLEND_FRAG ) );
	}
	catch ( const std::exception &exc )
	{
		console() << exc.what() << endl;
		throw exc;
	}

	mParams = params::InterfaceGl( "Parameters", Vec2i( 200, 300 ) );
	mParams.addParam( "Fps", &mFps, "", false );
	mBlend = 0;
	mParams.addParam( "Blend", &mBlend, "min=0 max=1 step=.005" );

	mFaceShift.import( "export" );

	mEyeDistance = -300;

	setupVbo();

	mFaceShift.connect();

	//gl::enable( GL_CULL_FACE );
}

void gpuBlendApp::setupVbo()
{
	const TriMesh& neutralMesh = mFaceShift.getNeutralMesh();
	size_t numVertices = neutralMesh.getNumVertices();
	size_t numBlendShapes = mFaceShift.getNumBlendshapes();

	gl::VboMesh::Layout layout;

	layout.setStaticPositions();
	//layout.setStaticTexCoords2d();
	layout.setStaticIndices();

	// TODO: int attribute
	layout.mCustomStatic.push_back( std::make_pair( gl::VboMesh::Layout::CUSTOM_ATTR_FLOAT, 0 ) );

	mVboMesh = gl::VboMesh( neutralMesh.getNumVertices(),
							neutralMesh.getNumIndices(), layout, GL_TRIANGLES );

	mVboMesh.bufferPositions( neutralMesh.getVertices() );
	//mVboMesh.bufferPositions( mFaceShift.getBlendshapeMesh( 0 ).getVertices() );
	mVboMesh.bufferIndices( neutralMesh.getIndices() );
	//mVboMesh.bufferTexCoords2d( 0, neutralMesh.getTexCoords() );
	mVboMesh.unbindBuffers();

	vector< float > vertexIndices( numVertices, 0.f );
	for ( uint32_t i = 0; i < numVertices; ++i )
		vertexIndices[ i ] = static_cast< float >( i );

	mVboMesh.getStaticVbo().bind();
	size_t offset = sizeof( GLfloat ) * 3 * neutralMesh.getNumVertices();
	mVboMesh.getStaticVbo().bufferSubData( offset,
			numVertices * sizeof( float ),
			&vertexIndices[ 0 ] );
	mVboMesh.getStaticVbo().unbind();

	mShader.bind();
	GLint location = mShader.getAttribLocation( "index" );
	mVboMesh.setCustomStaticLocation( 0, location );
	mShader.uniform( "blendshapes", 0 );
	mShader.uniform( "numBlendshapes", static_cast< int >( numBlendShapes ) );
	mShader.unbind();

	// blendshapes texture
	gl::Texture::Format format;
	format.setTargetRect();
	format.setWrap( GL_CLAMP, GL_CLAMP );
	format.setMinFilter( GL_NEAREST );
	format.setMagFilter( GL_NEAREST );
	format.setInternalFormat( GL_RGB32F_ARB );

#if 1
	int verticesPerRow = 32;
	int txtWidth = verticesPerRow * numBlendShapes;
	int txtHeight = math< float >::ceil( numVertices / (float)verticesPerRow );

	Surface32f blendshapeSurface = Surface32f( txtWidth, txtHeight, false );
	float *ptr = blendshapeSurface.getData();

	size_t idx = 0;
	for ( int j = 0; j < txtHeight; j++ )
	{
		for ( int s = 0; s < verticesPerRow; s++ )
		{
			for ( size_t i = 0; i < numBlendShapes; i++ )
			{
				*( reinterpret_cast< Vec3f * >( ptr ) ) = mFaceShift.getBlendshapeMesh( i ).getVertices()[ idx ]; // - neutralMesh.getVertices()[ idx ];
				ptr += 3;
			}
			idx++;
		}
	}
#endif

#if 0
	int verticesPerRow = 32;
	int txtWidth = verticesPerRow;
	int txtHeight = math< float >::ceil( numVertices / (float)verticesPerRow );

	Surface32f blendshapeSurface = Surface32f( txtWidth, txtHeight, false );
	float *ptr = blendshapeSurface.getData();

	size_t idx = 0;
	for ( int j = 0; j < txtHeight; j++ )
	{
		for ( int s = 0; s < verticesPerRow; s++ )
		{
			*( reinterpret_cast< Vec3f * >( ptr ) ) = mFaceShift.getBlendshapeMesh( 1 ).getVertices()[ idx ];
			ptr += 3;
			idx++;
		}
	}
#endif

	mBlendshapeTexture = gl::Texture( blendshapeSurface, format );
}

void gpuBlendApp::update()
{
	mHeadRotation = mFaceShift.getRotation();
	mLeftEyeRotation = mFaceShift.getLeftEyeRotation();
	mRightEyeRotation = mFaceShift.getRightEyeRotation();

	mFps = getAverageFps();
}

void gpuBlendApp::draw()
{
	gl::clear( Color::black() );

	CameraPersp cam( getWindowWidth(), getWindowHeight(), 60.0f );
	cam.setPerspective( 60, getWindowAspectRatio(), 1, 5000 );
	cam.lookAt( Vec3f( 0, 0, mEyeDistance ), Vec3f::zero(), Vec3f( 0, -1, 0 ) );
	gl::setMatrices( cam );

	gl::setViewport( getWindowBounds() );

	mShader.bind();
	mShader.uniform( "blend", mBlend );
	mBlendshapeTexture.enableAndBind();
	gl::color( ColorA::gray( 1.0, .1 ) );
	gl::enableAdditiveBlending();
	gl::enableWireframe();
	gl::pushModelView();
	gl::rotate( mArcball.getQuat() );
	gl::rotate( mHeadRotation );
	gl::draw( mVboMesh );
	gl::popModelView();
	gl::disableWireframe();
	mBlendshapeTexture.unbind();
	gl::disable( GL_TEXTURE_RECTANGLE_ARB );
	gl::disableAlphaBlending();
	mShader.unbind();

	params::InterfaceGl::draw();
}

void gpuBlendApp::mouseDown( MouseEvent event )
{
	mArcball.mouseDown( event.getPos() );
}

void gpuBlendApp::mouseDrag( MouseEvent event )
{
	mArcball.mouseDrag( event.getPos() );
}

void gpuBlendApp::mouseWheel ( MouseEvent event )
{
	float w = event.getWheelIncrement();

	if ( w > 0 )
		mEyeDistance *= ( 1.1 + .5 * w );
	else
		mEyeDistance /= ( 1.1 - .5 * w );
}

void gpuBlendApp::resize( ResizeEvent event )
{
	mArcball.setWindowSize( getWindowSize() );
	mArcball.setCenter( getWindowCenter() );
	mArcball.setRadius( 150 );
}

void gpuBlendApp::keyDown( KeyEvent event )
{
	switch ( event.getCode() )
	{
		case KeyEvent::KEY_f:
			if ( !isFullScreen() )
			{
				setFullScreen( true );
				if ( mParams.isVisible() )
					showCursor();
				else
					hideCursor();
			}
			else
			{
				setFullScreen( false );
				showCursor();
			}
			break;

		case KeyEvent::KEY_s:
			mParams.show( !mParams.isVisible() );
			if ( isFullScreen() )
			{
				if ( mParams.isVisible() )
					showCursor();
				else
					hideCursor();
			}
			break;

		case KeyEvent::KEY_ESCAPE:
			quit();
			break;

		default:
			break;
	}
}

CINDER_APP_BASIC( gpuBlendApp, RendererGl )


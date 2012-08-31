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

#include <iostream>
#include <algorithm>
#include <iterator>

#include <boost/assign.hpp>

#include "cinder/app/App.h"
#include "cinder/DataSource.h"
#include "cinder/ObjLoader.h"

#include "ciFaceShift.h"

using namespace ci;
using boost::asio::ip::tcp;

namespace mndl { namespace faceshift {

const std::vector< std::string > ciFaceShift::sBlendshapeNames =
	boost::assign::list_of( "EyeBlink_L" )( "EyeBlink_R" )
		( "EyeSquint_L" )( "EyeSquint_R" )
		( "EyeDown_L" )( "EyeDown_R" )( "EyeIn_L" )( "EyeIn_R" )
		( "EyeOpen_L" )( "EyeOpen_R" )( "EyeOut_L" )( "EyeOut_R" )
		( "EyeUp_L" )( "EyeUp_R" )( "BrowsD_L" )( "BrowsD_R" )
		( "BrowsU_C" )( "BrowsU_L" )( "BrowsU_R" )( "JawFwd" )
		( "JawLeft" )( "JawOpen" )( "JawRight" )( "MouthLeft" )( "MouthRight" )
		( "MouthFrown_L" )( "MouthFrown_R" )( "MouthSmile_L" )( "MouthSmile_R" )
		( "MouthDimple_L" )( "MouthDimple_R" )( "LipsStretch_L" )( "LipsStretch_R" )
		( "LipsUpperClose" )( "LipsLowerClose" )( "LipsUpperUp" )( "LipsLowerDown" )
		( "LipsLowerOpen" )( "LipsFunnel" )( "LipsPucker" )
		( "ChinLowerRaise" )( "ChinUpperRaise" )( "Sneer" )( "Puff" )
		( "CheekSquint_L" )( "CheekSquint_R" );

ciFaceShift::ciFaceShift() :
	mSocket ( mIoService ),
	mTimestamp( 0 ),
	mTrackingSuccessful( false ),
	mBlendNeedsUpdate( false )
{
	mBlendshapeWeights.assign( sBlendshapeNames.size(), 0.f );
}

ciFaceShift::~ciFaceShift()
{
	close();
	mThread->join();
}

void ciFaceShift::connect( std::string host /* = "127.0.0.1" */,
						   std::string port /* = "33433" */ )
{
	tcp::resolver resolver( mIoService );
	tcp::resolver::query query( host, port );
	tcp::resolver::iterator iterator = resolver.resolve( query );
	tcp::endpoint endpoint = *iterator;

	mSocket.async_connect( endpoint,
							boost::bind( &ciFaceShift::handleConnect, this,
							boost::asio::placeholders::error, ++iterator ) );

	mThread = std::shared_ptr< boost::thread >( new boost::thread( boost::bind(
					&boost::asio::io_service::run, &mIoService ) ) );
}

void ciFaceShift::handleConnect( const boost::system::error_code& error,
								 tcp::resolver::iterator endpoint_iterator )
{
	if ( !error )
	{
		boost::asio::async_read( mSocket,
				mStream,
				boost::asio::transfer_at_least( 1 ),
				boost::bind( &ciFaceShift::handleRead, this,
					boost::asio::placeholders::error ) );
	}
	else if ( endpoint_iterator != tcp::resolver::iterator() )
	{
		mSocket.close();
		tcp::endpoint endpoint = *endpoint_iterator;
		mSocket.async_connect( endpoint,
				boost::bind( &ciFaceShift::handleConnect, this,
					boost::asio::placeholders::error, ++endpoint_iterator ) );
	}
	else
	{
		throw boost::system::system_error( error );
	}
}

void ciFaceShift::handleRead( const boost::system::error_code& error )
{
	if ( error )
	{
		doClose();
		return;
	}

	std::istream is( &mStream );

	uint16_t blockId;
	uint16_t versionNumber;
	uint32_t blockSize;

	readRaw( is, blockId );
	readRaw( is, versionNumber );
	readRaw( is, blockSize );

	if ( blockId == FS_DATA_CONTAINER_BLOCK )
	{
		uint16_t numberBlocks;
		readRaw( is, numberBlocks );
		for ( uint16_t i = 0; i < numberBlocks; ++i )
		{
			readRaw( is, blockId );
			readRaw( is, versionNumber );
			readRaw( is, blockSize);

			switch ( blockId )
			{
				case FS_FRAME_INFO_BLOCK:
				{
					boost::lock_guard< boost::mutex > lock( mMutex );
					readRaw( is, mTimestamp );
					uint8_t success;
					readRaw( is, success );
					mTrackingSuccessful = ( success == 1 );
					break;
				}

				case FS_POSE_BLOCK:
				{
					boost::lock_guard< boost::mutex > lock( mMutex );
					readRaw( is, mHeadOrientation.v.x );
					readRaw( is, mHeadOrientation.v.y );
					readRaw( is, mHeadOrientation.v.z );
					readRaw( is, mHeadOrientation.w );
					readRaw( is, mHeadPosition.x );
					readRaw( is, mHeadPosition.y );
					readRaw( is, mHeadPosition.z );
					break;
				}

				case FS_BLENDSHAPES_BLOCK:
				{
					boost::lock_guard< boost::mutex > lock( mMutex );
					uint32_t blendshapeCount;
					readRaw( is, blendshapeCount );

					if ( blendshapeCount != mBlendshapeWeights.size() )
					{
						mBlendshapeWeights.resize( blendshapeCount );
					}

					for ( uint32_t i = 0; i < blendshapeCount; ++i )
					{
						readRaw( is, mBlendshapeWeights[ i ] );
					}
					mBlendNeedsUpdate = true;
					break;
				}

				case FS_EYES_BLOCK:
				{
					boost::lock_guard< boost::mutex > lock( mMutex );
					readRaw( is, mLeftEyeRotation.theta );
					readRaw( is, mLeftEyeRotation.phi );
					readRaw( is, mRightEyeRotation.theta );
					readRaw( is, mRightEyeRotation.phi );
					break;
				}

				case FS_MARKERS_BLOCK:
				{
					boost::lock_guard< boost::mutex > lock( mMutex );
					mMarkers.clear();
					uint16_t markerCount;
					readRaw( is, markerCount );
					for ( uint16_t i = 0; i < markerCount; ++i )
					{
						Vec3f marker;
						readRaw( is, marker.x );
						readRaw( is, marker.y );
						readRaw( is, marker.z );
						mMarkers.push_back( marker );
					}
					break;
				}
			}
		}
	}

	mStream.consume( mStream.size() );
	boost::asio::async_read( mSocket,
			mStream,
			boost::asio::transfer_at_least( 1 ),
			boost::bind( &ciFaceShift::handleRead, this,
				boost::asio::placeholders::error ) );
}

void ciFaceShift::doClose()
{
	mSocket.close();
}

void ciFaceShift::close()
{
	mIoService.post( boost::bind( &ciFaceShift::doClose, this ) );
}

void ciFaceShift::import( fs::path folder, bool exportTrimesh /* = false */ )
{
	fs::path dataPath = app::getAssetPath( folder );

	std::vector< fs::path > folderContents;
	copy( fs::directory_iterator( dataPath ), fs::directory_iterator(), std::back_inserter( folderContents ) );
	std::sort( folderContents.begin(), folderContents.end() );

	for ( std::vector< fs::path >::const_iterator it = folderContents.begin();
			it != folderContents.end(); ++it )
	{
		if ( fs::is_regular_file( *it ) && ( it->extension().string() == ".obj" ) ||
			 ( it->extension().string() == ".trimesh" ) )
		{
			if ( it->filename().extension().string() == ".obj" )
			{
				fs::path trimeshPath = *it;
				trimeshPath.replace_extension( ".trimesh" );

				if ( fs::exists( trimeshPath ) )
					continue;

				ObjLoader loader( loadFile( *it ) );
				if ( it->filename().stem() == "Neutral" )
				{
					// no normals, with texcoords, optimization
					loader.load( &mNeutralMesh, false, true, true );

					if ( exportTrimesh )
						mNeutralMesh.write( writeFile( trimeshPath ) );
				}
				else
				{
					TriMesh trimesh;
					// no normals, with texcoords, optimization
					loader.load( &trimesh, false, true, true );
					mBlendshapeMeshes.push_back( trimesh );

					if ( exportTrimesh )
						trimesh.write( writeFile( trimeshPath ) );
				}
			}
			else // .trimesh
			{
				if ( it->filename().stem() == "Neutral" )
				{
					mNeutralMesh.read( loadFile( *it ) );
				}
				else
				{
					TriMesh trimesh;
					trimesh.read( loadFile( *it ) );
					mBlendshapeMeshes.push_back( trimesh );
				}
			}
		}
	}

	/*
	const std::vector< Vec3f >& neutralVertices = mNeutralMesh.getVertices();
	for ( std::vector< TriMesh >::iterator it = mBlendshapeMeshes.begin();
			it != mBlendshapeMeshes.end(); ++it )
	{
		std::vector< Vec3f >& vertices = it->getVertices();
		for ( size_t n = 0; n < vertices.size(); n++ )
		{
			vertices[ n ] -= neutralVertices[ n ];
		}
	}
	*/

	mBlendMesh = mNeutralMesh;
}

Quatf ciFaceShift::getRotation() const
{
	boost::lock_guard< boost::mutex > lock( mMutex );
	return mHeadOrientation;
}

Vec3f ciFaceShift::getPosition() const
{
	boost::lock_guard< boost::mutex > lock( mMutex );
	return mHeadPosition;
}

double ciFaceShift::getTimestamp() const
{
	boost::lock_guard< boost::mutex > lock( mMutex );
	return mTimestamp;
}

bool ciFaceShift::isTrackingSuccessful() const
{
	boost::lock_guard< boost::mutex > lock( mMutex );
	return mTrackingSuccessful;
}

const std::vector< std::string >& ciFaceShift::getBlendshapeNames() const
{
	return sBlendshapeNames;
}

std::string ciFaceShift::getBlendshapeName( size_t i ) const
{
	return sBlendshapeNames[ i ];
}

size_t ciFaceShift::getNumBlendshapes() const
{
	boost::lock_guard< boost::mutex > lock( mMutex );
	return mBlendshapeWeights.size();
}

const std::vector< float >& ciFaceShift::getBlendshapeWeights() const
{
	boost::lock_guard< boost::mutex > lock( mMutex );
	return mBlendshapeWeights;
}

float ciFaceShift::getBlendshapeWeight( size_t i ) const
{
	boost::lock_guard< boost::mutex > lock( mMutex );
	return mBlendshapeWeights[ i ];
}

Quatf ciFaceShift::getLeftEyeRotation() const
{
	boost::lock_guard< boost::mutex > lock( mMutex );
	return mLeftEyeRotation.toQuat();
}

Quatf ciFaceShift::getRightEyeRotation() const
{
	boost::lock_guard< boost::mutex > lock( mMutex );
	return mRightEyeRotation.toQuat();
}

const TriMesh& ciFaceShift::getBlendshapeMesh( size_t i ) const
{
	return mBlendshapeMeshes[ i ];
}

TriMesh& ciFaceShift::getBlendMesh()
{
	if ( !mBlendshapeMeshes.empty() && mBlendNeedsUpdate )
	{
		std::vector< Vec3f >& outputVertices = mBlendMesh.getVertices();
		const std::vector< Vec3f >& neutralVertices = mNeutralMesh.getVertices();
		outputVertices = neutralVertices;

		for ( size_t i = 0; i < mBlendshapeWeights.size(); i++ )
		{
			float weight = getBlendshapeWeight( i );
			std::vector< Vec3f >& blendshapeVertices = mBlendshapeMeshes[ i ].getVertices();
			for ( size_t j = 0; j < outputVertices.size(); j++ )
			{
				outputVertices[ j ] += weight * ( blendshapeVertices[ j ] - neutralVertices[ j ] );
			}
		}
		mBlendNeedsUpdate = false;
	}

	return mBlendMesh;
}

const ci::TriMesh& ciFaceShift::getNeutralMesh() const
{
	return mNeutralMesh;
}

} } // mndl::faceshift

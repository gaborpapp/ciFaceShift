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

#include "cinder/app/App.h"

#include "ciFaceShift.h"

using namespace ci;
using boost::asio::ip::tcp;

namespace mndl { namespace faceshift {

ciFaceShift::ciFaceShift() :
	mSocket ( mIoService ),
	mTimestamp( 0 ),
	mTrackingSuccessful( false )
{}

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
					mBlendshapeWeights.clear();
					uint32_t blendshapeCount;
					readRaw( is, blendshapeCount );
					for ( uint32_t i = 0; i < blendshapeCount; ++i )
					{
						float blendshapeWeight;
						readRaw( is, blendshapeWeight );
						mBlendshapeWeights.push_back( blendshapeWeight );
					}
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

} } // mndl::faceshift

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
#include <string>
#include <vector>

#include "cinder/Cinder.h"
#include "cinder/CinderMath.h"
#include "cinder/Vector.h"
#include "cinder/Quaternion.h"

#include <boost/bind.hpp>
#include <boost/asio.hpp>
#include <boost/thread.hpp>

namespace mndl { namespace faceshift {

class ciFaceShift
{
	public:
		ciFaceShift();
		~ciFaceShift();

		/*! Connects to fsStudio.  The optional \a host and \a port parameters
		 * specify the fsStudio server.
		 * \note Only supports TCP/IP at the moment, which can be set in fsStudio Preferences/Streaming/Network/Protocol.
		 */
		void connect( std::string host = "127.0.0.1", std::string port = "33433" );
		//! Closes the connection to fsStudio.
		void close();

		//! Returns head orientation.
		ci::Quatf getRotation() const;
		/*! Returns head position in millimetres.
		 * \note This is relative position by default. Can be set to absolute in fsStudio Preferences/Streaming/Streaming Data/Head Pose.
		 */
		ci::Vec3f getPosition() const;

		//! Returns the timestamp of the last frame received.
		double getTimestamp() const;
		//! Returns true if the tracking of the last frame was successful.
		bool isTrackingSuccessful() const;

		//! Returns the name of the blendshapes as a vector of strings.
		const std::vector< std::string >& getBlendshapeNames() const;

		//! Returns the name of the \a i'th blendshape.
		std::string getBlendshapeName( size_t i ) const;

		//! Returns the blendshape coefficients for the last frame received.
		const std::vector< float >& getBlendshapeWeights() const;

		//! Returns the total number of blendshapes.
		size_t getNumBlendshapes() const;

		//! Returns the \a i'th blendshape coefficient for the last frame received.
		float getBlendshapeWeight( size_t i ) const;

		//! Returns left eye rotation.
		ci::Quatf getLeftEyeRotation() const;

		//! Returns right eye rotation.
		ci::Quatf getRightEyeRotation() const;

	private:
		void handleConnect( const boost::system::error_code& error,
							boost::asio::ip::tcp::resolver::iterator endpoint_iterator );
		void handleRead( const boost::system::error_code& error );
		void doClose();

		boost::asio::io_service mIoService;
		boost::asio::ip::tcp::socket mSocket;
		boost::asio::streambuf mStream;

		enum
		{
			FS_DATA_CONTAINER_BLOCK = 33433,
			FS_FRAME_INFO_BLOCK = 101,
			FS_POSE_BLOCK = 102,
			FS_BLENDSHAPES_BLOCK = 103,
			FS_EYES_BLOCK = 104,
			FS_MARKERS_BLOCK = 105
		};

		template <typename T>
		inline void readRaw( std::istream& is, T& data )
		{
			is.read( reinterpret_cast< char * >( &data), sizeof( T ) );
		}

		std::shared_ptr< boost::thread > mThread;
		mutable boost::mutex mMutex;

		double mTimestamp;
		bool mTrackingSuccessful;
		ci::Quatf mHeadOrientation;
		ci::Vec3f mHeadPosition;
		std::vector< float > mBlendshapeWeights;

		struct SpCoordf
		{
			float phi; //!< polar angle in degrees (-90, 90)
			float theta; //!< azimuthal angle in degrees (-90, 90)

			ci::Quatf toQuat() const
			{
				return ci::Quatf( ci::Vec3f( 0, 1, 0 ), ci::toRadians( phi ) ) *
					   ci::Quatf( ci::Vec3f( 1, 0, 0 ), ci::toRadians( theta ) );
			}

		};
		SpCoordf mLeftEyeRotation;
		SpCoordf mRightEyeRotation;

		std::vector< ci::Vec3f > mMarkers;

		static const std::vector< std::string > sBlendshapeNames;
};

} } // namespace mndl::faceshift

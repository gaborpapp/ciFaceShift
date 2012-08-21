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

		void connect( std::string host = "127.0.0.1", std::string port = "33433" );
		void close();

	private:
		void handleConnect( const boost::system::error_code& error,
							boost::asio::ip::tcp::resolver::iterator endpoint_iterator );
		void handleRead( const boost::system::error_code& error, size_t bytesRead );
		void doClose();

		boost::asio::io_service mIoService;
		boost::asio::ip::tcp::socket mSocket;
		boost::asio::streambuf mStream;

		enum
		{
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

		double mTimeStamp;
		bool mTrackingSuccessful;
		ci::Quatf mHeadOrientation;
		ci::Vec3f mHeadPosition;
		std::vector< float > mBlendshapeWeights;

		struct SpCoordf
		{
			float phi;
			float theta;
		};
		SpCoordf mLeftEyeRotation;
		SpCoordf mRightEyeRotation;

		std::vector< ci::Vec3f > mMarkers;
};

} } // namespace mndl::faceshift

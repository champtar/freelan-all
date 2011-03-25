/*
 * libcryptoplus - C++ portable OpenSSL cryptographic wrapper library.
 * Copyright (C) 2010-2011 Julien Kauffmann <julien.kauffmann@freelan.org>
 *
 * This file is part of libcryptoplus.
 *
 * libcryptoplus is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 3 of
 * the License, or (at your option) any later version.
 *
 * libcryptoplus is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program. If not, see
 * <http://www.gnu.org/licenses/>.
 *
 * In addition, as a special exception, the copyright holders give
 * permission to link the code of portions of this program with the
 * OpenSSL library under certain conditions as described in each
 * individual source file, and distribute linked combinations
 * including the two.
 * You must obey the GNU General Public License in all respects
 * for all of the code used other than OpenSSL.  If you modify
 * file(s) with this exception, you may extend this exception to your
 * version of the file(s), but you are not obligated to do so.  If you
 * do not wish to do so, delete this exception statement from your
 * version.  If you delete this exception statement from all source
 * files in the program, then also delete it here.
 *
 * If you intend to use libcryptoplus in a commercial software, please
 * contact me : we may arrange this for a small fee or no fee at all,
 * depending on the nature of your project.
 */

/**
 * \file utctime_ptr.cpp
 * \author Julien KAUFFMANN <julien.kauffmann@freelan.org>
 * \brief An ASN1_UTCTIME pointer class.
 */

#include "asn1/utctime_ptr.hpp"

#include <boost/shared_ptr.hpp>

#include <cassert>

namespace cryptoplus
{
	namespace asn1
	{
		namespace
		{
			boost::posix_time::ptime epoch(boost::gregorian::date(1970, 1, 1));
		}

		void utctime_ptr::set_time(const boost::posix_time::ptime& time)
		{
			set_time(static_cast<time_t>((time - epoch).total_seconds()));
		}
		
		boost::posix_time::ptime utctime_ptr::to_ptime()
		{
			//TODO: Implement this
			
			return boost::posix_time::ptime();
		}
	}
}


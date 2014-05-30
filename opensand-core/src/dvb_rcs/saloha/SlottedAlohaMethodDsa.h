/*
 *
 * OpenSAND is an emulation testbed aiming to represent in a cost effective way a
 * satellite telecommunication system for research and engineering activities.
 *
 *
 * Copyright © 2013 TAS
 * Copyright © 2013 CNES
 *
 *
 * This file is part of the OpenSAND testbed.
 *
 *
 * OpenSAND is free software : you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option) any later
 * version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY, without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program. If not, see http://www.gnu.org/licenses/.
 *
 */

/**
 * @file SlottedAlohaDsa.h
 * @brief The DSA method
 * @author Vincent WINKEL <vincent.winkel@thalesaleniaspace.com> <winkel@live.fr>
*/

#ifndef SALOHA_METHOD_DSA_H
#define SALOHA_METHOD_DSA_H

#include "SlottedAlohaMethod.h"

/**
 * @class SlottedAlohaDsa
 * @brief The DSA method
*/

class SlottedAlohaMethodDsa: public SlottedAlohaMethod
{
 public:
	SlottedAlohaMethodDsa();

	~SlottedAlohaMethodDsa();

 private:
	uint16_t removeCollisions(map<unsigned int, Slot *> &slots,
	                          saloha_packets_t *accepted_packets);
};

#endif


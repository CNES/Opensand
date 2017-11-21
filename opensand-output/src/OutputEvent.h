/*
 *
 * OpenSAND is an emulation testbed aiming to represent in a cost effective way a
 * satellite telecommunication system for research and engineering activities.
 *
 *
 * Copyright © 2017 TAS
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
 * @file OutputEvent.h
 * @brief The OutputEvent class represent an event generated by the application.
 *        This is a specific log that should always be sent to manager
 *        if collector is enabled, it is used to describe important
 *        steps in platform behaviour
 * @author Vincent Duvert <vduvert@toulouse.viveris.com>
 */


#ifndef _OUTPUT_EVENT_H
#define _OUTPUT_EVENT_H

#include "OutputLog.h"

#include <string>
#include <stdint.h>


/**
 * @class Represent an event
 */
class OutputEvent: public OutputLog
{
	friend class OutputInternal;
	friend class OutputOpensand;

  public:
	~OutputEvent() {};

  protected:
	OutputEvent(uint8_t id, const string &name):
		OutputLog(id, LEVEL_EVENT, name)
	{};

	void setDisplayLevel(log_level_t level)
	{
		return;
	};
};

#endif

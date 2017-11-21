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
 * @file OutputOpensand.h
 * @brief Class used to hold opensand output library variables and methods.
 * @author Alban Fricot <africot@toulouse.viveris.com>
 */


#ifndef _OUTPUT_OPENSAND_H
#define _OUTPUT_OPENSAND_H

#include "Probe.h"
#include "OutputLog.h"
#include "OutputEvent.h"
#include "OutputMutex.h"
#include "OutputInternal.h"
#include <assert.h>
#include <sys/un.h>
#include <vector>
#include <map>

using std::vector;
using std::map;


/**
 * @class hold opensand output library variables and methods
 */
class OutputOpensand : public OutputInternal
{
	friend class Output;

  public:
	~OutputOpensand();

  protected:
	OutputOpensand();

	/**
	 * @brief initialize the output element
	 *
	 * @param enable_collector  Whether the element is enabled
	 * @param sock_prefix        The socket prefix
	 * @return true on success, false otherwise
	 */
	bool init(bool enable_collector, 
	          const char *sock_prefix);

	/**
	 * @brief Finish the element initialization
	 **/
	bool finishInit(void);

	/**
	 * @brief Send all probes which got new values sinces the last call.
	 **/
	void sendProbes(void);

	/**
	 * @brief Send the specified log with the specified message
	 *
	 * @param log		The log
	 * @param log_level	The log level
	 * @param message	The message
	 **/
	void sendLog(const OutputLog *log, log_level_t log_level,
	             const string &message_text);

	/**
	 * @brief  Send registration for a probe outside initialization
	 *
	 * @param probe  The new probe to register
	 * @return true on success, false otherwise
	 */
	bool sendRegister(BaseProbe *probe);

	/**
	 * @brief  Send registration for a log outside initialization
	 *
	 * @param probe  The new log to register
	 * @return true on success, false otherwise
	 */
	bool sendRegister(OutputLog *log);

	/**
	 * @brief  Get the daemon socket address
	 *
	 * @return the daemon socket address
	 */
	inline const sockaddr_un *daemonSockAddr()
	{
		return &this->daemon_sock_addr;
	};

  private:
	/// the socket for communication with daemon
	int sock;

	/// the dameon socket address
	sockaddr_un daemon_sock_addr;

	/// the element socket address
	sockaddr_un self_sock_addr;

	/**
	 * @brief  Send a message to the daemon
	 *
	 * @param message  The message
	 * @param block    Whether we should block until message can be sent
	 * @return true on success or non-blocked operation, false otherwise
	 */
	bool sendMessage(const string &message, bool block=true) const;

	/**
	 * @brief receive a message from the daemon
	 *
	 * @return the command type on success, 0 on failure
	 */
	uint8_t rcvMessage(void) const;
};

#endif

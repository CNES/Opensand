/*
 *
 * OpenSAND is an emulation testbed aiming to represent in a cost effective way a
 * satellite telecommunication system for research and engineering activities.
 *
 *
 * Copyright © 2019 TAS
 *
 *
 * This file is part of the OpenSAND testbed.
 *
 *
 * OpenSAND is free software : you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License as published by the Free
 * Software Foundation, either version 3 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY, without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program. If not, see http://www.gnu.org/licenses/.
 *
 */

/**
 * @file RtChannel.cpp
 * @author Yohan SIMARD / <yohan.simard@viveris.fr>
 * @brief  A simple channel with 1 input fifo and 1 output fifo
 */

#include "RtChannel.h"
#include "RtFifo.h"


RtChannel::RtChannel(const std::string &name, const std::string &type):
	RtChannelBase{name, type},
	previous_fifo{nullptr},
	next_fifo{nullptr}
{
}


bool RtChannel::initPreviousFifo()
{
  return this->initSingleFifo(this->previous_fifo);
}


bool RtChannel::enqueueMessage(void **data, size_t size, uint8_t type)
{
	return this->pushMessage(this->next_fifo, data, size, type);
}


void RtChannel::setPreviousFifo(std::shared_ptr<RtFifo> &fifo)
{
	this->previous_fifo = fifo;
};


void RtChannel::setNextFifo(std::shared_ptr<RtFifo> &fifo)
{
	this->next_fifo = fifo;
};

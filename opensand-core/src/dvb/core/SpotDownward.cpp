/*
 *
 * OpenSAND is an emulation testbed aiming to represent in a cost effective way a
 * satellite telecommunication system for research and engineering activities.
 *
 *
 * Copyright © 2015 TAS
 * Copyright © 2015 CNES
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
 * @file SpotDownward.cpp
 * @brief Downward spot related functions for DVB NCC block
 * @author Bénédicte Motto <bmotto@toulouse.viveris.com>
 * @author Julien Bernard <julien.bernard@toulouse.viveris.com>
 *
 */

#include "SpotDownward.h"

#include "ForwardSchedulingS2.h"
#include "UplinkSchedulingRcs.h"
#include "DamaCtrlRcsLegacy.h"
#include "FileSimulator.h"
#include "RandomSimulator.h"

#include <errno.h>


SpotDownward::SpotDownward(spot_id_t spot_id,
                           tal_id_t mac_id,
                           time_ms_t fwd_down_frame_duration,
                           time_ms_t ret_up_frame_duration,
                           time_ms_t stats_period,
                           sat_type_t sat_type,
                           EncapPlugin::EncapPacketHandler *pkt_hdl,
                           StFmtSimuList *input_sts,
                           StFmtSimuList *output_sts,
                           bool phy_layer):
	DvbChannel(),
	dama_ctrl(NULL),
	scheduling(NULL),
	fwd_frame_counter(0),
	ctrl_carrier_id(),
	sof_carrier_id(),
	data_carrier_id(),
	spot_id(spot_id),
	mac_id(mac_id),
	dvb_fifos(),
	default_fifo_id(0),
	complete_dvb_frames(),
	categories(),
	terminal_affectation(),
	default_category(NULL),
	up_return_pkt_hdl(NULL),
	fwd_fmt_groups(),
	ret_fmt_groups(),
	cni(100),
	pep_cmd_apply_timer(),
	acm_timer(-1),
	request_simu(NULL),
	event_file(NULL),
	simulate(none_simu),
	probe_gw_queue_size(),
	probe_gw_queue_size_kb(),
	probe_gw_queue_loss(),
	probe_gw_queue_loss_kb(),
	probe_gw_l2_to_sat_before_sched(),
	probe_gw_l2_to_sat_after_sched(),
	probe_gw_l2_to_sat_total(NULL),
	l2_to_sat_total_bytes(0),
	probe_frame_interval(NULL),
	probe_used_modcod(NULL),
	event_logon_resp(NULL)
{
	this->fwd_down_frame_duration_ms = fwd_down_frame_duration;
	this->ret_up_frame_duration_ms = ret_up_frame_duration;
	this->stats_period_ms = stats_period;
	this->satellite_type = sat_type;
	this->pkt_hdl = pkt_hdl;
	this->with_phy_layer = phy_layer;
	this->input_sts = input_sts;
	this->output_sts = output_sts;

	this->log_request_simulation = Output::registerLog(LEVEL_WARNING,
	                                                   "Spot_%d.Dvb.RequestSimulation",
		                                               this->spot_id);

}

SpotDownward::~SpotDownward()
{
	if(this->dama_ctrl)
		delete this->dama_ctrl;
	if(this->scheduling)
		delete this->scheduling;

	this->complete_dvb_frames.clear();

	// delete FMT groups here because they may be present in many carriers
	// TODO do something to avoid groups here
	for(fmt_groups_t::iterator it = this->fwd_fmt_groups.begin();
	    it != this->fwd_fmt_groups.end(); ++it)
	{
		delete (*it).second;
	}
	// delete FMT groups here because they may be present in many carriers
	// TODO do something to avoid groups here
	for(fmt_groups_t::iterator it = this->ret_fmt_groups.begin();
	    it != this->ret_fmt_groups.end(); ++it)
	{
		delete (*it).second;
	}

	// delete fifos
	for(fifos_t::iterator it = this->dvb_fifos.begin();
	    it != this->dvb_fifos.end(); ++it)
	{
		delete (*it).second;
	}
	this->dvb_fifos.clear();

	this->terminal_affectation.clear();
}


bool SpotDownward::onInit(void)
{
	// Get the carrier Ids
	if(!this->initCarrierIds())
	{
		LOG(this->log_init_channel, LEVEL_ERROR,
		    "failed to complete the carrier IDs part of the "
		    "initialisation\n");
		return false;
	}
	
	if(!this->initFifo())
	{
		LOG(this->log_init_channel, LEVEL_ERROR,
		    "failed to complete the FIFO part of the "
		    "initialisation\n");
		return false;
	}

	if(!this->initMode())
	{
		LOG(this->log_init_channel, LEVEL_ERROR,
		    "failed to complete the mode part of the "
		    "initialisation\n");
		return false;
	}

	this->initStatsTimer(this->fwd_down_frame_duration_ms);

	if(!this->initRequestSimulation())
	{
		LOG(this->log_init_channel, LEVEL_ERROR,
		    "failed to complete the request simulation part of "
		    "the initialisation\n");
		goto release_dama;
	}

	// get and launch the dama algorithm
	if(!this->initDama())
	{
		LOG(this->log_init_channel, LEVEL_ERROR,
		    "failed to complete the DAMA part of the "
		    "initialisation\n");
		goto release_dama;
	}


	if(!this->initOutput())
	{
		LOG(this->log_init_channel, LEVEL_ERROR,
		    "failed to complete the initialization of "
		    "statistics\n");
		goto release_dama;
	}
	// everything went fine
	return true;

release_dama:
	delete this->dama_ctrl;
	return false;
}

bool SpotDownward::initCarrierIds(void)
{
	ConfigurationList carrier_list ;
	ConfigurationList::iterator iter;
	ConfigurationList::iterator iter_spots;
	ConfigurationList current_gw;
	
	if(!OpenSandConf::getSpot(SATCAR_SECTION,
		                      this->spot_id, 
		                      this->mac_id, current_gw))
	{
		LOG(this->log_init_channel, LEVEL_ERROR,
		    "section '%s', missing spot for id %d and gw id %d\n",
		    SATCAR_SECTION, this->spot_id, this->mac_id);
		return false;
	}
	
	// get satellite channels from configuration
	if(!Conf::getListItems(current_gw, CARRIER_LIST, carrier_list))
	{
		LOG(this->log_init_channel, LEVEL_ERROR,
		    "section '%s, %s': missing satellite channels\n",
		    SATCAR_SECTION, CARRIER_LIST);
		return false;
	}

	for(iter = carrier_list.begin(); iter != carrier_list.end(); ++iter)
	{
		unsigned int carrier_id;
		string carrier_type;
		// Get the carrier id
		if(!Conf::getAttributeValue(iter, CARRIER_ID, carrier_id))
		{
			LOG(this->log_init_channel, LEVEL_ERROR,
			    "section '%s/%s%d/%s': missing parameter '%s'\n",
			    SATCAR_SECTION, SPOT_LIST, this->spot_id,
			    CARRIER_LIST, CARRIER_ID);
			return false;
		}

		// Get the carrier type
		if(!Conf::getAttributeValue(iter, CARRIER_TYPE, carrier_type))
		{
			LOG(this->log_init_channel, LEVEL_ERROR,
			    "section '%s/%s%d/%s': missing parameter '%s'\n",
			    SATCAR_SECTION, SPOT_LIST, this->spot_id,
			    CARRIER_LIST, CARRIER_TYPE);
			return false;
		}

		if(strcmp(carrier_type.c_str(), CTRL_IN) == 0)
		{
			this->ctrl_carrier_id = carrier_id;
			this->sof_carrier_id = carrier_id;
		}
		else if(strcmp(carrier_type.c_str(), DATA_IN_GW) == 0)
		{
			this->data_carrier_id = carrier_id;
		}
	}

	// Check carrier errors

	// Control carrier error
	if(this->ctrl_carrier_id == 0)
	{
		LOG(this->log_init_channel, LEVEL_ERROR,
		    "SF#%u %s missing from section %s/%s%d\n",
		    this->super_frame_counter,
		    DVB_CAR_ID_CTRL, SATCAR_SECTION,
		    SPOT_LIST, this->spot_id);
		return false;
	}

	// Logon carrier error
	if(this->sof_carrier_id == 0)
	{
		LOG(this->log_init_channel, LEVEL_ERROR,
		    "SF#%u %s missing from section %s/%s%d\n",
		    this->super_frame_counter,
		    DVB_SOF_CAR, SATCAR_SECTION,
		    SPOT_LIST, this->spot_id);
		return false;
	}

	// Data carrier error
	if(this->data_carrier_id == 0)
	{
		LOG(this->log_init_channel, LEVEL_ERROR,
		    "SF#%u %s missing from section %s/%s%d\n",
		    this->super_frame_counter,
		    DVB_CAR_ID_DATA, SATCAR_SECTION,
		    SPOT_LIST, this->spot_id);
		return false;
	}

	LOG(this->log_init_channel, LEVEL_NOTICE,
	    "SF#%u: carrier IDs for Ctrl = %u, Sof = %u, "
	    "Data = %u\n", this->super_frame_counter,
	    this->ctrl_carrier_id,
	    this->sof_carrier_id, this->data_carrier_id);

	return true;
}


bool SpotDownward::initFifo(void)
{
	ConfigurationList fifo_list;
	ConfigurationList::iterator iter;
	ConfigurationList::iterator iter_spots;
	ConfigurationList spot_node;

	if(!OpenSandConf::getSpot(DVB_NCC_SECTION,
		                      this->spot_id, 
		                      NO_GW, spot_node))
	{
		LOG(this->log_init_channel, LEVEL_ERROR,
		    "section '%s', missing spot for id %d\n",
		    DVB_NCC_SECTION, this->spot_id);
		return false;
	}
	
	/*
	 * Read the MAC queues configuration in the configuration file.
	 * Create and initialize MAC FIFOs
	 */
	if(!Conf::getListItems(spot_node,
	                       FIFO_LIST, 
	                       fifo_list))
	{
		LOG(this->log_init_channel, LEVEL_ERROR,
		    "section '%s, %s': missing fifo list\n", DVB_NCC_SECTION,
		    FIFO_LIST);
		goto err_fifo_release;
	}

	for(iter = fifo_list.begin(); iter != fifo_list.end(); ++iter)
	{
		unsigned int fifo_priority;
		vol_pkt_t fifo_size = 0;
		string fifo_name;
		string fifo_access_type;
		DvbFifo *fifo;

		// get fifo_id --> fifo_priority
		if(!Conf::getAttributeValue(iter, FIFO_PRIO, fifo_priority))
		{
			LOG(this->log_init_channel, LEVEL_ERROR,
			    "cannot get %s from section '%s, %s'\n",
			    FIFO_PRIO, DVB_NCC_SECTION, FIFO_LIST);
			goto err_fifo_release;
		}
		// get fifo_name
		if(!Conf::getAttributeValue(iter, FIFO_NAME, fifo_name))
		{
			LOG(this->log_init_channel, LEVEL_ERROR,
			    "cannot get %s from section '%s, %s'\n",
			    FIFO_NAME, DVB_NCC_SECTION, FIFO_LIST);
			goto err_fifo_release;
		}
		// get fifo_size
		if(!Conf::getAttributeValue(iter, FIFO_SIZE, fifo_size))
		{
			LOG(this->log_init_channel, LEVEL_ERROR,
			    "cannot get %s from section '%s, %s'\n",
			    FIFO_SIZE, DVB_NCC_SECTION, FIFO_LIST);
			goto err_fifo_release;
		}
		// get the fifo CR type
		if(!Conf::getAttributeValue(iter, FIFO_ACCESS_TYPE,
		                            fifo_access_type))
		{
			LOG(this->log_init_channel, LEVEL_ERROR,
			    "cannot get %s from section '%s, %s'\n",
			    FIFO_ACCESS_TYPE, DVB_NCC_SECTION,
			    FIFO_LIST);
			goto err_fifo_release;
		}

		fifo = new DvbFifo(fifo_priority, fifo_name,
		                   fifo_access_type, fifo_size);

		LOG(this->log_init_channel, LEVEL_NOTICE,
		    "Fifo priority = %u, FIFO name %s, size %u, "
		    "access type %d\n",
		    fifo->getPriority(),
		    fifo->getName().c_str(),
		    fifo->getMaxSize(),
		    fifo->getAccessType());

		// the default FIFO is the last one = the one with the smallest
		// priority actually, the IP plugin should add packets in the
		// default FIFO if the DSCP field is not recognize, default_fifo_id
		// should not be used this is only used if traffic categories
		// configuration and fifo configuration are not coherent.
		this->default_fifo_id = std::max(this->default_fifo_id,
		                                 fifo->getPriority());

		this->dvb_fifos.insert(pair<unsigned int, DvbFifo *>(fifo->getPriority(),
		                                                     fifo));
	} // end for(queues are now instanciated and initialized)

	return true;

err_fifo_release:
	for(fifos_t::iterator it = this->dvb_fifos.begin();
	    it != this->dvb_fifos.end(); ++it)
	{
		delete (*it).second;
	}
	this->dvb_fifos.clear();
	return false;
}

bool SpotDownward::initRequestSimulation(void)
{
	ConfigurationList current_gw;
	string str_config;
	
	if(!OpenSandConf::getSpot(DVB_NCC_SECTION,
		                      this->spot_id, 
		                      NO_GW, current_gw))
	{
		LOG(this->log_init_channel, LEVEL_ERROR,
		    "section '%s', missing spot for id %d\n",
		    DVB_NCC_SECTION, spot_id);
		return false;
	}
	
	// Get and open the event file
	if(!Conf::getValue(current_gw, DVB_SIMU_MODE, str_config))
	{
		LOG(this->log_init_channel, LEVEL_ERROR,
		    "cannot load parameter %s from section %s\n",
		    DVB_SIMU_FILE, DVB_NCC_SECTION);
		return false;
	}
	
	// TODO for stdin use FileEvent for simu_timer ?
	if(str_config == "file")
	{
		this->request_simu = new FileSimulator(this->spot_id, 
		                                       this->mac_id,
		                                       this->satellite_type,
		                                       this->with_phy_layer,
		                                       &this->event_file,
		                                       this->simulate,
		                                       current_gw);
	}
	else if(str_config == "random")
	{
		this->request_simu = new RandomSimulator(this->spot_id, 
		                                         this->mac_id,
		                                         this->satellite_type,
		                                         this->with_phy_layer,
		                                         &this->event_file,
		                                         this->simulate,
		                                         current_gw);
	}
	else
	{
		LOG(this->log_init_channel, LEVEL_NOTICE,
		    "no event simulation\n");
	}

	return true;
}

bool SpotDownward::initOutput(void)
{
	// Events
	this->event_logon_resp = Output::registerEvent("Spot_%d.DVB.logon_response",
	                                               this->spot_id);

	for(fifos_t::iterator it = this->dvb_fifos.begin();
	    it != this->dvb_fifos.end(); ++it)
	{
		const char *fifo_name = ((*it).second)->getName().data();
		unsigned int id = (*it).first;

		this->probe_gw_queue_size[id] =
			Output::registerProbe<int>("Packets", true, SAMPLE_LAST,
		                               "Spot_%d.Queue size.packets.%s",
		                               spot_id, fifo_name);
		this->probe_gw_queue_size_kb[id] =
			Output::registerProbe<int>("kbits", true, SAMPLE_LAST,
		                               "Spot_%d.Queue size.%s", spot_id, fifo_name);
		this->probe_gw_l2_to_sat_before_sched[id] =
			Output::registerProbe<int>("Kbits/s", true, SAMPLE_AVG,
		                               "Spot_%d.Throughputs.L2_to_SAT_before_sched.%s",
		                               spot_id, fifo_name);
		this->probe_gw_l2_to_sat_after_sched[id] =
			Output::registerProbe<int>("Kbits/s", true, SAMPLE_AVG,
		                               "Spot_%d.Throughputs.L2_to_SAT_after_sched.%s",
		                               spot_id, fifo_name);
		this->probe_gw_queue_loss[id] =
			Output::registerProbe<int>("Packets", true, SAMPLE_SUM,
		                               "Spot_%d.Queue loss.packets.%s",
		                               spot_id, fifo_name);
		this->probe_gw_queue_loss_kb[id] =
			Output::registerProbe<int>("Kbits/s", true, SAMPLE_SUM,
		                               "Spot_%d.Queue loss.%s",
		                               spot_id, fifo_name);
	}
	this->probe_gw_l2_to_sat_total =
		Output::registerProbe<int>("Kbits/s", true, SAMPLE_AVG,
		                           "Spot_%d.Throughputs.L2_to_SAT_after_sched.total",
		                           spot_id);

	return true;
}


bool SpotDownward::handleSalohaAcks(const list<DvbFrame *> *UNUSED(ack_frames))
{
	assert(0);
}

bool SpotDownward::handleEncapPacket(NetPacket *packet)
{
	qos_t fifo_priority = packet->getQos();
	LOG(this->log_receive_channel, LEVEL_INFO,
	    "SF#%u: store one encapsulation "
	    "packet\n", this->super_frame_counter);

	// find the FIFO associated to the IP QoS (= MAC FIFO id)
	// else use the default id
	if(this->dvb_fifos.find(fifo_priority) == this->dvb_fifos.end())
	{
		fifo_priority = this->default_fifo_id;
	}

	if(!this->pushInFifo(this->dvb_fifos[fifo_priority], packet, 0))
	{
		// a problem occured, we got memory allocation error
		// or fifo full and we won't empty fifo until next
		// call to onDownwardEvent => return
		LOG(this->log_receive_channel, LEVEL_ERROR,
		    "SF#%u: unable to store received "
		    "encapsulation packet (see previous errors)\n",
		    this->super_frame_counter);
		return false;
	}

	LOG(this->log_receive_channel, LEVEL_INFO,
	    "SF#%u: encapsulation packet is "
	    "successfully stored\n",
	    this->super_frame_counter);

	return true;
}


bool SpotDownward::handleLogonReq(const LogonRequest *logon_req)
{
	uint16_t mac = logon_req->getMac();

	// Inform the Dama controller (for its own context)
	if(this->dama_ctrl && !this->dama_ctrl->hereIsLogon(logon_req))
	{
		return false;
	}

	// send the corresponding event
	Output::sendEvent(this->event_logon_resp,
	                  "Logon response send to %u",
	                  mac);

	LOG(this->log_send_channel, LEVEL_DEBUG,
	    "SF#%u: logon response sent to lower layer\n",
	    this->super_frame_counter);

	return true;
}


bool SpotDownward::handleLogoffReq(const DvbFrame *dvb_frame)
{
	// TODO	Logoff *logoff = dynamic_cast<Logoff *>(dvb_frame);
	Logoff *logoff = (Logoff *)dvb_frame;

	// unregister the ST identified by the MAC ID found in DVB frame
	if(!this->delInputTerminal(logoff->getMac(), this->mac_id, this->spot_id))
	{
		LOG(this->log_receive_channel, LEVEL_ERROR,
		    "failed to delete the ST with ID %d from FMT simulation\n",
		    logoff->getMac());
		delete dvb_frame;
		return false;
	}
	if(!this->delOutputTerminal(logoff->getMac(), this->mac_id, this->spot_id))
	{
		LOG(this->log_receive_channel, LEVEL_ERROR,
		    "failed to delete the ST with ID %d from FMT simulation\n",
		    logoff->getMac());
		delete dvb_frame;
		return false;
	}

	if(this->dama_ctrl)
	{
		this->dama_ctrl->hereIsLogoff(logoff);
	}
	LOG(this->log_receive_channel, LEVEL_DEBUG,
	    "SF#%u: logoff request from %d\n",
	    this->super_frame_counter, logoff->getMac());

	delete dvb_frame;
	return true;
}



bool SpotDownward::buildTtp(Ttp *ttp)
{
	return this->dama_ctrl->buildTTP(ttp);
}

void SpotDownward::updateStatistics(void)
{
	if(!this->doSendStats())
	{
		return;
	}

	// Update stats on the GW
	if(this->dama_ctrl)
	{
		this->dama_ctrl->updateStatistics(this->stats_period_ms);
	}

	mac_fifo_stat_context_t fifo_stat;
	// MAC fifos stats
	for(fifos_t::iterator it = this->dvb_fifos.begin();
	    it != this->dvb_fifos.end(); ++it)
	{
		(*it).second->getStatsCxt(fifo_stat);
		this->l2_to_sat_total_bytes += fifo_stat.out_length_bytes;

		this->probe_gw_l2_to_sat_before_sched[(*it).first]->put(
			fifo_stat.in_length_bytes * 8.0 / this->stats_period_ms);
		this->probe_gw_l2_to_sat_after_sched[(*it).first]->put(
			fifo_stat.out_length_bytes * 8.0 / this->stats_period_ms);

		// Mac fifo stats
		this->probe_gw_queue_size[(*it).first]->put(fifo_stat.current_pkt_nbr);
		this->probe_gw_queue_size_kb[(*it).first]->put(
			fifo_stat.current_length_bytes * 8 / 1000);
		this->probe_gw_queue_loss[(*it).first]->put(fifo_stat.drop_pkt_nbr);
		this->probe_gw_queue_loss_kb[(*it).first]->put(fifo_stat.drop_bytes * 8);
	}

	this->probe_gw_l2_to_sat_total->put(this->l2_to_sat_total_bytes * 8 /
	                                    this->stats_period_ms);

	this->l2_to_sat_total_bytes = 0;
}


bool SpotDownward::checkDama()
{
	if(!this->dama_ctrl)
	{
		// stop here
		return true;
	}
	return false;
}


bool SpotDownward::handleFrameTimer(time_sf_t super_frame_counter)
{
	// Upate the superframe counter
	this->super_frame_counter = super_frame_counter;

	if(this->with_phy_layer)
	{
		// for each terminal in DamaCtrl update FMT because in
		// this case this it not done with scenario timer and
		// FMT is updated each received frame but we only need
		// it for allocation
		this->dama_ctrl->updateFmt();
	}

	// run the allocation algorithms (DAMA)
	this->dama_ctrl->runOnSuperFrameChange(this->super_frame_counter);
	
	list<DvbFrame *> msgs;
	list<DvbFrame *>::iterator msg;
	
	// handle simulated terminals
	if(!this->request_simu)
	{
		return true;
	}

	if(!this->request_simu->simulation(&msgs, this->super_frame_counter))
	{
		this->request_simu->stopSimulation();

		LOG(this->log_request_simulation, LEVEL_ERROR,
		    "failed to simulate");
		return false;
	}

	for(msg = msgs.begin(); msg != msgs.end(); ++msg)	
	{
		uint8_t msg_type = (*msg)->getMessageType();
		switch(msg_type)
		{
			case MSG_TYPE_SAC:
			{
				LOG(this->log_request_simulation, LEVEL_INFO,
				    "simulate message type SAC");
				
				Sac *sac = (Sac*)(*msg);
				if(!this->dama_ctrl->hereIsSAC(sac))
				{
					return false;
				}
				break;
			}
			case MSG_TYPE_SESSION_LOGON_REQ:
			{
				LOG(this->log_request_simulation, LEVEL_INFO,
				    "simulate message session logon request");
				
				LogonRequest *logon_req = (LogonRequest*)(*msg);
				tal_id_t st_id = logon_req->getMac();
				
				// check for column in FMT simulation list
				if(!this->addInputTerminal(st_id, this->mac_id, this->spot_id))
				{
					LOG(this->log_request_simulation, LEVEL_ERROR,
					    "failed to register simulated ST with MAC "
					    "ID %u\n", st_id);
					return false;
				}
				if(!this->addOutputTerminal(st_id, this->mac_id, this->spot_id))
				{
					LOG(this->log_request_simulation, LEVEL_ERROR,
					    "failed to register simulated ST with MAC "
					    "ID %u\n", st_id);
					return false;
				}
				if(!this->dama_ctrl->hereIsLogon(logon_req))
				{
					return false;
				}
				break;
			}
			case MSG_TYPE_SESSION_LOGOFF:
			{
				LOG(this->log_request_simulation, LEVEL_INFO,
				    "simulate message logoff");
				
				// TODO remove Terminals
				Logoff *logoff = (Logoff*)(*msg);
				if(!this->dama_ctrl->hereIsLogoff(logoff))
				{
					return false;
				}
				break;
			}
			default:
				LOG(this->log_request_simulation, LEVEL_WARNING,
					"default");
				break;
		}
	}
	
	return true;
}


bool SpotDownward::handleFwdFrameTimer(time_sf_t fwd_frame_counter)
{
	uint32_t remaining_alloc_sym = 0;
	this->fwd_frame_counter = fwd_frame_counter;
	this->updateStatistics();

	// schedule encapsulation packets
	// TODO loop on categories (see todo in initMode)
	// TODO In regenerative mode we should schedule in frame_timer ??
	if(!this->scheduling->schedule(this->fwd_frame_counter,
	                               getCurrentTime(),
	                               &this->complete_dvb_frames,
	                               remaining_alloc_sym))
	{
		LOG(this->log_send_channel, LEVEL_ERROR,
		    "failed to schedule encapsulation "
		    "packets stored in DVB FIFO\n");
		return false;
	}

	LOG(this->log_receive_channel, LEVEL_INFO,
	    "SF#%u: %u symbols remaining after "
	    "scheduling\n", this->super_frame_counter,
	    remaining_alloc_sym);

	return true;

}

bool SpotDownward::applyPepCommand(PepRequest *pep_request)
{
	if(this->dama_ctrl->applyPepCommand(pep_request))
	{
		LOG(this->log_receive_channel, LEVEL_NOTICE,
		    "PEP request successfully "
		    "applied in DAMA\n");
	}
	else
	{
		LOG(this->log_receive_channel, LEVEL_ERROR,
		    "failed to apply PEP request "
		    "in DAMA\n");
		return false;
	}

	return true;
}

void SpotDownward::updateFmt(void)
{
	if(!this->dama_ctrl)
	{
		// stop here
		return;
	}
	// TODO FMT in slotted aloha should be handled on ST
	//  => so remove return fmt simu !
	//  => keep this todo in order to think of it on ST

	// for each terminal in DamaCtrl update FMT
	this->dama_ctrl->updateFmt();
}

double SpotDownward::getCni(void) const
{
	return this->cni;
}

uint8_t SpotDownward::getCtrlCarrierId(void) const
{
	return this->ctrl_carrier_id;
}

uint8_t SpotDownward::getSofCarrierId(void) const
{
	return this->sof_carrier_id;
}

uint8_t SpotDownward::getDataCarrierId(void) const
{
	return this->data_carrier_id;
}

list<DvbFrame *> &SpotDownward::getCompleteDvbFrames(void)
{
	return this->complete_dvb_frames;
}

event_id_t SpotDownward::getPepCmdApplyTimer(void)
{
	return this->pep_cmd_apply_timer;
}

void SpotDownward::setPepCmdApplyTimer(event_id_t pep_cmd_a_timer)
{
	this->pep_cmd_apply_timer = pep_cmd_a_timer;
}

event_id_t SpotDownward::getAcmTimer(void)
{
	return this->acm_timer;
}

void SpotDownward::setAcmTimer(event_id_t new_acm_timer)
{
	this->acm_timer = new_acm_timer;
}

bool SpotDownward::handleSac(const DvbFrame *dvb_frame)
{
	Sac *sac = (Sac *)dvb_frame;

	if(!this->dama_ctrl->hereIsSAC(sac))
	{
		LOG(this->log_receive_channel, LEVEL_ERROR,
		    "failed to handle SAC frame\n");
		delete dvb_frame;
		return false;
	}

	return true;
}


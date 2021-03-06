/***
    This file is part of snapcast
    Copyright (C) 2015  Johannes Pohl

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
***/

#include <iostream>
#include <boost/lexical_cast.hpp>
#include <boost/program_options.hpp>

#include "common/daemon.h"
#include "common/log.h"
#include "common/signalHandler.h"
#include "controller.h"
#include "alsaPlayer.h"
#include "browseAvahi.h"


using namespace std;
namespace po = boost::program_options;

bool g_terminated = false;

PcmDevice getPcmDevice(const std::string& soundcard)
{
	vector<PcmDevice> pcmDevices = Player::pcm_list();
	int soundcardIdx = -1;

	try
	{
		soundcardIdx = boost::lexical_cast<int>(soundcard);
		for (auto dev: pcmDevices)
			if (dev.idx == soundcardIdx)
				return dev;
	}
	catch(...)
	{
	}

	for (auto dev: pcmDevices)
		if (dev.name.find(soundcard) != string::npos)
			return dev;

	PcmDevice pcmDevice;
	return pcmDevice;
}


int main (int argc, char *argv[])
{
	string soundcard;
	string ip;
//	int bufferMs;
	size_t port;
	double latency;
	string serviceName;
	bool runAsDaemon;
	bool listPcmDevices;

	po::options_description desc("Allowed options");
	desc.add_options()
	("help,h", "produce help message")
	("version,v", "show version number")
	("list,l", po::bool_switch(&listPcmDevices)->default_value(false), "list pcm devices")
	("ip,i", po::value<string>(&ip), "server IP")
	("port,p", po::value<size_t>(&port)->default_value(98765), "server port")
	("soundcard,s", po::value<string>(&soundcard)->default_value("default"), "index or name of the soundcard")
	("daemon,d", po::bool_switch(&runAsDaemon)->default_value(false), "daemonize")
	("latency", po::value<double>(&latency)->default_value(0), "latency")
	("serviceName,n", po::value<string>(&serviceName)->default_value("SnapCast"), "Zone/ServiceName")
	;

	po::variables_map vm;
	po::store(po::parse_command_line(argc, argv, desc), vm);
	po::notify(vm);

	std::clog.rdbuf(new Log("snapclient", LOG_DAEMON));

	if (vm.count("help"))
	{
		logO << desc << "\n";
		return 1;
	}

	if (vm.count("version"))
	{
		logO << "snapclient v" << VERSION << "\n"
			 << "Copyright (C) 2014 BadAix (snapcast@badaix.de).\n"
			 << "License GPLv3+: GNU GPL version 3 or later <http://gnu.org/licenses/gpl.html>.\n"
			 << "This is free software: you are free to change and redistribute it.\n"
			 << "There is NO WARRANTY, to the extent permitted by law.\n\n"
			 << "Written by Johannes M. Pohl.\n";
		return 1;
	}

	if (listPcmDevices)
	{
		vector<PcmDevice> pcmDevices = Player::pcm_list();
		for (auto dev: pcmDevices)
		{
			logO << dev.idx << ": " << dev.name << "\n"
				 << dev.description << "\n\n";
		}
		return 1;
	}

    signal(SIGHUP, signal_handler);
    signal(SIGTERM, signal_handler);
    signal(SIGINT, signal_handler);

	if (runAsDaemon)
	{
		daemonize("/var/run/snapclient.pid");
		logS(kLogNotice) << "daemon started" << std::endl;
	}

	PcmDevice pcmDevice = getPcmDevice(soundcard);
	if (pcmDevice.idx == -1)
	{
		logO << "soundcard \"" << soundcard << "\" not found\n";
		return 1;
	}

	if (!vm.count("ip"))
	{
		BrowseAvahi browseAvahi;
		AvahiResult avahiResult;
		while (!g_terminated)
		{
			if (browseAvahi.browse("_snapcast._tcp", AVAHI_PROTO_INET, avahiResult, 5000))
			{
				if (avahiResult.serviceName_.compare(serviceName) == 0)
				{
					ip = avahiResult.ip_;
					port = avahiResult.port_;
					std::cout << ip << ":" << port << "\n";
					break;
				}
				else
				{
					std::cout << "Found Snapcast service but Name/Zone wrong\n";
					usleep(1000*1000);
				}
			}
		}
	}

	Controller controller;
	if (!g_terminated)
	{
		controller.start(pcmDevice, ip, port, latency);
		while(!g_terminated)
			usleep(100*1000);
		controller.stop();
	}
	daemonShutdown();

	return 0;
}



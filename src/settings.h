//
// Created by Mitch on 10/04/2020.
//

#ifndef BEAMMP_SERVER_SETTINGS_H
#define BEAMMP_SERVER_SETTINGS_H

// This is a declaration of your variable, which tells the linker this value
// is found elsewhere.  Anyone who wishes to use it must include settings.h,
// either directly or indirectly.
static string MapName = "";
static string Private = "false";
static string UUID = "";
static int PlayerCount = 0;
static int MaxPlayers = 10;
static int UDPPort = 0;
static int TCPPort = 0;
static string ServerName = "BeamMP Server";
static string Resource = "/Resources";
static string ServerVersion = "0.1";
static string ClientVersion = "0.21";

#endif //BEAMMP_SERVER_SETTINGS_H

/*%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

 Module:       FGInputSocket.cpp
 Author:       Paul Chavent
 Date started: 01/20/15
 Purpose:      Manage input of sim parameters to a socket
 Called by:    FGInput

 ------------- Copyright (C) 2015 Paul Chavent -------------

 This program is free software; you can redistribute it and/or modify it under
 the terms of the GNU Lesser General Public License as published by the Free Software
 Foundation; either version 2 of the License, or (at your option) any later
 version.

 This program is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for more
 details.

 You should have received a copy of the GNU Lesser General Public License along with
 this program; if not, write to the Free Software Foundation, Inc., 59 Temple
 Place - Suite 330, Boston, MA  02111-1307, USA.

 Further information about the GNU Lesser General Public License can also be found on
 the world wide web at http://www.gnu.org.

FUNCTIONAL DESCRIPTION
--------------------------------------------------------------------------------
This is the place where you create input routines to dump data for perusal
later.

HISTORY
--------------------------------------------------------------------------------
01/20/15   PC   Created

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
INCLUDES
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%*/

#include <cstring>
#include <cstdlib>
#include <sstream>
#include <iomanip>

#include "FGInputSocket.h"
#include "FGFDMExec.h"
#include "models/FGAircraft.h"
#include "models/FGGroundReactions.h"
#include "input_output/FGXMLElement.h"
#include "input_output/string_utilities.h"
#include "initialization/FGInitialCondition.h"
#include "models/FGPropagate.h"

using namespace std;

namespace JSBSim {

/*%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
CLASS IMPLEMENTATION
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%*/

FGInputSocket::FGInputSocket(FGFDMExec* fdmex) :
  FGInputType(fdmex), socket(0), SockProtocol(FGfdmSocket::ptTCP),
  BlockingInput(false)
{
}

//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

FGInputSocket::~FGInputSocket()
{
  delete socket;
}

//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

bool FGInputSocket::Load(Element* el)
{
  if (!FGInputType::Load(el))
    return false;

  SockPort = atoi(el->GetAttributeValue("port").c_str());

  if (SockPort == 0) {
    cerr << endl << "No port assigned in input element" << endl;
    return false;
  }

  string action = el->GetAttributeValue("action");
  if (to_upper(action) == "BLOCKING_INPUT")
    BlockingInput = true;

  return true;
}

//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

bool FGInputSocket::InitModel(void)
{
  if (FGInputType::InitModel()) {
    delete socket;
    socket = new FGfdmSocket(SockPort, SockProtocol);

    if (socket == 0) return false;
    if (!socket->GetConnectStatus()) return false;

    return true;
  }

  return false;
}

//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

void FGInputSocket::Read(bool Holding)
{
  if (!socket) return;
  if (!socket->GetConnectStatus()) return;

  if (BlockingInput)
    socket->WaitUntilReadable(); // block until a transmission is received

  string raw_data = socket->Receive(); // read data

  if (!raw_data.empty()) {
    size_t start = 0;

    data += raw_data;

    // parse lines
    while (1) {
      size_t string_start = data.find_first_not_of("\r\n", start);
      if (string_start == string::npos) break;
      size_t string_end = data.find_first_of("\r\n", string_start);
      if (string_end == string::npos) break;
      string line = data.substr(string_start, string_end-string_start);
      if (line.empty()) break;

      // now parse individual line
      vector <string> tokens = split(line,' ');

      string command, argument, str_value;
      if (!tokens.empty()) {
        command = to_lower(tokens[0]);
        if (tokens.size() > 1) {
          argument = trim(tokens[1]);
          if (tokens.size() > 2) {
            str_value = trim(tokens[2]);
          }
        }
      }

      if (command == "set") {                       // SET PROPERTY
        SGPropertyNode* node = nullptr;

        if (argument.empty()) {
          socket->Reply("No property argument supplied.\r\n");
          break;
        }
        try {
          node = PropertyManager->GetNode(argument);
        } catch(...) {
          socket->Reply("Badly formed property query\r\n");
          break;
        }

        if (!node) {
          socket->Reply("Unknown property\r\n");
          break;
        } else if (!node->hasValue()) {
          socket->Reply("Not a leaf property\r\n");
          break;
        } else {
          try {
            double value = atof_locale_c(str_value);
            node->setDoubleValue(value);
          } catch(InvalidNumber& e) {
            string msg(e.what());
            msg += "\r\n";
            socket->Reply(msg);
            break;
          }
        }
        socket->Reply("set successful\r\n");

      } else if (command == "get") {             // GET PROPERTY
        SGPropertyNode* node = nullptr;

        if (argument.empty()) {
          socket->Reply("No property argument supplied.\r\n");
          break;
        }
        try {
          node = PropertyManager->GetNode(argument);
        } catch(...) {
          socket->Reply("Badly formed property query\r\n");
          break;
        }

        if (!node) {
          socket->Reply("Unknown property\r\n");
          break;
        } else if (!node->hasValue()) {
          if (Holding) { // if holding can query property list
            string query = FDMExec->QueryPropertyCatalog(argument, "\r\n");
            socket->Reply(query);
          } else {
            socket->Reply("Must be in HOLD to search properties\r\n");
          }
        } else {
          ostringstream buf;
          buf << argument << " = " << setw(12) << setprecision(6) << node->getDoubleValue() << '\r' << endl;
          socket->Reply(buf.str());
        }

      } else if (command == "hold") {               // PAUSE

        FDMExec->Hold();
        socket->Reply("Holding\r\n");

      } else if (command == "resume") {             // RESUME

        FDMExec->Resume();
        socket->Reply("Resuming\r\n");

      } else if (command == "iterate") {            // ITERATE

        int argumentInt;
        istringstream (argument) >> argumentInt;
        if (argument.empty()) {
          socket->Reply("No argument supplied for number of iterations.\r\n");
          break;
        }
        if ( !(argumentInt > 0) ){
          socket->Reply("Required argument must be a positive Integer.\r\n");
          break;
        }
        FDMExec->EnableIncrementThenHold( argumentInt );
        FDMExec->Resume();
        socket->Reply("Iterations performed\r\n");

      } else if (command == "quit") {               // QUIT

        // close the socket connection
        socket->Send("Closing connection\r\n");
        socket->Close();

      } else if (command == "info") {               // INFO

        // get info about the sim run and/or aircraft, etc.
        ostringstream info;
        info << "JSBSim version: " << JSBSim_version << "\r\n";
        info << "Config File version: " << needed_cfg_version << "\r\n";
        info << "Aircraft simulated: " << FDMExec->GetAircraft()->GetAircraftName() << "\r\n";
        info << "Simulation time: " << setw(8) << setprecision(3) << FDMExec->GetSimTime() << '\r' << endl;
        socket->Reply(info.str());

      } else if (command == "help") {               // HELP

        socket->Reply(
        " JSBSim Server commands:\r\n\r\n"
        "   get {property name}\r\n"
        "   set {property name} {value}\r\n"
        "   hold\r\n"
        "   resume\r\n"
        "   iterate {value}\r\n"
        "   help\r\n"
        "   quit\r\n"
        "   info\r\n"
        "   reset_ic {complete|state}\r\n\r\n");

      } else if (command == "reset_ic") {               // RESET IC

        string mode = argument.empty() ? "complete" : to_lower(argument);

        if (mode == "complete") {
          // Full initialization - replicates RunIC() flow but sources position, orientation and velocities from current properties

          // Position properties
          SGPropertyNode* latNode = PropertyManager->GetNode("ic/lat-gc-deg");
          SGPropertyNode* lonNode = PropertyManager->GetNode("ic/long-gc-deg");
          SGPropertyNode* hAglNode = PropertyManager->GetNode("ic/h-agl-ft");
          SGPropertyNode* hSlNode = PropertyManager->GetNode("ic/h-sl-ft");
          SGPropertyNode* terrainNode = PropertyManager->GetNode("ic/terrain-elevation-ft");

          // Orientation properties
          SGPropertyNode* psiNode = PropertyManager->GetNode("ic/psi-true-deg");
          SGPropertyNode* thetaNode = PropertyManager->GetNode("ic/theta-deg");
          SGPropertyNode* phiNode = PropertyManager->GetNode("ic/phi-deg");

          // Read property values
          double lat_prop = latNode ? latNode->getDoubleValue() : 0.0;
          double lon_prop = lonNode ? lonNode->getDoubleValue() : 0.0;
          double h_agl_prop = hAglNode ? hAglNode->getDoubleValue() : 0.0;
          double h_sl_prop = hSlNode ? hSlNode->getDoubleValue() : 0.0;
          double terrain_prop = terrainNode ? terrainNode->getDoubleValue() : 0.0;
          double psi_prop = psiNode ? psiNode->getDoubleValue() : 0.0;
          double theta_prop = thetaNode ? thetaNode->getDoubleValue() : 0.0;
          double phi_prop = phiNode ? phiNode->getDoubleValue() : 0.0;

          // Read IC values before update for debug
          double lat_before = FDMExec->GetIC()->GetLatitudeDegIC();
          double lon_before = FDMExec->GetIC()->GetLongitudeDegIC();
          double psi_before = FDMExec->GetIC()->GetPsiDegIC();
          double theta_before = FDMExec->GetIC()->GetThetaDegIC();

          // Update IC object with position properties (overriding reset.xml defaults)
          if (latNode && latNode->hasValue()) {
            FDMExec->GetIC()->SetLatitudeDegIC(lat_prop);
          }
          if (lonNode && lonNode->hasValue()) {
            FDMExec->GetIC()->SetLongitudeDegIC(lon_prop);
          }
          if (hAglNode && hAglNode->hasValue()) {
            FDMExec->GetIC()->SetAltitudeAGLFtIC(h_agl_prop);
          }
          if (hSlNode && hSlNode->hasValue()) {
            FDMExec->GetIC()->SetAltitudeASLFtIC(h_sl_prop);
          }
          if (terrainNode && terrainNode->hasValue()) {
            FDMExec->GetIC()->SetTerrainElevationFtIC(terrain_prop);
          }

          // Update IC object with orientation properties (overriding reset.xml defaults)
          if (psiNode && psiNode->hasValue()) {
            FDMExec->GetIC()->SetPsiDegIC(psi_prop);
          }
          if (thetaNode && thetaNode->hasValue()) {
            FDMExec->GetIC()->SetThetaDegIC(theta_prop);
          }
          if (phiNode && phiNode->hasValue()) {
            FDMExec->GetIC()->SetPhiDegIC(phi_prop);
          }

          // Zero body frame velocities to ensure clean catapult start (overriding any stale values)
          FDMExec->GetIC()->SetUBodyFpsIC(0.0);
          FDMExec->GetIC()->SetVBodyFpsIC(0.0);
          FDMExec->GetIC()->SetWBodyFpsIC(0.0);

          // Debug: Read IC values after update
          double lat_after = FDMExec->GetIC()->GetLatitudeDegIC();
          double lon_after = FDMExec->GetIC()->GetLongitudeDegIC();
          double psi_after_update = FDMExec->GetIC()->GetPsiDegIC();
          double theta_after_update = FDMExec->GetIC()->GetThetaDegIC();

          // Run standard JSBSim initialization sequence (same as RunIC())
          FDMExec->SuspendIntegration(); // Set dt=0 for initialization runs

          // Apply updated IC values to propagate state
          FDMExec->GetPropagate()->SetInitialState(FDMExec->GetIC().get());

          // Reset ground reactions to clear stale WOW, compressLength, compressSpeed values
          FDMExec->GetGroundReactions()->InitModel();

          // Run twice with dt=0 to update all model states and resolve inter-model dependencies
          FDMExec->Run();
          FDMExec->Run();

          // Clear derivative history to prevent stale values from affecting new trajectory
          FDMExec->GetPropagate()->InitializeDerivatives();

          FDMExec->ResumeIntegration(); // Restore dt

          // Debug output - now includes position
          cerr << "reset_ic complete: prop lat=" << lat_prop << " lon=" << lon_prop
               << " psi=" << psi_prop << " theta=" << theta_prop
               << " | IC before: lat=" << lat_before << " lon=" << lon_before
               << " psi=" << psi_before << " theta=" << theta_before
               << " | IC after: lat=" << lat_after << " lon=" << lon_after
               << " psi=" << psi_after_update << " theta=" << theta_after_update << endl;

          socket->Reply("Initial conditions reset (complete)\r\n");
        } else if (mode == "state") {
          // State-only update - applies orientation and velocity

          auto propagate = FDMExec->GetPropagate();

          // Get Local-to-Body quaternion from IC
          FGQuaternion qLocal = FDMExec->GetIC()->GetOrientation();

          // Get ECI-to-Local transformation matrix
          const FGMatrix33& Ti2l = propagate->GetTi2l();

          // Convert to ECI-to-Body: qECI = Ti2l * qLocal
          FGQuaternion qECI = Ti2l.GetQuaternion() * qLocal;

          // Apply orientation and velocity
          propagate->SetInertialOrientation(qECI);
          propagate->SetInertialVelocity(FDMExec->GetIC()->GetUVWFpsIC());

          socket->Reply("Initial conditions applied (state)\r\n");
        } else {
          socket->Reply("Invalid reset_ic mode. Use 'complete' or 'state'\r\n");
        }

      } else {
        socket->Reply(string("Unknown command: ") + command + "\r\n");
      }

      start = string_end;
    }

    // Remove processed commands.
    size_t last_crlf = data.find_last_of("\r\n");
    if (last_crlf != string::npos) {
      if (last_crlf < data.length()-1)
        data = data.substr(last_crlf+1);
      else
        data.clear();
    }
  }

}

}

/*
 * Copyright (C) 2012-2016 Open Source Robotics Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */
#ifdef _WIN32
  #include <algorithm>
  // Ensure that Winsock2.h is included before Windows.h, which can get
  // pulled in by anybody (e.g., Boost).
  #include <Winsock2.h>
#endif

#include <functional>
#include "gazebo/common/Assert.hh"
#include "gazebo/common/CommonIface.hh"
#include "gazebo/common/Events.hh"
#include "gazebo/common/SystemPaths.hh"
#include "gazebo/transport/transport.hh"
#include "gazebo/util/DiagnosticsPrivate.hh"
#include "gazebo/util/Diagnostics.hh"

using namespace gazebo;
using namespace util;

//////////////////////////////////////////////////
DiagnosticManager::DiagnosticManager()
: dataPtr(new DiagnosticManagerPrivate)
{
#ifndef _WIN32
  const char *homePath = common::getEnv("HOME");
#else
  const char *homePath = common::getEnv("HOMEPATH");
#endif
  this->dataPtr->logPath = homePath;

  // Get the base of the time logging path
  if (!homePath)
  {
    common::SystemPaths *paths = common::SystemPaths::Instance();
    gzwarn << "HOME environment variable missing. Diagnostic timing " <<
      "information will be logged to " << paths->TmpPath() << "\n";
    this->dataPtr->logPath = paths->TmpPath() + "/gazebo";
  }
  else
  {
    this->dataPtr->logPath /= ".gazebo";
  }

  std::string timeStr = common::Time::GetWallTimeAsISOString();

#ifdef _WIN32
  std::replace(timeStr.begin(), timeStr.end(), ':', '_');
#endif

  this->dataPtr->logPath = this->dataPtr->logPath / "diagnostics" / timeStr;

  // Make sure the path exists.
  if (!boost::filesystem::exists(this->dataPtr->logPath))
    boost::filesystem::create_directories(this->dataPtr->logPath);
}

//////////////////////////////////////////////////
DiagnosticManager::~DiagnosticManager()
{
  event::Events::DisconnectWorldUpdateBegin(this->dataPtr->updateConnection);
}

//////////////////////////////////////////////////
void DiagnosticManager::Init(const std::string &_worldName)
{
  this->dataPtr->node.reset(new transport::Node());

  this->dataPtr->node->Init(_worldName);

  this->dataPtr->pub =
    this->dataPtr->node->Advertise<msgs::Diagnostics>("~/diagnostics");

  this->dataPtr->updateConnection = event::Events::ConnectWorldUpdateBegin(
      std::bind(&DiagnosticManager::Update, this, std::placeholders::_1));
}

//////////////////////////////////////////////////
boost::filesystem::path DiagnosticManager::GetLogPath() const
{
  return this->LogPath();
}

//////////////////////////////////////////////////
boost::filesystem::path DiagnosticManager::LogPath() const
{
  return this->dataPtr->logPath;
}

//////////////////////////////////////////////////
void DiagnosticManager::Update(const common::UpdateInfo &_info)
{
  if (_info.realTime > common::Time::Zero)
  {
    this->dataPtr->msg.set_real_time_factor(
        (_info.simTime / _info.realTime).Double());
  }
  else
  {
    this->dataPtr->msg.set_real_time_factor(0.0);
  }

  msgs::Set(this->dataPtr->msg.mutable_real_time(), _info.realTime);
  msgs::Set(this->dataPtr->msg.mutable_sim_time(), _info.simTime);

  if (this->dataPtr->pub && this->dataPtr->pub->HasConnections())
    this->dataPtr->pub->Publish(this->dataPtr->msg);

  this->dataPtr->msg.clear_time();
}

//////////////////////////////////////////////////
void DiagnosticManager::AddTime(const std::string &_name,
    const common::Time &_wallTime, const common::Time &_elapsedTime)
{
  msgs::Diagnostics::DiagTime *time = this->dataPtr->msg.add_time();
  time->set_name(_name);
  msgs::Set(time->mutable_elapsed(), _elapsedTime);
  msgs::Set(time->mutable_wall(), _wallTime);
}

//////////////////////////////////////////////////
void DiagnosticManager::StartTimer(const std::string &_name)
{
  TimerMap::iterator iter = this->dataPtr->timers.find(_name);
  if (iter != this->dataPtr->timers.end())
  {
    GZ_ASSERT(iter->second != nullptr, "DiagnosticTimerPtr is nullptr");
    iter->second->Start();
  }
  else
  {
    this->dataPtr->timers[_name] =
      DiagnosticTimerPtr(new DiagnosticTimer(_name));
  }
}

//////////////////////////////////////////////////
void DiagnosticManager::StopTimer(const std::string &_name)
{
  TimerMap::iterator iter = this->dataPtr->timers.find(_name);
  if (iter != this->dataPtr->timers.end())
  {
    GZ_ASSERT(iter->second, "DiagnosticTimerPtr is nullptr");
    iter->second->Stop();
  }
  else
    gzerr << "Unable to find timer[" << _name << "]\n";
}

//////////////////////////////////////////////////
void DiagnosticManager::Lap(const std::string &_name,
                            const std::string &_prefix)
{
  TimerMap::iterator iter;
  iter = this->dataPtr->timers.find(_name);

  if (iter == this->dataPtr->timers.end())
    gzerr << "Unable to find timer with name[" << _name << "]\n";
  else
  {
    GZ_ASSERT(iter->second, "DiagnosticTimerPtr is nullptr");

    iter->second->Lap(_prefix);
  }
}

//////////////////////////////////////////////////
int DiagnosticManager::GetTimerCount() const
{
  return this->TimerCount();
}

//////////////////////////////////////////////////
int DiagnosticManager::TimerCount() const
{
  return this->dataPtr->timers.size();
}

//////////////////////////////////////////////////
common::Time DiagnosticManager::GetTime(int _index) const
{
  return this->Time(_index);
}

//////////////////////////////////////////////////
common::Time DiagnosticManager::Time(const int _index) const
{
  if (_index < 0 || static_cast<size_t>(_index) > this->dataPtr->timers.size())
  {
    gzerr << "Invalid index of[" << _index << "]. Must be between 0 and "
      << this->dataPtr->timers.size()-1 << ", inclusive.\n";
    return common::Time();
  }

  TimerMap::const_iterator iter;

  iter = this->dataPtr->timers.begin();
  std::advance(iter, _index);

  if (iter != this->dataPtr->timers.end())
  {
    GZ_ASSERT(iter->second, "DiagnosticTimerPtr is nullptr");
    return iter->second->GetElapsed();
  }
  else
    gzerr << "Error getting time\n";

  return common::Time();
}

//////////////////////////////////////////////////
std::string DiagnosticManager::GetLabel(int _index) const
{
  return this->Label(_index);
}

//////////////////////////////////////////////////
std::string DiagnosticManager::Label(const int _index) const
{
  if (_index < 0 || static_cast<size_t>(_index) > this->dataPtr->timers.size())
  {
    gzerr << "Invalid index of[" << _index << "]. Must be between 0 and "
      << this->dataPtr->timers.size()-1 << ", inclusive.\n";
    return std::string();
  }
  TimerMap::const_iterator iter;

  iter = this->dataPtr->timers.begin();
  std::advance(iter, _index);

  if (iter != this->dataPtr->timers.end())
    return iter->first;
  else
    gzerr << "Erorr getting label\n";

  return "null";
}

//////////////////////////////////////////////////
common::Time DiagnosticManager::GetTime(const std::string &_label) const
{
  return this->Time(_label);
}

//////////////////////////////////////////////////
common::Time DiagnosticManager::Time(const std::string &_label) const
{
  TimerMap::const_iterator iter;
  iter = this->dataPtr->timers.find(_label);

  if (iter != this->dataPtr->timers.end())
  {
    GZ_ASSERT(iter->second, "DiagnosticTimerPtr is nullptr");
    return iter->second->GetElapsed();
  }
  else
    gzerr << "Error getting time\n";

  return common::Time();
}

//////////////////////////////////////////////////
DiagnosticTimer::DiagnosticTimer(const std::string &_name)
: Timer(),
  dataPtr(new DiagnosticTimerPrivate)
{
  boost::filesystem::path logPath;

  logPath = DiagnosticManager::Instance()->LogPath() / (_name + ".log");
  this->dataPtr->log.open(logPath.string().c_str(),
      std::ios::out | std::ios::app);

  this->dataPtr->name = _name;
  this->Start();
}

//////////////////////////////////////////////////
DiagnosticTimer::~DiagnosticTimer()
{
  this->Stop();
  this->dataPtr->log.close();
}

//////////////////////////////////////////////////
void DiagnosticTimer::Start()
{
  // Only start if not running.
  if (!this->GetRunning())
  {
    // Start the timer
    Timer::Start();

    // Make sure the prev lap is reset
    this->dataPtr->prevLap.Set(0, 0);
  }
}

//////////////////////////////////////////////////
void DiagnosticTimer::Stop()
{
  // Only stop is currently running
  if (this->GetRunning())
  {
    // Stop the timer
    Timer::Stop();

    common::Time elapsed = this->GetElapsed();
    common::Time currTime = common::Time::GetWallTime();

    // Write out the total elapsed time.
    this->dataPtr->log << this->dataPtr->name << " " << currTime << " "
      << elapsed.Double() << std::endl;
    this->dataPtr->log.flush();

    DiagnosticManager::Instance()->AddTime(this->dataPtr->name,
        currTime, elapsed);

    // Reset the lap time
    this->dataPtr->prevLap.Set(0, 0);
  }
}

//////////////////////////////////////////////////
void DiagnosticTimer::Lap(const std::string &_prefix)
{
  // Get the current elapsed time.
  common::Time elapsed = this->GetElapsed();
  common::Time delta = elapsed - this->dataPtr->prevLap;
  common::Time currTime = common::Time::GetWallTime();

  // Write out the delta time.
  this->dataPtr->log << this->dataPtr->name << ":" << _prefix << " " <<
    currTime << " " << delta.Double() << std::endl;

  DiagnosticManager::Instance()->AddTime(this->dataPtr->name + ":" + _prefix,
      currTime, delta);

  // Store the prev lap time.
  this->dataPtr->prevLap = elapsed;
}

//////////////////////////////////////////////////
const std::string DiagnosticTimer::GetName() const
{
  return this->Name();
}

//////////////////////////////////////////////////
const std::string DiagnosticTimer::Name() const
{
  return this->dataPtr->name;
}

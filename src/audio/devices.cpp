/*
    Copyright © 2014-2016 by The qTox Project

    This file is part of qTox, a Qt-based graphical interface for Tox.

    qTox is libre software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    qTox is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with qTox.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "devices.h"

#include <QDebug>
#include <QVector>

#include <RtAudio.h>

namespace qTox {
namespace Audio {

/**
@class Device
@brief Representation of a physical audio device.

@class StreamContext
@brief Stream context.
*/

/**
This callback outputs a warning on RtAudio errors.
*/
static void cb_rtaudio_error(RtAudioError::Type type, const std::string& message)
{
    Q_UNUSED(type);
    qWarning() << "RtAudio:" << QString::fromStdString(message);
}


/**
@internal
@brief Private implementation of the Device class.

Encapsulates RtAudio::DeviceInfo and provides access to the physical audio
device.
*/
class Device::Private : public QSharedData
{
public:
    Private(const RtAudio::DeviceInfo& devInfo)
        : info(devInfo)
    {
    }

public:
    RtAudio::DeviceInfo info;
};

/**
@internal
@brief Private implementation of the StreamContext class.
*/
class StreamContext::Private : public QSharedData
{
    friend class StreamContext;

public:
    Private(RtAudio::StreamParameters* inputParms = nullptr,
            RtAudio::StreamParameters* outputParams = nullptr,
            RtAudio::StreamOptions* options = nullptr)
        : format(RTAUDIO_SINT16)
        , sampleRate(44100)
        , bufferFrames(256)
        , inParams(inputParms)
        , outParams(outputParams)
        , opts(options)
    {
        audio.showWarnings(true);
    }

    bool open()
    {
        if (audio.isStreamOpen())
            return true;

        // TODO: implement and assign callbacks
        audio.openStream(outParams, inParams, format, sampleRate,
                         &bufferFrames, nullptr, nullptr, opts,
                         &cb_rtaudio_error);

        return audio.isStreamOpen();
    }

    void close()
    {
        if (!audio.isStreamOpen())
            return;

        audio.closeStream();
    }

    bool start()
    {
        if (!open())
            return false;

        audio.startStream();

        return audio.isStreamRunning();
    }

    bool stop()
    {
        if (!audio.isStreamRunning())
            return true;

        audio.stopStream();

        return !audio.isStreamRunning();
    }

private:
    RtAudio audio;

    RtAudioFormat   format;
    unsigned int    sampleRate;
    unsigned int    bufferFrames;

    RtAudio::StreamParameters*  inParams;
    RtAudio::StreamParameters*  outParams;
    RtAudio::StreamOptions*     opts;
};

/**
@brief Creates an audio stream context.
@param[in] inputDevice      the input device index; -1 == no input
@param[in] outputDevice     the output device index; -1 == no output
@return the created StreamContext or a null object, if creation failed

The created StreamContext has 1 (mono) or 2 (stereo) channels, depending on the
devices' capabilities. A device in qTox cannot have more than two channels.
*/
StreamContext create(int inputDevice, int outputDevice)
{
    if (inputDevice < 0 && outputDevice < 0)
        return nullptr;

    RtAudio audio;
    RtAudio::StreamParameters* inParams = nullptr;
    RtAudio::StreamParameters* outParams = nullptr;

    if (inputDevice >= 0)
    {
        unsigned int devId = static_cast<quint32>(inputDevice);
        RtAudio::DeviceInfo info = audio.getDeviceInfo(devId);
        inParams = new RtAudio::StreamParameters();
        (*inParams).deviceId = devId;
        (*inParams).nChannels = (info.inputChannels > 1) ? 2 : 1;
        (*inParams).firstChannel = 0;
    }

    if (outputDevice >= 0)
    {
        unsigned int devId = static_cast<quint32>(outputDevice);
        RtAudio::DeviceInfo info = audio.getDeviceInfo(devId);
        outParams = new RtAudio::StreamParameters();
        (*outParams).deviceId = devId;
        (*outParams).nChannels = (info.inputChannels > 1) ? 2 : 1;
        (*outParams).firstChannel = 0;
    }

    return new StreamContext::Private(inParams, outParams);
}

/**
@brief Creates a list of all available audio input and output devices.
@return the list of available audio devices
*/
Device::List Device::availableDevices()
{
    RtAudio audio;
    List devices;

    for (unsigned int i = 0; i < audio.getDeviceCount(); i++)
    {
        devices << new Device::Private(audio.getDeviceInfo(i));
    }

    return devices;
}

/**
@brief  Searches the available audio devices for deviceName and returns the
        index.
@param[in] deviceName   the device name
@return the device index or -1, if not found
*/
int Device::find(const QString& deviceName)
{
    RtAudio audio;
    std::string name = deviceName.toStdString();

    for (unsigned int i = 0; i < audio.getDeviceCount(); i++)
    {
        if (name == audio.getDeviceInfo(i).name)
            return static_cast<int>(i);
    }

    return -1;
}

Device::Device(Private* p)
    : d(p)
{
}

bool Device::isValid() const
{
    return d->info.probed;
}

/**
@brief Returns a QString representation of the audio device name.
@return the device name
*/
QString Device::name() const
{
    return QString::fromStdString(d->info.name);
}

/**
The maximum input channels the audio device supports.
*/
quint32 Device::inputChannels() const
{
    return d->info.inputChannels;
}

/**
The maximum output channels the audio device supports.
*/
quint32 Device::outputChannels() const
{
    return d->info.outputChannels;
}

/**
The maximum simultaneous input/output channels the audio device supports.
*/
quint32 Device::duplexChannels() const
{
    return d->info.duplexChannels;
}

/**
Convenience method.

@return true, if outputChannels() > 0
*/
bool Device::isOutput() const
{
    return d->info.outputChannels > 0;
}

/**
@brief Returns, if this is the default audio output device.
@return true, if this is the default output device
*/
bool Device::isDefaultOutput() const
{
    return d->info.isDefaultOutput;
}

/**
Convenience method.

@return true, if inputChannels() > 0
*/
bool Device::isInput() const
{
    return d->info.inputChannels > 0;
}

/**
@brief Returns, if this is the default audio input device.
@return true, if this is the default input device
*/
bool Device::isDefaultInput() const
{
    return d->info.isDefaultOutput;
}

StreamContext::StreamContext(StreamContext::Private* p)
    : d(p)
{
}

/**
@brief Opens the stream for reading and writing.
@return
*/
bool StreamContext::open()
{
    return d->open();
}

/**
Closes the audio stream.
*/
void StreamContext::close()
{
    d->close();
}

bool StreamContext::isOpen() const
{
    return d->audio.isStreamOpen();
}

bool StreamContext::isRunning() const
{
    return d->audio.isStreamRunning();
}

}
}

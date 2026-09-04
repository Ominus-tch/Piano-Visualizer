#include "VSTStateStream.h"

#include <algorithm>
#include <cstring>

namespace vst
{
    VSTStateStream::VSTStateStream()
        : _position(0),
        _refCount(1)
    {}


    VSTStateStream::VSTStateStream(
        const std::vector<uint8_t>& data)
        : _data(data),
        _position(0),
        _refCount(1)
    {}


    Steinberg::tresult PLUGIN_API
        VSTStateStream::read(
            void* buffer,
            Steinberg::int32 numBytes,
            Steinberg::int32* numBytesRead)
    {
        if (!buffer || numBytes < 0)
            return Steinberg::kInvalidArgument;

        const size_t remaining =
            _data.size() - std::min(
                _position,
                _data.size());

        const size_t bytesToRead =
            std::min(
                static_cast<size_t>(numBytes),
                remaining);

        if (bytesToRead > 0)
        {
            std::memcpy(
                buffer,
                _data.data() + _position,
                bytesToRead);

            _position += bytesToRead;
        }

        if (numBytesRead)
        {
            *numBytesRead =
                static_cast<Steinberg::int32>(
                    bytesToRead);
        }

        return bytesToRead == static_cast<size_t>(numBytes)
            ? Steinberg::kResultOk
            : Steinberg::kResultFalse;
    }


    Steinberg::tresult PLUGIN_API
        VSTStateStream::write(
            void* buffer,
            Steinberg::int32 numBytes,
            Steinberg::int32* numBytesWritten)
    {
        if (!buffer || numBytes < 0)
            return Steinberg::kInvalidArgument;

        const size_t bytes =
            static_cast<size_t>(numBytes);

        if (_position + bytes > _data.size())
        {
            _data.resize(
                _position + bytes);
        }

        std::memcpy(
            _data.data() + _position,
            buffer,
            bytes);

        _position += bytes;

        if (numBytesWritten)
        {
            *numBytesWritten =
                numBytes;
        }

        return Steinberg::kResultOk;
    }


    Steinberg::tresult PLUGIN_API
        VSTStateStream::seek(
            Steinberg::int64 pos,
            Steinberg::int32 mode,
            Steinberg::int64* result)
    {
        Steinberg::int64 newPosition = 0;

        switch (mode)
        {
        case Steinberg::IBStream::kIBSeekSet:
            newPosition = pos;
            break;

        case Steinberg::IBStream::kIBSeekCur:
            newPosition =
                static_cast<Steinberg::int64>(
                    _position) + pos;
            break;

        case Steinberg::IBStream::kIBSeekEnd:
            newPosition =
                static_cast<Steinberg::int64>(
                    _data.size()) + pos;
            break;

        default:
            return Steinberg::kInvalidArgument;
        }

        if (newPosition < 0)
            return Steinberg::kInvalidArgument;

        _position =
            static_cast<size_t>(
                newPosition);

        if (result)
        {
            *result = newPosition;
        }

        return Steinberg::kResultOk;
    }


    Steinberg::tresult PLUGIN_API
        VSTStateStream::tell(
            Steinberg::int64* pos)
    {
        if (!pos)
            return Steinberg::kInvalidArgument;

        *pos =
            static_cast<Steinberg::int64>(
                _position);

        return Steinberg::kResultOk;
    }


    Steinberg::tresult PLUGIN_API
        VSTStateStream::queryInterface(
            const Steinberg::TUID iid,
            void** obj)
    {
        if (!obj)
            return Steinberg::kInvalidArgument;

        *obj = nullptr;

        if (Steinberg::FUnknownPrivate::iidEqual(
            iid,
            Steinberg::IBStream::iid))
        {
            *obj =
                static_cast<
                Steinberg::IBStream*>(
                    this);
        }
        else if (Steinberg::FUnknownPrivate::iidEqual(
            iid,
            Steinberg::FUnknown::iid))
        {
            *obj =
                static_cast<
                Steinberg::FUnknown*>(
                    this);
        }
        else
        {
            return Steinberg::kNoInterface;
        }

        addRef();

        return Steinberg::kResultOk;
    }


    Steinberg::uint32 PLUGIN_API
        VSTStateStream::addRef()
    {
        return ++_refCount;
    }


    Steinberg::uint32 PLUGIN_API
        VSTStateStream::release()
    {
        const auto count =
            --_refCount;

        if (count == 0)
            delete this;

        return count;
    }


    const std::vector<uint8_t>&
        VSTStateStream::data() const
    {
        return _data;
    }


    void VSTStateStream::reset()
    {
        _position = 0;
    }
}
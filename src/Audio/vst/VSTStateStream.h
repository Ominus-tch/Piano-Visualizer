#pragma once

#include <vector>
#include <cstdint>
#include <cstddef>

#include "pluginterfaces/base/ibstream.h"

namespace vst
{
    class VSTStateStream
        : public Steinberg::IBStream
    {
    public:
        VSTStateStream();

        explicit VSTStateStream(
            const std::vector<uint8_t>& data);

        Steinberg::tresult PLUGIN_API read(
            void* buffer,
            Steinberg::int32 numBytes,
            Steinberg::int32* numBytesRead) override;

        Steinberg::tresult PLUGIN_API write(
            void* buffer,
            Steinberg::int32 numBytes,
            Steinberg::int32* numBytesWritten) override;

        Steinberg::tresult PLUGIN_API seek(
            Steinberg::int64 pos,
            Steinberg::int32 mode,
            Steinberg::int64* result) override;

        Steinberg::tresult PLUGIN_API tell(
            Steinberg::int64* pos) override;

        Steinberg::tresult PLUGIN_API queryInterface(
            const Steinberg::TUID iid,
            void** obj) override;

        Steinberg::uint32 PLUGIN_API addRef() override;

        Steinberg::uint32 PLUGIN_API release() override;

        const std::vector<uint8_t>& data() const;

        void reset();

    private:
        std::vector<uint8_t> _data;
        size_t _position;

        Steinberg::uint32 _refCount;
    };
}
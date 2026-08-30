#pragma once

#include "pluginterfaces/vst/ivstparameterchanges.h"

#include <vector>
#include <algorithm>

class ParamValueQueue final : public Steinberg::Vst::IParamValueQueue
{
public:
    struct Point
    {
        Steinberg::int32 sampleOffset;
        Steinberg::Vst::ParamValue value;
    };

    explicit ParamValueQueue(
        Steinberg::Vst::ParamID id)
        : _id(id)
    {
    }

    Steinberg::Vst::ParamID PLUGIN_API
        getParameterId() override
    {
        return _id;
    }

    Steinberg::int32 PLUGIN_API
        getPointCount() override
    {
        return static_cast<Steinberg::int32>(_points.size());
    }

    Steinberg::tresult PLUGIN_API
        getPoint(
            Steinberg::int32 index,
            Steinberg::int32& sampleOffset,
            Steinberg::Vst::ParamValue& value) override
    {
        if (index < 0 ||
            index >= static_cast<Steinberg::int32>(_points.size()))
        {
            return Steinberg::kResultFalse;
        }

        const Point& point = _points[index];

        sampleOffset = point.sampleOffset;
        value = point.value;

        return Steinberg::kResultTrue;
    }

    Steinberg::tresult PLUGIN_API
        addPoint(
            Steinberg::int32 sampleOffset,
            Steinberg::Vst::ParamValue value,
            Steinberg::int32& index) override
    {
        Point point;
        point.sampleOffset = sampleOffset;
        point.value = value;

        _points.push_back(point);

        index =
            static_cast<Steinberg::int32>(_points.size() - 1);

        return Steinberg::kResultTrue;
    }

    Steinberg::uint32 PLUGIN_API
        addRef() override
    {
        return ++_refCount;
    }

    Steinberg::uint32 PLUGIN_API
        release() override
    {
        const auto count = --_refCount;

        if (count == 0)
            delete this;

        return count;
    }

    Steinberg::tresult PLUGIN_API
        queryInterface(
            const Steinberg::TUID _iid,
            void** obj) override
    {
        if (!obj)
            return Steinberg::kInvalidArgument;

        *obj = nullptr;

        if (Steinberg::FUnknownPrivate::iidEqual(
            _iid,
            Steinberg::Vst::IParamValueQueue::iid))
        {
            *obj = static_cast<Steinberg::Vst::IParamValueQueue*>(this);
            addRef();
            return Steinberg::kResultTrue;
        }

        if (Steinberg::FUnknownPrivate::iidEqual(
            _iid,
            Steinberg::FUnknown::iid))
        {
            *obj = static_cast<Steinberg::FUnknown*>(this);
            addRef();
            return Steinberg::kResultTrue;
        }

        return Steinberg::kNoInterface;
    }

    void clear()
    {
        _points.clear();
    }

private:
    Steinberg::Vst::ParamID _id;
    std::vector<Point> _points;
    Steinberg::uint32 _refCount = 1;
};


class ParameterChanges final : public Steinberg::Vst::IParameterChanges
{
public:
    Steinberg::int32 PLUGIN_API
        getParameterCount() override
    {
        return static_cast<Steinberg::int32>(_queues.size());
    }

    Steinberg::Vst::IParamValueQueue* PLUGIN_API
        getParameterData(
            Steinberg::int32 index) override
    {
        if (index < 0 ||
            index >= static_cast<Steinberg::int32>(_queues.size()))
        {
            return nullptr;
        }

        return &_queues[index];
    }

    Steinberg::Vst::IParamValueQueue* PLUGIN_API
        addParameterData(
            const Steinberg::Vst::ParamID& id,
            Steinberg::int32& index) override
    {
        for (Steinberg::int32 i = 0;
            i < static_cast<Steinberg::int32>(_queues.size());
            ++i)
        {
            if (_queues[i].getParameterId() == id)
            {
                index = i;
                return &_queues[i];
            }
        }

        _queues.emplace_back(id);

        index =
            static_cast<Steinberg::int32>(_queues.size() - 1);

        return &_queues.back();
    }

    Steinberg::uint32 PLUGIN_API
        addRef() override
    {
        return ++_refCount;
    }

    Steinberg::uint32 PLUGIN_API
        release() override
    {
        const auto count = --_refCount;

        if (count == 0)
            delete this;

        return count;
    }

    Steinberg::tresult PLUGIN_API
        queryInterface(
            const Steinberg::TUID _iid,
            void** obj) override
    {
        if (!obj)
            return Steinberg::kInvalidArgument;

        *obj = nullptr;

        if (Steinberg::FUnknownPrivate::iidEqual(
            _iid,
            Steinberg::Vst::IParameterChanges::iid))
        {
            *obj =
                static_cast<Steinberg::Vst::IParameterChanges*>(this);

            addRef();

            return Steinberg::kResultTrue;
        }

        if (Steinberg::FUnknownPrivate::iidEqual(
            _iid,
            Steinberg::FUnknown::iid))
        {
            *obj =
                static_cast<Steinberg::FUnknown*>(this);

            addRef();

            return Steinberg::kResultTrue;
        }

        return Steinberg::kNoInterface;
    }

    void clear()
    {
        for (auto& queue : _queues)
            queue.clear();

        _queues.clear();
    }

private:
    std::vector<ParamValueQueue> _queues;
    Steinberg::uint32 _refCount = 1;
};
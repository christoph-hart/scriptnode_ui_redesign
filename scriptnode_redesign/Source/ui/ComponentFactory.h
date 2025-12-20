/*  ===========================================================================
*
*   This file is part of HISE.
*   Copyright 2016 Christoph Hart
*
*   HISE is free software: you can redistribute it and/or modify
*   it under the terms of the GNU General Public License as published by
*   the Free Software Foundation, either version 3 of the License, or
*   (at your option) any later version.
*
*   HISE is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*   GNU General Public License for more details.
*
*   You should have received a copy of the GNU General Public License
*   along with HISE.  If not, see <http://www.gnu.org/licenses/>.
*
*   Commercial licenses for using HISE in an closed source project are
*   available on request. Please visit the project's website to get more
*   information about commercial licensing:
*
*   http://www.hise.audio/
*
*   HISE is based on the JUCE library,
*   which must be separately licensed for closed source applications:
*
*   http://www.juce.com
*
*   ===========================================================================
*/

#pragma once

namespace scriptnode {
using namespace hise;
using namespace juce;

struct NodeResizer : public juce::ResizableCornerComponent
{
	NodeResizer(Component* c, ComponentBoundsConstrainer* constrainer=nullptr) :
		ResizableCornerComponent(c, constrainer)
	{};

	void paint(Graphics& g) override;

	void mouseDrag(const MouseEvent& e) override;

	void mouseUp(const MouseEvent& e) override;
};


namespace display_buffer_library
{

	struct BuildData
	{
		operator bool() const { return rb != nullptr; }

		ValueTree v;
		SimpleRingBuffer::Ptr rb;
		ParameterSourceObject* pSource;
		PooledUIUpdater* updater;
	};

	struct WrappedDisplayBufferComponent : public Component,
										   public PooledUIUpdater::SimpleTimer
	{
		WrappedDisplayBufferComponent(const BuildData& bd_) :
			SimpleTimer(bd_.updater, false),
			bd(bd_)
		{}

		~WrappedDisplayBufferComponent() override {}

	protected:

		struct WatchedParameter
		{
			String name;
			int index;
			double lastValue;
		};

		void addWatchedParameterIds(const StringArray& parameters)
		{
			auto ptree = bd.v.getChildWithName(PropertyIds::Parameters);

			int idx = 0;

			watchedParameters.clear();

			for (auto p : ptree)
			{
				auto pid = p[PropertyIds::ID].toString();
				if (parameters.contains(pid))
				{
					watchedParameters.push_back({ pid, idx, 0.0 });

				}


				idx++;
			}

			if (!watchedParameters.empty())
				start();
			else
				stop();
		}


		complex_ui_laf laf;
		BuildData bd;

		virtual void onParameterChange(const WatchedParameter& p)
		{
		}

	private:

		void timerCallback() override
		{
			for (auto& wp : watchedParameters)
			{
				auto lv = wp.lastValue;
				auto nv = bd.pSource->getParameterValue(wp.index);

				wp.lastValue = nv;

				if (lv != nv)
					onParameterChange(wp);
			}
		}

		std::vector<WatchedParameter> watchedParameters;

		JUCE_DECLARE_WEAK_REFERENCEABLE(WrappedDisplayBufferComponent);
	};

	struct oscillator : public WrappedDisplayBufferComponent
	{
		oscillator(const BuildData& bd) :
			WrappedDisplayBufferComponent(bd)
		{
			bd.rb->setCurrentWriter(&oscProvider);

			{
				SimpleRingBuffer::ScopedPropertyCreator sv(bd.rb.get());
				bd.rb->registerPropertyObject<OscillatorDisplayProvider::OscillatorDisplayObject>();
			}

			dp = new OscillatorDisplayProvider::osc_display();
			addAndMakeVisible(dp);
			dp->setComplexDataUIBase(bd.rb.get());
			dp->setSpecialLookAndFeel(&laf, false);

			bd.rb->getUpdater().sendDisplayChangeMessage(bd.rb->getReadBuffer().getNumSamples(), sendNotificationSync);

			addAndMakeVisible(dp);

			addWatchedParameterIds({ "Mode", "Phase" });

			setSize(Helpers::ParameterWidth, 80);
		}

		void onParameterChange(const WatchedParameter& p) override
		{
			if (p.name == "Mode")
				oscProvider.currentMode = (OscillatorDisplayProvider::Mode)(int)p.lastValue;

			if (p.name == "Phase")
				oscProvider.uiData.uptime = p.lastValue * 2048.0;

			bd.rb->getUpdater().sendDisplayChangeMessage(bd.rb->getReadBuffer().getNumSamples(), sendNotificationSync);
		}

		void resized() override
		{
			dp->setBounds(getLocalBounds());
		}

		ScopedPointer<OscillatorDisplayProvider::osc_display> dp;
		OscillatorDisplayProvider oscProvider;

		static WrappedDisplayBufferComponent* create(const BuildData& bd)
		{
			return new oscillator(bd);
		}
	};

	struct Factory
	{


		using CreateFunction = std::function<WrappedDisplayBufferComponent* (const BuildData&)>;

		Factory()
		{
		}

		WrappedDisplayBufferComponent* create(const BuildData& bd)
		{
			if (!bd)
				return nullptr;

			auto id = bd.v[PropertyIds::FactoryPath].toString();

			if (data->functions.find(id) != data->functions.end())
			{
				return data->functions.at(id)(bd);
			}

			return nullptr;
		}

		struct Data
		{
			Data()
			{
				registerFunction<display_buffer_library::oscillator>("core.oscillator");
			}

			template <typename T> void registerFunction(const String& id)
			{
				functions[id] = T::create;
			}

			std::map<String, CreateFunction> functions;
		};

		SharedResourcePointer<Data> data;
	};

}

struct DummyComplexDataProvider : public ExternalDataHolder
{
	struct Editor : public ScriptnodeExtraComponentBase,
					public ComponentBoundsConstrainer
	{
		Editor(const ComplexDataUIBase::List& dataObjects_, ParameterSourceObject* pSource_, PooledUIUpdater* updater, UndoManager* um, const ValueTree& v) :
			dataObjects(dataObjects_),
			pSource(pSource_),
			resizer(this, this)
		{
			display_buffer_library::Factory f;
			display_buffer_library::BuildData bd;

			bd.rb = dynamic_cast<SimpleRingBuffer*>(dataObjects.getFirst().get());
			bd.pSource = pSource;
			bd.v = v;
			bd.updater = updater;

			if (auto c = f.create(bd))
			{
				addAndMakeVisible(c);
				editors.add(c);
				setSize(c->getWidth(), c->getHeight());
			}
			else
			{
				int w = Helpers::ParameterWidth + Helpers::ModulationOutputWidth;
				int h = 80;

				for (auto obj : dataObjects)
				{
					obj->setUndoManager(um);
					obj->setGlobalUIUpdater(updater);
					auto ed = ExternalData::createEditor(obj);

					if (dynamic_cast<SimpleRingBuffer*>(obj) == nullptr)
					{
						w = 512;
						h = 100;
					}

					ed->setSpecialLookAndFeel(&laf, false);
					editors.add(dynamic_cast<Component*>(ed));
					addAndMakeVisible(editors.getLast());
				}

				setSize(w, dataObjects.size() * h + 15);
			}

			addAndMakeVisible(resizer);
		}

		void checkBounds(Rectangle<int>& bounds,
			const Rectangle<int>& previousBounds,
			const Rectangle<int>& limits,
			bool isStretchingTop,
			bool isStretchingLeft,
			bool isStretchingBottom,
			bool isStretchingRight) override;

		void resized() override
		{
			if(editors.isEmpty())
				return;

			auto b = getLocalBounds();

			resizer.setBounds(b.removeFromBottom(15).removeFromRight(15));

			auto h = b.getHeight() / editors.size();

			for (auto e : editors)
			{
				e->setBounds(b.removeFromTop(h));
			}
		}

		OscillatorDisplayProvider oscProvider;

		ComplexDataUIBase::List dataObjects;
		OwnedArray<Component> editors;
		complex_ui_laf laf;

		ParameterSourceObject* pSource;
		NodeResizer resizer;
	};

	int getNumDataObjects(ExternalData::DataType t) const override
	{
		switch (t)
		{
		case ExternalData::DataType::Table:
			return tables.size();
		case ExternalData::DataType::SliderPack:
			return sliderPacks.size();
		case ExternalData::DataType::AudioFile:
			return audioFiles.size();
		case ExternalData::DataType::FilterCoefficients:
			return filters.size();
		case ExternalData::DataType::DisplayBuffer:
			return displayBuffers.size();
		case ExternalData::DataType::numDataTypes:
			break;
		case ExternalData::DataType::ConstantLookUp:
			break;
		default:
			break;
		}

		return 0;
	}

	Table* getTable(int index) override
	{
		return this->get<SampleLookupTable, Table>(tables, index);
	}

	SliderPackData* getSliderPack(int index) override
	{
		return this->get<SliderPackData, SliderPackData>(sliderPacks, index);
	}

	MultiChannelAudioBuffer* getAudioFile(int index) override
	{
		return this->get<MultiChannelAudioBuffer, MultiChannelAudioBuffer>(audioFiles, index);
	}

	FilterDataObject* getFilterData(int index) override
	{
		return this->get<FilterDataObject, FilterDataObject>(filters, index);
	}

	SimpleRingBuffer* getDisplayBuffer(int index) override
	{
		auto rb = this->get<SimpleRingBuffer, SimpleRingBuffer>(displayBuffers, index);

		rb->setRingBufferSize(1, 8192);
		rb->setSamplerate(44100.0);

		AudioSampleBuffer bf(1, 8192);

		for (int i = 0; i < 8192; i++)
		{
			bf.setSample(0, i, hmath::fmod((float)((float)i / (float)1024), 1.0f));
		}

		rb->write(bf, 0, 8192);

		return rb;
	}

	bool removeDataObject(ExternalData::DataType t, int index) override
	{
		jassertfalse;
		return false;
	}

	ExternalData getFromValueTree(const ValueTree& v)
	{
		auto type = ExternalData::getDataTypeForId(v.getType());

		auto idx = (int)v[PropertyIds::Index];
		auto data = getData(type, idx);

		if (idx == -1)
		{
			auto b64 = v[PropertyIds::EmbeddedData].toString();

			if (data.obj != nullptr)
				data.obj->fromBase64String(b64);
		}

		return data;
	}

private:

	template <typename Derived, typename T> T* get(ReferenceCountedArray<T>& list, int index)
	{
		if (index == -1)
			return new Derived();

		if (isPositiveAndBelow(index, list.size()))
			return list[index].get();

		while (list.size() != (index + 1))
			list.add(nullptr);

		list.set(index, new Derived());
		return list[index].get();
	}

	ReferenceCountedArray<SliderPackData> sliderPacks;
	ReferenceCountedArray<Table> tables;
	ReferenceCountedArray<MultiChannelAudioBuffer> audioFiles;
	ReferenceCountedArray<SimpleRingBuffer> displayBuffers;
	ReferenceCountedArray<FilterDataObject> filters;

};



}
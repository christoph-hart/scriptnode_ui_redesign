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

struct DummyBody : public Component
{
	static Component* createDummyComponent(const String& path);

	DummyBody(const Image& img_);

	void paint(Graphics& g) override;

	Image img;
};


namespace UIPropertyIds
{
#define DECLARE_ID(x) static const Identifier x(#x);
DECLARE_ID(x);
DECLARE_ID(y);
DECLARE_ID(width);
DECLARE_ID(height);
DECLARE_ID(foldedWidth);
DECLARE_ID(foldedHeight);
DECLARE_ID(LockPosition);
DECLARE_ID(ParameterYOffset);
DECLARE_ID(CommentWidth);
DECLARE_ID(Comments);
DECLARE_ID(CommentOffsetX);
DECLARE_ID(CommentOffsetY);
DECLARE_ID(CableOffset);
DECLARE_ID(Groups);
DECLARE_ID(Group);
DECLARE_ID(CurrentRoot);
DECLARE_ID(HideCable);
#undef DECLARE_ID

struct Helpers
{
	static Array<Identifier> getPositionIds() { return { x, y, width, height, foldedWidth, foldedHeight }; }
};

}


struct CommentHelpers
{

	static int getCommentWidth(ValueTree data);
	static Point<int> getCommentOffset(ValueTree data);
};

namespace display_buffer_library
{

struct BuildData
{
	operator bool() const { return rb != nullptr; }

	ValueTree v;
	SimpleRingBuffer::Ptr rb;
	NodeComponentParameterSource* pSource;
	PooledUIUpdater* updater;
};

struct WrappedDisplayBufferComponent: public Component,
									  public PooledUIUpdater::SimpleTimer
{
	WrappedDisplayBufferComponent(const BuildData& bd_):
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

		for(auto p: ptree)
		{
			auto pid = p[PropertyIds::ID].toString();
			if(parameters.contains(pid))
			{
				watchedParameters.push_back({ pid, idx, 0.0 });
				
			}
				

			idx++;
		}

		if(!watchedParameters.empty())
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
		for(auto& wp: watchedParameters)
		{
			auto lv = wp.lastValue;
			auto nv = bd.pSource->getParameterValue(wp.index);

			wp.lastValue = nv;

			if(lv != nv)
				onParameterChange(wp);
		}
	}

	std::vector<WatchedParameter> watchedParameters;
	
	JUCE_DECLARE_WEAK_REFERENCEABLE(WrappedDisplayBufferComponent);
	
};

struct oscillator: public WrappedDisplayBufferComponent
{
	oscillator(const BuildData& bd):
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

		setSize(140, 50);
	}

	void onParameterChange(const WatchedParameter& p) override
	{
		if(p.name == "Mode")
			oscProvider.currentMode = (OscillatorDisplayProvider::Mode)(int)p.lastValue;

		if(p.name == "Phase")
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
	

	using CreateFunction = std::function<WrappedDisplayBufferComponent*(const BuildData&)>;

	Factory()
	{
	}

	WrappedDisplayBufferComponent* create(const BuildData& bd)
	{
		if(!bd)
			return nullptr;

		auto id = bd.v[PropertyIds::FactoryPath].toString();

		if(data->functions.find(id) != data->functions.end())
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

struct DummyComplexDataProvider: public ExternalDataHolder
{
	struct Editor: public Component
	{
		Editor(const ComplexDataUIBase::List& dataObjects_, NodeComponentParameterSource* pSource, PooledUIUpdater* updater, UndoManager* um, const ValueTree& v):
		  dataObjects(dataObjects_)
		{
			display_buffer_library::Factory f;
			display_buffer_library::BuildData bd;

			bd.rb = dynamic_cast<SimpleRingBuffer*>(dataObjects.getFirst().get());
			bd.pSource = pSource;
			bd.v = v;
			bd.updater = updater;

			if(auto c = f.create(bd))
			{
				addAndMakeVisible(c);
				editors.add(c);
				setSize(c->getWidth(), c->getHeight());
			}
			else
			{
				int w = 256;
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

				setSize(w, dataObjects.size() * h);
			}
		}

		void resized() override
		{
			auto b = getLocalBounds();

			for(auto e: editors)
			{
				e->setBounds(b.removeFromTop(100));
			}
		}

		OscillatorDisplayProvider oscProvider;

		ComplexDataUIBase::List dataObjects;
		OwnedArray<Component> editors;
		complex_ui_laf laf;
	};

	int getNumDataObjects(ExternalData::DataType t) const override
	{
		switch(t)
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

		for(int i = 0; i < 8192; i++)
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

		if(idx == -1)
		{
			auto b64 = v[PropertyIds::EmbeddedData].toString();

			if(data.obj != nullptr)
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


struct LayoutTools
{
	struct CableData
	{
		Point<float> s;
		Point<float> e;
		ValueTree con;
	};

	template <char Dimension> struct Sorter
	{
		static int compareElements(const ValueTree& v1, const ValueTree& v2)
		{
			auto y1 = v1[Dimension == 'y' ? UIPropertyIds::y : UIPropertyIds::x];
			auto y2 = v2[Dimension == 'y' ? UIPropertyIds::y : UIPropertyIds::x];

			if (y1 < y2)
				return -1;
			if (y1 > y2)
				return 1;

			return 0;
		}
	};

	static void alignVertically(const Array<ValueTree>& list, UndoManager* um);
	static void alignHorizontally(const Array<ValueTree>& list, UndoManager* um);

	static void distributeVertically(const Array<ValueTree>& list, UndoManager* um);
	static void distributeHorizontally(const Array<ValueTree>& list, UndoManager* um);

	static void distributeInternal(const Array<ValueTree>& list, UndoManager* um, bool horizontal);

	static void alignCables(const Array<CableData>& list, UndoManager* um);
	static void distributeCableOffsets(const Array<ValueTree>& list, UndoManager* um);

	static float getCableThickness(int lod)
	{
		float data[3] = { 1.0f, 2.0f, 4.0f };
		return data[jlimit(0, 2, lod)];
	}

	

};



struct Helpers
{
	enum class ConnectionType
	{
		Parameter,
		Bypassed,
		Modulation,
		MultiModulation,
		RoutableSignal,
		numConnectionTypes
	};

	static constexpr int HeaderHeight = 24;
	static constexpr int SignalHeight = 40;
	static constexpr int NodeMargin = 30;
	static constexpr int ParameterHeight = 32;
	static constexpr int ParameterWidth = 150;
	static constexpr int ParameterMargin = 5;
	static constexpr int ModulationOutputWidth = 80;
	static constexpr int ModulationOutputHeight = 32;

	static constexpr valuetree::AsyncMode UIMode = valuetree::AsyncMode::Asynchronously;

	static std::pair<String, String> getFactoryPath(const ValueTree& v);

	static var getNodeProperty(const ValueTree& v, const Identifier& id, const var& defaultValue);

	static String getUniqueId(const String& prefix, const ValueTree& rootTree);

	static void setNodeProperty(ValueTree v, const Identifier& propertyName, const var& value, UndoManager* um);

	static Colour getFadeColour(int index, int numPaths)
	{
		if (numPaths == 0)
			return Colours::transparentBlack;

		auto hue = (float)index / (float)numPaths;

		auto saturation = JUCE_LIVE_CONSTANT_OFF(0.3f);
		auto brightness = JUCE_LIVE_CONSTANT_OFF(1.0f);
		auto minHue = JUCE_LIVE_CONSTANT_OFF(0.2f);
		auto maxHue = JUCE_LIVE_CONSTANT_OFF(0.8f);
		auto alpha = JUCE_LIVE_CONSTANT_OFF(0.4f);

		hue = jmap(hue, minHue, maxHue);

		return Colour::fromHSV(hue, saturation, brightness, alpha);
	}

	static ValueTree findParentNode(const ValueTree& v)
	{
		return valuetree::Helpers::findParentWithType(v, PropertyIds::Node);
	}

	static Path createPinHole();

	static void createCustomizableCurve(Path& path, Point<float> start, Point<float> end, float offset, float roundedCorners=10.0f, bool startNewPath=true)
	{
		float dx = end.x - start.x;
		float dy = end.y - start.y;

		if(dx < 0.0f)
		{
			//offset = jlimit(-1.0f, 1.0f, offset);
			//offset *= dy * 0.5;

			offset = jlimit(-std::abs(dy) * 0.5f, std::abs(dy) * 0.5f, offset);

			auto cx1 = start.translated(15.0f, 0.0f);
			auto cx2 = end.translated(-15.0f, 0.0f);

			auto mid1 = cx1.translated(0.0f, offset + dy * 0.5f);
			auto mid2 = cx2.translated(0.0f, offset + dy * -0.5f);

			if(startNewPath)
				path.startNewSubPath(start);

			path.lineTo(cx1);
			path.lineTo(mid1);
			path.lineTo(mid2);
			path.lineTo(cx2);
			path.lineTo(end);

			if(roundedCorners > 0.0f)
				path = path.createPathWithRoundedCorners(roundedCorners);
		}
		else
		{
			//offset = jlimit(-0.5f, 0.5f, offset);
			//offset *= dx;

			offset = jlimit(-std::abs(dx) * 0.5f, std::abs(dx) * 0.5f, offset);

			auto midX = start.x + dx * 0.5f + offset;

			const auto ARC = jmin(std::abs(dy * 0.5f), roundedCorners);

			float sign = end.y > start.y ? 1.0f : -1.0f;

			if(startNewPath)
				path.startNewSubPath(start);

			path.lineTo(midX - ARC, start.y);
			path.quadraticTo(midX, start.y, midX, start.y + ARC * sign);
			path.lineTo(midX, end.y - ARC * sign);
			path.quadraticTo(midX, end.y, midX + ARC, end.y);
			path.lineTo(end);
		}
	}

	static void createCurve(Path& path, Point<float> start, Point<float> end, bool preferLines)
	{
		
		float dx = end.x - start.x;
		float dy = end.y - start.y;

		if(preferLines)
		{
			path.lineTo(end);
		}
		else
		{
			float controlOffsetX = dx * 0.5f;

			juce::Point<float> cp1(start.x + controlOffsetX, start.y);
			juce::Point<float> cp2(end.x - controlOffsetX, end.y);

			path.cubicTo(cp1, cp2, end);
		}
	}

	static void forEachVisibleNode(const ValueTree& rootNode, const std::function<void(ValueTree)>& f, bool forceFirst)
	{
		if(rootNode.getType() == PropertyIds::Node)
			f(rootNode);

		auto childNodes = rootNode.getChildWithName(PropertyIds::Nodes);

		if(childNodes.isValid() && (!isFoldedOrLockedContainer(rootNode) || forceFirst))
		{
			for(auto cn: childNodes)
				forEachVisibleNode(cn, f, false);
		}
	}

	static bool hasRoutableSignal(const ValueTree& v);

	static bool isProcessNode(const ValueTree& v);

	static bool isVerticalContainer(const ValueTree& v);

	static bool isContainerNode(const ValueTree& v);

	static bool isRootNode(const ValueTree&);

	static ValueTree getCurrentRoot(const ValueTree& child);

	static bool shouldBeVertical(const ValueTree& container);

	static bool isFoldedRecursive(const ValueTree& v);

	static bool isFoldedOrLockedContainer(const ValueTree& v);

	static bool isProcessingSignal(const ValueTree& v);

	static void translatePosition(ValueTree node, Point<int> delta, UndoManager* um);

	static void setMinPosition(Rectangle<int>& b, Point<int> minOffset);

	static bool hasDefinedBounds(const ValueTree& v);

	

	static void updateBounds(ValueTree v, Rectangle<int> newBounds, UndoManager* um);

	

	static bool isImmediateChildNode(const ValueTree& childNode, const ValueTree& parent);

	static void fixOverlap(ValueTree v, UndoManager* um, bool sortProcessNodesFirst);

	

	static Colour getNodeColour(const ValueTree& v);

	static String getHeaderTitle(const ValueTree& v);

	static String getSignalDescription(const ValueTree& container);

	static Point<int> getPosition(const ValueTree& v);

	static Rectangle<int> getBounds(const ValueTree& v, bool includingComment);

	static Rectangle<int> getBoundsInRoot(const ValueTree& v, bool includingComment);

	static int getNumChannels(const ValueTree& v)
	{
		return (int)v.getProperty(PropertyIds::CompileChannelAmount, 2);
	}

	static void updateChannelCount(const ValueTree& root, bool remove, UndoManager* um)
	{
		if (remove)
		{
			valuetree::Helpers::forEach(root, [&](ValueTree& v)
			{
				v.removeProperty(PropertyIds::CompileChannelAmount, um);
				return false;
			});
		}
		else
		{
			auto numChannels = getNumChannels(root);
			updateChannelRecursive(root.getChild(0), numChannels, um);
		}
	}

	static void resetLayout(ValueTree v, UndoManager* um);

	static void migrateFeedbackConnections(ValueTree root, bool createConnections, UndoManager* um);

	private:

	static void resetLayoutRecursive(ValueTree root, ValueTree& child, UndoManager* um);

	static void fixOverlapRecursive(ValueTree node, UndoManager* um, bool sortProcessNodesFirst);

	static void updateChannelRecursive(ValueTree v, int numChannels, UndoManager* um);

	

};

struct ParameterHelpers
{
	/** Finds the parent node that will be used for the CablePinBase class. */
	static ValueTree findConnectionParent(const ValueTree& con)
	{
		jassert(con.getType() == PropertyIds::Connection);

		auto p = valuetree::Helpers::findParentWithType(con, PropertyIds::Parameter);

		if(p.isValid())
			return p;

		auto mt = valuetree::Helpers::findParentWithType(con, PropertyIds::ModulationTargets);

		if(mt.isValid())
			return mt;

		auto rt = valuetree::Helpers::findParentWithType(con, PropertyIds::ReceiveTargets);

		if(rt.isValid())
			return mt;

		auto sw = valuetree::Helpers::findParentWithType(con, PropertyIds::SwitchTarget);

		if(sw.isValid())
			return sw;

		jassertfalse;
		return ValueTree();
	}

	static bool isRoutingReceiveNode(const ValueTree& v);

	static bool isRoutingSendNode(const ValueTree& v);

	static bool isSoftBypassNode(const ValueTree& v);

	/** Checks if the connection points to either a feedback or softbypass node. */
	static bool isNodeConnection(const ValueTree& con);

	static Colour getParameterColour(const ValueTree& p);

	static Array<ValueTree> getAutomatedChildParameters(ValueTree container);

	static String getParameterPath(const ValueTree& v);

	/** Searches the entire network tree for a matching connection. */
	static ValueTree getConnection(const ValueTree& p);

	/** Searches the entire network tree for the matching parameter. */
	static ValueTree getTarget(const ValueTree& con);
	static double getThisValueOrFindDirectSource(juce::ValueTree ptree);
};

struct DataBaseHelpers
{
	static bool isSignalNode(const ValueTree& v)
	{
		auto p = v[PropertyIds::FactoryPath].toString();

		NodeDatabase db;
		
		if(auto obj = db.getProperties(p))
		{
			return !obj->getProperty(PropertyIds::OutsideSignalPath);
		}

		return false;
	}
};



}

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
DECLARE_ID(Locked);
DECLARE_ID(ParameterYOffset);
DECLARE_ID(CommentWidth);
DECLARE_ID(CommentOffsetX);
DECLARE_ID(CommentOffsetY);
#undef DECLARE_ID

struct Helpers
{
	static Array<Identifier> getPositionIds() { return { x, y, width, height }; }
};

}


struct CommentHelpers
{

	static int getCommentWidth(ValueTree data);
	static Point<int> getCommentOffset(ValueTree data);
};

struct LayoutTools
{
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
	static constexpr int NodeMargin = 50;
	static constexpr int ParameterHeight = 32;
	static constexpr int ParameterWidth = 128;
	static constexpr int ParameterMargin = 10;

	static constexpr valuetree::AsyncMode UIMode = valuetree::AsyncMode::Asynchronously;

	static std::pair<String, String> getFactoryPath(const ValueTree& v);

	static var getNodeProperty(const ValueTree& v, const Identifier& id, const var& defaultValue);

	static void setNodeProperty(ValueTree& v, const Identifier& propertyName, const var& value, UndoManager* um);

	static Path createPinHole();

	static void createCurve(Path& path, Point<float> start, Point<float> end, Point<float> velocity, bool useCubic)
	{
		float dx = end.x - start.x;
		float absDx = std::abs(dx);

		// Minimum outward bend when wiring backward
		float minOffset = JUCE_LIVE_CONSTANT_OFF(80.0f);

		// Use half-distance going forward, otherwise give it some breathing room
		float controlOffsetX = dx > minOffset ? absDx * 0.5f : 20.0f;

		// Control points (bend horizontally)
		juce::Point<float> cp1(start.x + controlOffsetX, start.y);
		juce::Point<float> cp2(end.x - controlOffsetX, end.y);

		path.cubicTo(cp1, cp2, end);
	}

	static bool hasRoutableSignal(const ValueTree& v);

	static bool isProcessNode(const ValueTree& v);

	static bool isVerticalContainer(const ValueTree& v);

	static bool isContainerNode(const ValueTree& v);

	static bool isRootNode(const ValueTree&);

	static bool shouldBeVertical(const ValueTree& container);

	static bool isFoldedRecursive(const ValueTree& v);

	static bool isProcessingSignal(const ValueTree& v);

	static void translatePosition(ValueTree& node, Point<int> delta, UndoManager* um);

	static void setMinPosition(Rectangle<int>& b, Point<int> minOffset);

	static bool hasDefinedBounds(const ValueTree& v);

	static void updateBounds(ValueTree& v, Rectangle<int> newBounds, UndoManager* um);

	static Colour getParameterColour(const ValueTree& p)
	{
		if(p.getType() == PropertyIds::Node)
			return getNodeColour(p);

		auto c = (int64)p[PropertyIds::NodeColour];

		if (c != 0)
			return Colour((uint32)c);

		return Colours::red.withHue(Random::getSystemRandom().nextFloat()).withSaturation(0.6f).withBrightness(0.7f);
	}

	static bool isImmediateChildNode(const ValueTree& childNode, const ValueTree& parent);

	static void fixOverlap(ValueTree v, UndoManager* um, bool sortProcessNodesFirst);

	static Array<ValueTree> getAutomatedChildParameters(ValueTree container);

	static Colour getNodeColour(const ValueTree& v);

	static String getHeaderTitle(const ValueTree& v);

	static String getSignalDescription(const ValueTree& container);

	static Point<int> getPosition(const ValueTree& v);

	static Rectangle<int> getBounds(const ValueTree& v, bool includingComment);

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

	static void resetLayout(ValueTree& v, UndoManager* um);

	private:

	static void resetLayoutRecursive(ValueTree& root, ValueTree& child, UndoManager* um);

	static void fixOverlapRecursive(ValueTree& node, UndoManager* um, bool sortProcessNodesFirst);

	static void updateChannelRecursive(ValueTree& v, int numChannels, UndoManager* um)
	{
		jassert(v.getType() == PropertyIds::Node);

		auto path = getFactoryPath(v);

		auto isMulti = path.second == "multi";
		auto isSideChain = path.second == "sidechain";
		auto isModChain = path.second == "modchain";

		if (isSideChain)
			numChannels *= 2;

		if(isModChain)
			numChannels = 1;

		v.setProperty(PropertyIds::CompileChannelAmount, numChannels, um);

		auto childNodes = v.getChildWithName(PropertyIds::Nodes);

		if(isMulti)
			numChannels /= childNodes.getNumChildren();
		

		for (auto cn : childNodes)
		{
			updateChannelRecursive(cn, numChannels, um);
		}
	}

public:
	
};

}
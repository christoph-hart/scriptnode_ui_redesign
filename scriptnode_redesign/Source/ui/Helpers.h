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
DECLARE_ID(CableOffset);
DECLARE_ID(Groups);
DECLARE_ID(Group);
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
	static constexpr int ParameterMargin = 5;

	static constexpr valuetree::AsyncMode UIMode = valuetree::AsyncMode::Asynchronously;

	static std::pair<String, String> getFactoryPath(const ValueTree& v);

	static var getNodeProperty(const ValueTree& v, const Identifier& id, const var& defaultValue);

	static String getUniqueId(const String& prefix, const ValueTree& rootTree);

	static void setNodeProperty(ValueTree& v, const Identifier& propertyName, const var& value, UndoManager* um);

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

	static Path createPinHole();

	static void createCustomizableCurve(Path& path, Point<float> start, Point<float> end, float offset)
	{
		float dx = end.x - start.x;
		float dy = end.y - start.y;

		if(dx < 0.0f)
		{
			offset = jlimit(-1.0f, 1.0f, offset);
			offset *= dy * 0.5;

			auto cx1 = start.translated(15.0f, 0.0f);
			auto cx2 = end.translated(-15.0f, 0.0f);

			auto mid1 = cx1.translated(0.0f, offset + dy * 0.5f);
			auto mid2 = cx2.translated(0.0f, offset + dy * -0.5f);

			path.startNewSubPath(start);
			path.lineTo(cx1);
			path.lineTo(mid1);
			path.lineTo(mid2);
			path.lineTo(cx2);
			path.lineTo(end);

			path = path.createPathWithRoundedCorners(10);
		}
		else
		{
			offset = jlimit(-0.5f, 0.5f, offset);
			offset *= dx;
			auto midX = start.x + dx * 0.5f + offset;

			const auto ARC = jmin(std::abs(dy * 0.5f), 10.0f);

			float sign = end.y > start.y ? 1.0f : -1.0f;

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

		if(p.getType() == PropertyIds::ModulationTargets || p.getType() == PropertyIds::SwitchTargets)
			return getNodeColour(valuetree::Helpers::findParentWithType(p, PropertyIds::Node));

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

	static void updateChannelRecursive(ValueTree& v, int numChannels, UndoManager* um);

};

struct DataBaseHelpers
{
	static bool isSignalNode(const ValueTree& v)
	{
		auto p = v[PropertyIds::FactoryPath].toString();

		NodeDatabase db;
		
		if(auto obj = db.getProperties(p))
		{
			return obj->getProperty(PropertyIds::AddToSignal);
		}

		return false;
	}
};



}
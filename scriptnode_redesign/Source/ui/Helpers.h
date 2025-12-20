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

struct HoverShapeButton: public HiseShapeButton
						 
{
	struct HoverListener: public juce::MouseListener
	{
		HoverListener(HoverShapeButton& p, Component* c_):
		  parent(p),
		  c(c_)
		{
			c->addMouseListener(this, true);
		}

		~HoverListener()
		{
			c->removeMouseListener(this);
		}

		void mouseEnter (const MouseEvent& e) override 
		{
			parent.repaint();
			
		}

		void mouseExit (const MouseEvent& e) override 
		{
			parent.repaint();
		}

		HoverShapeButton& parent;
		Component::SafePointer<Component> c;
	} listener;

	HoverShapeButton(const String& name, Component* parent, const PathFactory& factory):
	  HiseShapeButton(name, nullptr, factory),
	  listener(*this, parent)
	{
		
	}
	
	void paint(Graphics& g) override
	{
		if(listener.c->isMouseOver(true))
			HiseShapeButton::paint(g);
	}

};

struct DummyBody : public ScriptnodeExtraComponentBase
{
	static ScriptnodeExtraComponentBase* createDummyComponent(const String& path);

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
	static constexpr int ParameterWidth = 110;
	static constexpr int ContainerParameterWidth = 150;
	static constexpr int ParameterMargin = 5;
	static constexpr int ModulationOutputWidth = 80;
	static constexpr int ModulationOutputHeight = 32;

	static constexpr valuetree::AsyncMode UIMode = valuetree::AsyncMode::Asynchronously;

	static std::pair<String, String> getFactoryPath(const ValueTree& v);
	static var getNodeProperty(const ValueTree& v, const Identifier& id, const var& defaultValue);
	static String getUniqueId(const String& prefix, const ValueTree& rootTree);

	static void setNodeProperty(ValueTree v, const Identifier& propertyName, const var& value, UndoManager* um);
	static Colour getFadeColour(int index, int numPaths);

	static ValueTree findParentNode(const ValueTree& v);

	static Path createPinHole();
	static void createCustomizableCurve(Path& path, Point<float> start, Point<float> end, float offset, float roundedCorners=10.0f, bool startNewPath=true);
	static void createCurve(Path& path, Point<float> start, Point<float> end, bool preferLines);

	static void forEachVisibleNode(const ValueTree& rootNode, const std::function<void(ValueTree)>& f, bool forceFirst);

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

	static int getNumChannels(const ValueTree& v);
	static void updateChannelCount(const ValueTree& root, bool remove, UndoManager* um);
	static void resetLayout(ValueTree v, UndoManager* um);
	static void migrateFeedbackConnections(ValueTree root, bool createConnections, UndoManager* um);

	static snex::Types::PrepareSpecs getDummyPrepareSpecs()
	{
		snex::Types::PrepareSpecs ps;
		ps.sampleRate = 44100.0;
		ps.blockSize = 512;
		ps.numChannels = 2;

		return ps;
	}

	private:

	static void resetLayoutRecursive(ValueTree root, ValueTree& child, UndoManager* um);
	static void fixOverlapRecursive(ValueTree node, UndoManager* um, bool sortProcessNodesFirst);
	static void updateChannelRecursive(ValueTree v, int numChannels, UndoManager* um);
};

struct ParameterHelpers
{
	/** Finds the parent node that will be used for the CablePinBase class. */
	static ValueTree findConnectionParent(const ValueTree& con);

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

	struct ModOutputSyncer
	{
		ModOutputSyncer(const ValueTree& n, UndoManager* um_):
		  nodeTree(n),
		  um(um_)
		{
			jassert(n.getType() == PropertyIds::Node);

			auto propTree = n.getChildWithName(PropertyIds::Properties);
			auto p = propTree.getChildWithProperty(PropertyIds::ID, PropertyIds::NumParameters.toString());
			auto st = n.getChildWithName(PropertyIds::SwitchTargets);

			ScopedValueSetter<bool> svs(recursive, true);

			switchTargetListener.setCallback(st, valuetree::AsyncMode::Synchronously, VT_BIND_CHILD_LISTENER(onSwitchTarget));
			numParameterListener.setCallback(p, { PropertyIds::Value }, valuetree::AsyncMode::Synchronously, VT_BIND_PROPERTY_LISTENER(onNumParameters));
		}

		std::function<void()> onChange;

	private:

		void change(int current, int shouldHave)
		{
			if(!recursive && current != shouldHave)
			{
				ScopedValueSetter<bool> svs(recursive, true);

				auto numToDelete = current - shouldHave;
				auto numToAdd = shouldHave - current;

				auto sts = nodeTree.getChildWithName(PropertyIds::SwitchTargets);

				while(numToDelete > 0)
				{
					auto lastChild = sts.getChild(sts.getNumChildren()-1);
					sts.removeChild(lastChild, um);
					numToDelete--;
				}

				for(int i = 0; i < numToAdd; i++)
				{
					ValueTree newChild(PropertyIds::SwitchTarget);
					ValueTree con(PropertyIds::Connections);
					newChild.addChild(con, -1, nullptr);
					sts.addChild(newChild, -1, um);
				}

				Helpers::setNodeProperty(nodeTree, PropertyIds::NumParameters, shouldHave, um);

				if(onChange)
					onChange();
			}
		}

		void onSwitchTarget(const ValueTree& v, bool wasAdded)
		{
			auto numTargets = nodeTree.getChildWithName(PropertyIds::SwitchTargets).getNumChildren();
			auto numParameters = Helpers::getNodeProperty(nodeTree, PropertyIds::NumParameters, 2);
			change(numParameters, numTargets);
		}

		void onNumParameters(const Identifier&, const var& newValue)
		{
			auto numParameters = (int)newValue;
			auto numTargets = nodeTree.getChildWithName(PropertyIds::SwitchTargets).getNumChildren();
			change(numTargets, numParameters);
		}

		UndoManager* um;
		ValueTree nodeTree;
		bool recursive = false;
		valuetree::PropertyListener numParameterListener;
		valuetree::ChildListener switchTargetListener;
	};
};

struct DataBaseHelpers
{
	static bool isSignalNode(const ValueTree& v);
};



}

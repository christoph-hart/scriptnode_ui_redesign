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


struct ContainerComponent : public NodeComponent,
							public ContainerComponentBase
{
	struct CableSetup
	{
		enum class ContainerType
		{
			Serial,
			Split,
			Multi,
			ModChain,
			Branch,
			Offline
		};

		struct Connection
		{
			int sourceIndex;
			int targetIndex;
			bool active;
		};

		struct Pins
		{
			Point<float> containerStart;
			Point<float> containerEnd;

			struct Position
			{
				Point<float> first;
				Point<float> second;
				int dropIndex;
			};


			std::vector<Position> childPositions;
		};

		using MultiChannelConnection = std::vector<Connection>;

		Path pin;
		int activeBranch = -1;

		CableSetup(ContainerComponent& parent_, const ValueTree& v);

		bool isSerialType() const;

		void onBranch(const Identifier&, const var& newValue);

		void updatePins(ContainerComponent& p);

		void init(const ValueTree& v);

		void updateVerticalState()
		{
			isForcedVertical = type == CableSetup::ContainerType::Serial && Helpers::isVerticalContainer(parent.getValueTree());
		}

		struct CableDrawData
		{
			Point<float> p1;
			Point<float> p2;
			Colour c;
			int channelIndex;
			int numChannels;
			float cableOffset;
			int lod;
			bool active = true;

			void setCoordinates(Point<float> p1_, Point<float> p2_, float cableOffset_, int channelIndex_)
			{
				p1 = p1_;
				p2 = p2_;
				cableOffset = cableOffset_;
				channelIndex = channelIndex_;
			}

			void draw(Graphics& g, bool preferLines);
		};

		bool isVertical() const
		{
			if (type == ContainerType::Branch || type == ContainerType::Multi || type == ContainerType::Split)
				return true;

			return isForcedVertical;
		}

		bool routeThroughChildNodes() const;

		void setDragPosition(Rectangle<int> currentlyDraggedNodeBounds, Point<int> downPos);

		float getCableOffset(int nodeIndex, Point<float> p1, Point<float> p2) const;
		float getCableOffset(int nodeIndex, float maxWidth) const;

		void draw(Graphics& g);

		int lod = 0;
		valuetree::PropertyListener branchListener;
		ContainerComponent& parent;
		bool isForcedVertical = false;
		ContainerType type;
		int numChannels = 0;
		int numNodes = 0;
		std::vector<MultiChannelConnection> connections;
		Rectangle<int> dragPosition;
		int dropIndex = -1;

		Pins pinPositions;
	};

	ContainerComponent(Lasso* l, const ValueTree& v, UndoManager* um_);

	void onGroup(const ValueTree& v, bool wasAdded);
	void onFreeComment(const ValueTree& v, bool wasAdded);
	void onChannel(const Identifier&, const var& newValue);
	void onChildLock(const ValueTree& v, const Identifier& id);
	void onCommentChange(const ValueTree& v, const Identifier& id);
	void onBypass(const ValueTree& v, const Identifier&);
	void onFold(const Identifier& id, const var& newValue) override;
	void onChildPositionUpdate(const ValueTree& v, const Identifier& id);
	void onChildAddRemove(const ValueTree& v, bool wasAdded);
	void onResize(const Identifier& id, const var& newValue);
	void onIsVertical(const ValueTree& v, const Identifier& id);
	void onParameterPosition(const ValueTree& v, const Identifier& id);
	void onParameterAddRemove(const ValueTree& v, bool wasAdded)
	{
		rebuildDefaultParametersAndOutputs();

		if(wasAdded && !Helpers::getPosition(v).isOrigin())
			onParameterPosition(v, UIPropertyIds::x);
	}

	void rebuildDescription();

	void paint(Graphics& g) override;

	void resized() override;

	void renameGroup(Point<float> position);
	bool selectGroup(Point<float> position);

	CablePinBase* createOutsideParameterComponent(const ValueTree& source) override
	{
		auto conParent = ParameterHelpers::findConnectionParent(source);

		if (conParent.getType() == PropertyIds::Parameter)
			return new ParameterComponent(getUpdater(), conParent, asNodeComponent().getUndoManager());
		else
			return new ModulationBridge(conParent, asNodeComponent().getUndoManager());
	}

	ParameterComponent* getDraggableParameterComponent(int index);

	ContainerComponent* getInnerContainer(Component* root, Point<int> rootPosition, Component* toSkip)
	{
		if(this == toSkip)
			return nullptr;

		if (root->getLocalArea(this, getLocalBounds()).contains(rootPosition))
		{
			for (auto c : childNodes)
			{
				if (auto childContainer = dynamic_cast<ContainerComponent*>(c))
				{
					auto inner = childContainer->getInnerContainer(root, rootPosition, toSkip);

					if (inner != nullptr)
						return inner;
				}
			}

			return this;
		}

		return nullptr;
	}

	struct Group
	{
		Group(ContainerComponent& parent_, const ValueTree& v_) :
			name(v_[PropertyIds::ID].toString()),
			v(v_),
			parent(parent_)
		{
			auto list = v[PropertyIds::Value].toString();
			idList = StringArray::fromTokens(list, ";", "");
			idList.removeDuplicates(false);
			idList.removeEmptyStrings();
			idList.trim();
		}

		bool operator==(const ValueTree& other) const { return v == other; }

		void draw(Graphics& g);

		Rectangle<float> lastBounds;
		ValueTree v;
		ContainerComponent& parent;
		String name;
		StringArray idList;

		JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Group);
	};

	struct Comment : public Component
	{
		Comment(ContainerComponent& parent_, const ValueTree& d):
		  parent(parent_),
		  data(d),
		  renderer("")
		{
			auto sd = renderer.getStyleData();
			sd.fontSize = 14.0f;
			renderer.setStyleData(sd);

			positionListener.setCallback(data,
			{ PropertyIds::Comment,
				UIPropertyIds::CommentWidth,
				UIPropertyIds::CommentOffsetX,
				UIPropertyIds::CommentOffsetY,
			}, Helpers::UIMode, VT_BIND_PROPERTY_LISTENER(onUpdate));

			parent.addAndMakeVisible(this);
		}

		Comment(ContainerComponent& parent_, NodeComponent* attachedNode_) :
			parent(parent_),
			data(attachedNode_->getValueTree()),
			renderer("")
		{
			auto sd = renderer.getStyleData();
			sd.fontSize = 14.0f;
			renderer.setStyleData(sd);

			positionListener.setCallback(data,
				{ PropertyIds::Comment,
				  UIPropertyIds::x,
				  UIPropertyIds::y,
				  UIPropertyIds::width,
				  UIPropertyIds::height,
				  UIPropertyIds::CommentWidth,
				  UIPropertyIds::CommentOffsetX,
				  UIPropertyIds::CommentOffsetY,
				  PropertyIds::NodeColour,
				}, Helpers::UIMode, VT_BIND_PROPERTY_LISTENER(onUpdate));

			parent.addAndMakeVisible(this);
		}

		void showEditor();

		bool dragRightEdge = false;

		void mouseDoubleClick(const MouseEvent& e) override
		{
			showEditor();
		}

		void mouseMove(const MouseEvent& e) override
		{
			dragRightEdge = getWidth() -  e.getPosition().getX() < 20;
			setMouseCursor(dragRightEdge ? MouseCursor::LeftEdgeResizeCursor : MouseCursor::DraggingHandCursor);
			repaint();
		}

		Point<int> currentOffset;
		Point<int> downOffset;
		int downWidth = 0;

		void mouseDown(const MouseEvent& e) override
		{
			if(dragRightEdge)
			{
				downWidth = getWidth();
			}
			else
			{
				downOffset = currentOffset;
			}
		}

		void mouseUp(const MouseEvent& e) override
		{
			if(e.mouseWasDraggedSinceMouseDown())
				Helpers::fixOverlap(parent.getValueTree(), parent.um, false);
		}

		void mouseDrag(const MouseEvent& e) override
		{
			if(dragRightEdge)
			{
				auto newWidth = jlimit(128, 800, e.getDistanceFromDragStartX() + downWidth);
				data.setProperty(UIPropertyIds::CommentWidth, newWidth, parent.um);
			}
			else
			{
				auto newOffset = downOffset.translated(e.getDistanceFromDragStartX(), e.getDistanceFromDragStartY());
				data.setProperty(UIPropertyIds::CommentOffsetX, newOffset.getX(), parent.um);
				data.setProperty(UIPropertyIds::CommentOffsetY, newOffset.getY(), parent.um);
			}

			if(data.getType() == PropertyIds::Comment)
			{
				expandParentsRecursive(*this, getBoundsInParent(), false);
			}
		}

		void mouseExit(const MouseEvent& e) override
		{
			dragRightEdge = false;
			repaint();
		}

		void onUpdate(const Identifier& id, const var& newValue)
		{
			if(id == UIPropertyIds::x || id == UIPropertyIds::y || 
			   id == UIPropertyIds::CommentOffsetX || id == UIPropertyIds::CommentOffsetY)
			{
				currentOffset = CommentHelpers::getCommentOffset(data);

				if(data.getType() == PropertyIds::Comment)
				{
					setTopLeftPosition(currentOffset);
				}
				else
				{
					setTopLeftPosition(Helpers::getBounds(data, false).getTopRight().translated(10 + currentOffset.getX(), currentOffset.getY()));
				}
			}
			
			if(id == PropertyIds::NodeColour)
			{
				auto c = Helpers::getNodeColour(data);
				auto sd = renderer.getStyleData();
				sd.headlineColour = c;
				renderer.setStyleData(sd);
			}
			if(id == UIPropertyIds::CommentWidth)
			{
				int widthToUse = CommentHelpers::getCommentWidth(data);
				auto h = renderer.getHeightForWidth(widthToUse);
				setSize(widthToUse + 20, h + 20);
			}
			if (id == PropertyIds::Comment)
			{
				renderer.setNewText(newValue.toString());
				auto widthToUse = CommentHelpers::getCommentWidth(data);
				auto h = renderer.getHeightForWidth(widthToUse);
				setSize(widthToUse + 20, h + 20);
			}
		}

		void paint(Graphics& g) override
		{
			auto c = Helpers::getNodeColour(data);

			if(dragRightEdge)
			{
				g.setColour(c);
				g.fillRect(getLocalBounds().removeFromRight(3));
			}

			g.setColour(c.withAlpha(0.05f));
			g.fillRoundedRectangle(getLocalBounds().toFloat(), 4.0f);
			renderer.draw(g, getLocalBounds().toFloat().reduced(10.0f));
		}

		void resized() override
		{

		}

		ContainerComponent& parent;
		ValueTree data;

		valuetree::PropertyListener positionListener;

		MarkdownRenderer renderer;
	};

	struct BreakoutParameter: public Component,
							  public SelectableComponent,
							  public PathFactory
	{
		BreakoutParameter(ContainerComponent& realParent_, const ValueTree& data_, UndoManager* um_);;

		void resized() override
		{
			auto b = getLocalBounds();
			auto header = b.removeFromTop(Helpers::HeaderHeight);

			closeButton.setBounds(header.removeFromRight(header.getHeight()).reduced(4));
			gotoButton.setBounds(header.removeFromLeft(header.getHeight()).reduced(4));

			parameter.setBounds(b.reduced(Helpers::ParameterMargin));
		}

		void mouseDown(const MouseEvent& e) override
		{
			dragger.startDraggingComponent(this, e);
		}

		void mouseDrag(const MouseEvent& e) override;

		void mouseUp(const MouseEvent& e) override;

		void paint(Graphics& g) override
		{
			g.fillAll(Colour(0xFF353535));
			auto nc = ParameterHelpers::getParameterColour(getValueTree());
			g.setColour(nc);
			auto b = getLocalBounds().toFloat();
			g.drawRect(b, 1.0f);
			auto hb = b.removeFromTop(Helpers::HeaderHeight);
			g.fillRect(hb);
			hb.removeFromLeft(hb.getHeight());
			hb.removeFromRight(hb.getHeight());
			g.setColour(Colours::white);
			g.setFont(GLOBAL_FONT());
			g.drawText(parameter.getSourceDescription(), hb, Justification::left);
		}

		Path createPath(const String& url) const override
		{
			Path p;

			LOAD_EPATH_IF_URL("goto", ColumnIcons::openWorkspaceIcon);
			LOAD_EPATH_IF_URL("close", HiBinaryData::ProcessorEditorHeaderIcons::closeIcon);

			return p;
		}

		double getParameterValue(int index) const override { return 0.0; }

		InvertableParameterRange getParameterRange(int index) const override { return {}; }

		ValueTree getValueTree() const override { return data; }
		UndoManager* getUndoManager() const override { return um; }

		ComponentDragger dragger;

		ParameterComponent parameter;

		HeaderComponent::PowerButton gotoButton;
		HeaderComponent::PowerButton closeButton;

		ContainerComponent& realParent;

		ValueTree data;
		UndoManager* um;
		int index;
		
	};

	struct AddButton: public Component
	{
		AddButton(ContainerComponent& parent_, const Path& hitzone_, int index_):
		  p(parent.createPath("add")),
		  parent(parent_),
		  hitzone(hitzone_),
		  index(index_)
		{
			setBounds(hitzone.getBounds().toNearestInt());

			hitzone.applyTransform(AffineTransform::translation(getPosition()).inverted());
			parent.addAndMakeVisible(this);
			toBack();
			setRepaintsOnMouseActivity(true);
		}

		~AddButton()
		{
			int x = 5;
		}

		bool hitTest(int x, int y) override
		{
			Point<float> pos((float)x, (float)y);
			return hitzone.contains(pos);
		}

		void mouseDown(const MouseEvent& e) override
		{
			SelectableComponent::Lasso::checkLassoEvent(e, SelectableComponent::LassoState::Down);

			downOffset = getNodeBefore()[UIPropertyIds::CableOffset];
			repaint();
		}

		ValueTree getNodeBefore()
		{
			if(index == 0)
				return parent.getValueTree().getChildWithName(PropertyIds::Nodes);
			else
				return parent.getValueTree().getChildWithName(PropertyIds::Nodes).getChild(index-1);
		}

		void mouseDrag(const MouseEvent& e) override
		{
			SelectableComponent::Lasso::checkLassoEvent(e, SelectableComponent::LassoState::Drag);

			setMouseCursor(MouseCursor::LeftRightResizeCursor);

			auto dx = e.getDistanceFromDragStartX();

			getNodeBefore().setProperty(UIPropertyIds::CableOffset, downOffset + dx, parent.um);
			parent.repaint();
		}

		float downOffset = 0.0f;

		int onNodeDrag(Point<float> lp)
		{
			auto prev = draggedOver;

			draggedOver = contains(lp);

			if(prev && !draggedOver)
			{
				int x = 5;
			}

			repaint();
			return draggedOver ? index : -1;
		}

		bool draggedOver = false;

		void mouseUp(const MouseEvent& e) override;
		
		void paint(Graphics& g) override
		{
			if(draggedOver)
			{
				g.setColour(Colour(SIGNAL_COLOUR).withAlpha(0.3f));
				g.fillPath(hitzone);
				g.setColour(Colour(SIGNAL_COLOUR));
				g.strokePath(hitzone, PathStrokeType(1.0f));
			}

			if(active && downOffset == 0.0f)
			{
				g.setColour(Colours::white.withAlpha(0.1f));
				g.fillPath(hitzone);

				g.setColour(Colours::white.withAlpha(0.4f));
				g.fillPath(p);
			}
		}

		bool active = false;

		void mouseExit(const MouseEvent& e) override
		{
			active = false;
			setMouseCursor(MouseCursor::NormalCursor);
			repaint();
		}

		void mouseMove(const MouseEvent& e) override;

		void resized() override 
		{
			auto b = getLocalBounds().toFloat();

			auto a = Rectangle<float>(b.getCentre(), b.getCentre()).withSizeKeepingCentre(12.0f, 12.0f);

			auto offset = (float)getNodeBefore()[UIPropertyIds::CableOffset];

			a = a.translated(offset, 0.0f);

			parent.scalePath(p, a);
		}

		ContainerComponent& parent;
		
		

		Path p;

		Path hitzone;

		const int index;
	};


	static void expandParentsRecursive(Component& componentToShow, Rectangle<int> firstBoundsMightBeUnion, bool addMarginToFirst);

	OwnedArray<BreakoutParameter> breakoutParameters;
	
	NodeResizer resizer;

	Image cableImage;

	OwnedArray<NodeComponent> childNodes;
	OwnedArray<Group> groups;
	OwnedArray<AddButton> addButtons;
	OwnedArray<Comment> comments;
	CableSetup cables;
	HoverShapeButton lockButton;

	ScopedPointer<TextEditor> groupEditor;

	valuetree::PropertyListener resizeListener;
	valuetree::PropertyListener channelListener;
	valuetree::ChildListener nodeListener;
	valuetree::ChildListener parameterListener;
	valuetree::RecursivePropertyListener childPositionListener;
	valuetree::RecursivePropertyListener bypassListener;
	valuetree::RecursivePropertyListener commentListener;
	valuetree::RecursivePropertyListener verticalListener;
	valuetree::ChildListener groupListener;
	valuetree::ChildListener freeCommentListener;
	valuetree::RecursivePropertyListener lockListener;
	valuetree::RecursivePropertyListener breakoutListener;

	AttributedString description;

	JUCE_DECLARE_WEAK_REFERENCEABLE(ContainerComponent);
	
};


}
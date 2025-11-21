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

struct CableBase: public Component
{
	bool hitTest(int x, int y) override
	{
		Point<float> sp((float)x, (float)y);
		Point<float> tp;
		p.getNearestPoint(sp, tp);

		return sp.getDistanceFrom(tp) < 20.0f;
	}

	void rebuildPath(Point<float> newStart, Point<float> newEnd, Component* parent)
	{
		s = newStart;
		e = newEnd;

		Rectangle<float> a(s, e);
		setBounds(a.toNearestInt().expanded(22));

		s = getLocalPoint(parent, s);
		e = getLocalPoint(parent, e);

		p.clear();
		p.startNewSubPath(s);
		Helpers::createCurve(p, s, e.translated(-3.0f, 0.0f), { 0.0f, 0.0f }, true);
		repaint();
	}

	void paint(Graphics& g) override
	{
		if(colour1 == colour2)
			g.setColour(colour1);
		else
			g.setGradientFill(ColourGradient(colour1, s, colour2, e, false));
		g.strokePath(p, PathStrokeType(over ? 3.0f : 1.0f));

		Path arrow;
		g.fillEllipse(Rectangle<float>(s, s).withSizeKeepingCentre(6.0f, 6.0f));
		arrow.startNewSubPath(e);
		arrow.lineTo(e.translated(-7.0f, 5.0f));
		arrow.lineTo(e.translated(-7.0f, -5.0f));
		arrow.closeSubPath();
		g.fillPath(arrow);
	}

	Path p;
	bool over = false;
	Point<float> s, e;
	Colour colour1;
	Colour colour2;
};

struct DraggedCable: public CableBase
{
	DraggedCable(CablePinBase* src_):
	  src(src_)
	{
		colour1 = Colour(SIGNAL_COLOUR);
		colour2 = colour1;
	}

	void setTargetPosition(Point<int> rootPosition);


	CablePinBase::WeakPtr src;
	CablePinBase::WeakPtr hoveredPin;
};

struct CableComponent : public CableBase,
					    public SelectableComponent
{
	struct CableLabel : public Component,
						public ComponentMovementWatcher,
						public AsyncUpdater 
	{
		CableLabel(CableComponent* c) :
			ComponentMovementWatcher(c),
			attachedCable(c),
			currentText(attachedCable->getLabelText())
		{
			auto w = GLOBAL_FONT().getStringWidth(currentText) + 10;
			setSize(w, 20);
			updatePosition();
		};

		void handleAsyncUpdate() override
		{
			updatePosition();
		}

		void updatePosition()
		{
			if (attachedCable.getComponent() != nullptr)
			{
				setVisible((attachedCable->getWidth() > 300 || attachedCable->getHeight() > 500) && attachedCable->isVisible());

				auto holder = dynamic_cast<Component*>(attachedCable->findParentComponentOfClass<CableHolder>());

				if (holder != nullptr)
				{
					auto pos = holder->getLocalPoint(attachedCable, attachedCable->e.toInt());
					pos = pos.translated(-10, -10);
					setTopRightPosition(pos.getX(), pos.getY());
				}
			}
		}

		void componentVisibilityChanged() override
		{
			triggerAsyncUpdate();
		}

		void componentMovedOrResized(bool wasMoved, bool wasResized) override
		{
			triggerAsyncUpdate();
		}

		void componentPeerChanged() override {};

		void paint(Graphics& g) override
		{
			if (attachedCable.getComponent() != nullptr)
			{
				g.setFont(GLOBAL_FONT());
				g.setColour(Colour(0xaa222222));
				g.fillRoundedRectangle(getLocalBounds().toFloat(), (float)getHeight() * 0.5f);
				g.setColour(attachedCable->colour2);
				g.drawText(currentText, getLocalBounds().toFloat().reduced(5.0f, 0.0f), Justification::right);
			}
		}

		Component::SafePointer<CableComponent> attachedCable;
		String currentText;
	};

	struct CableHolder
	{
		CableHolder(const ValueTree& v)
		{
			connectionListener.setTypesToWatch({
			PropertyIds::Connections,
			PropertyIds::ModulationTargets
				});

			connectionListener.setCallback(v, Helpers::UIMode, VT_BIND_CHILD_LISTENER(onConnectionChange));
			initialised = true;
		}

		virtual ~CableHolder() = default;

		bool initialised = false;

		void rebuildCables();

		OwnedArray<CableComponent> cables;
		OwnedArray<CableLabel> labels;
		ScopedPointer<DraggedCable> currentlyDraggedCable;

		void onConnectionChange(const ValueTree& v, bool wasAdded)
		{
			rebuildCables();
		}

		valuetree::RecursiveTypedChildListener connectionListener;
	};

	


	static ValueTree getConnectionTree(CablePinBase* src, CablePinBase* dst)
	{
		auto nodeId = dst->getNodeTree()[PropertyIds::ID].toString();
		auto parameterId = dst->getTargetParameterId();

		auto conTree = src->getConnectionTree();

		for(auto c: conTree)
		{
			if(c[PropertyIds::NodeId].toString() == nodeId &&
			   c[PropertyIds::ParameterId].toString() == parameterId)
				return c;
		}

		jassertfalse;
		return {};
	}

	CableComponent(Lasso& l, CablePinBase* src_, CablePinBase* dst_) :
		CableBase(),
		SelectableComponent(l),
		src(src_),
		dst(dst_),
		connectionTree(getConnectionTree(src_, dst_))
	{
		auto ids = UIPropertyIds::Helpers::getPositionIds();
		ids.add(PropertyIds::Folded);

		sourceListener.setCallback(src->getNodeTree(), ids, valuetree::AsyncMode::Asynchronously,
			VT_BIND_PROPERTY_LISTENER(updatePosition));

		targetListener.setCallback(dst->getNodeTree(), ids, valuetree::AsyncMode::Asynchronously,
			VT_BIND_PROPERTY_LISTENER(updatePosition));

		setInterceptsMouseClicks(false, false);
		setRepaintsOnMouseActivity(true);

		removeListener.setCallback(connectionTree, Helpers::UIMode, true, [this](const ValueTree& v)
		{
			auto ch = findParentComponentOfClass<CableHolder>();
			ch->rebuildCables();
		});
	}

	ValueTree connectionTree;

	ValueTree getValueTree() const
	{
		return connectionTree;
	}

	String getLabelText() const
	{
		String msg;

		auto n = src->getNodeTree()[PropertyIds::ID].toString();

		if (src->getNodeTree().getParent().getType() != PropertyIds::Network)
			msg << n;

		auto p = src->getTargetParameterId();

		if (msg.isNotEmpty() && p.isNotEmpty())
			msg << ".";

		if (p.isNotEmpty())
			msg << p;

		return msg;
	}

	void updatePosition(const Identifier& id, const var&)
	{
		if (id == PropertyIds::Folded)
		{
			auto sourceUnfolded = !Helpers::isFoldedRecursive(src->getNodeTree());
			auto targetUnfolded = !Helpers::isFoldedRecursive(dst->getNodeTree());

			setVisible(sourceUnfolded && targetUnfolded);
		}
		else
		{
			auto parent = dynamic_cast<Component*>(findParentComponentOfClass<CableHolder>());

			if (parent == nullptr)
				return;

			auto start = parent->getLocalArea(src, src->getLocalBounds()).toFloat();
			auto ns = Point<float>(start.getRight(), start.getCentreY());
			auto end = parent->getLocalArea(dst, dst->getLocalBounds()).toFloat();
			auto ne = Point<float>(end.getX(), end.getCentreY());
			
			rebuildPath(ns, ne, parent);
		}
	}

	void paintOverChildren(Graphics& g) override
	{
		if(selected)
		{
			g.setColour(Colour(SIGNAL_COLOUR).withAlpha(0.4f));
			g.strokePath(p, PathStrokeType(3));
		}
	}

	void mouseDown(const MouseEvent& e) override
	{
		auto s = findParentComponentOfClass<SelectableComponent::Lasso>();
		s->getLassoSelection().addToSelectionBasedOnModifiers(this, e.mods);
	}

	void mouseMove(const MouseEvent& e) override
	{
		over = contains(e.getPosition());
		hoverPoint = e.position;
		repaint();
	}

	void mouseExit(const MouseEvent& e) override
	{
		over = false;
		repaint();
	}

	Point<float> hoverPoint;
	CablePinBase::WeakPtr src, dst;

	valuetree::PropertyListener sourceListener;
	valuetree::PropertyListener targetListener;

	valuetree::RemoveListener removeListener;
};

struct DspNetworkComponent : public Component,
							 public NodeComponent::Lasso,
							 public CableComponent::CableHolder
{
	enum class Action
	{
		Open,
		DeselectAll,
		Cut,
		Copy,
		Paste,
		Duplicate,
		Delete,
		ToggleVertical,
		ToggleEdit,
		AddComment,
		Undo,
		Redo,
		AutoLayout,
		AlignTop,
		AlignLeft,
		DistributeHorizontally,
		DistributeVertically,
		numActions
	};

	DspNetworkComponent(const ValueTree& v_) :
		CableHolder(v_),
		data(v_)
	{
		Helpers::updateChannelCount(data, false, &um);

		setWantsKeyboardFocus(true);

		addAndMakeVisible(lasso);

		lasso.setColour(LassoComponent<NodeComponent*>::ColourIds::lassoFillColourId, Colour(SIGNAL_COLOUR).withAlpha(0.05f));
		lasso.setColour(LassoComponent<NodeComponent*>::ColourIds::lassoOutlineColourId, Colour(SIGNAL_COLOUR));

		auto rootContainer = data.getChild(0);
		jassert(rootContainer.getType() == PropertyIds::Node);

		auto sortProcessNodes = !rootContainer.hasProperty(UIPropertyIds::width);

		Helpers::fixOverlap(rootContainer, &um, sortProcessNodes);

		addAndMakeVisible(rootComponent = new ContainerComponent(*this, rootContainer, &um));

		rootSizeListener.setCallback(rootContainer,
			{ UIPropertyIds::width, UIPropertyIds::height },
			Helpers::UIMode,
			VT_BIND_PROPERTY_LISTENER(onContainerResize));

		
		foldListener.setCallback(rootContainer, { PropertyIds::Folded }, Helpers::UIMode, VT_BIND_RECURSIVE_PROPERTY_LISTENER(onFold));

		Helpers::fixOverlap(rootContainer, &um, false);

		auto b = Helpers::getBounds(rootContainer, false);
		setSize(b.getWidth(), b.getHeight());
	}

	void resized() override
	{
		rebuildCables();
	}

	void onFold(const ValueTree& v, const Identifier& id)
	{
		for(auto c: cables)
		{
			auto sourceVisible = !Helpers::isFoldedRecursive(c->src->getNodeTree());
			auto targetVisible = !Helpers::isFoldedRecursive(c->dst->getNodeTree());
			
			c->setVisible(sourceVisible && targetVisible);
		}
	}

	void onContainerResize(const Identifier& id, const var& newValue)
	{
		auto b = Helpers::getBounds(rootComponent->getValueTree(), false);
		setSize(b.getWidth(), b.getHeight());
	}

	static bool isEditModeEnabled(const MouseEvent& e)
	{
		auto root = e.eventComponent->findParentComponentOfClass<DspNetworkComponent>();
		
		if(root != nullptr && root->editMode)
			return true;

		return e.mods.isCommandDown();
	}

	void paint(Graphics& g) override
	{
		g.fillAll(Colour(0xFF222222));

		if(editMode)
			GlobalHiseLookAndFeel::draw1PixelGrid(g, this, getLocalBounds());
	}

	Array<Component::SafePointer<NodeComponent>> currentlyDraggedComponents;
	Component::SafePointer<ContainerComponent> currentlyHoveredContainer;

	Point<int> pos;

	void mouseMove(const MouseEvent& e) override
	{
		pos = e.getPosition();
	}

	void mouseDown(const MouseEvent& e) override
	{
		CHECK_MIDDLE_MOUSE_DOWN(e);

		checkLassoEvent(e, SelectableComponent::LassoState::Down);
	}

	void mouseDrag(const MouseEvent& e) override
	{
		CHECK_MIDDLE_MOUSE_DRAG(e);

		if(ZoomableViewport::checkDragScroll(e, false))
			return;

		checkLassoEvent(e, SelectableComponent::LassoState::Drag);
	}

	void mouseUp(const MouseEvent& e) override
	{
		CHECK_MIDDLE_MOUSE_UP(e);

		if(ZoomableViewport::checkDragScroll(e, true))
			return;

		checkLassoEvent(e, SelectableComponent::LassoState::Up);

		if (!e.mouseWasDraggedSinceMouseDown())
			selection.deselectAll();
	}

	bool editMode = false;

	void toggleEditMode()
	{
		editMode = !editMode;
		repaint();
	}

	bool performAction(Action a)
	{
		switch(a)
		{
		case Action::Open:
			break;
		case Action::DeselectAll:
			getLassoSelection().deselectAll();
			return true;
		case Action::Cut:
			cutSelection();
			return true;
		case Action::Copy:
			copySelection();
			return true;
		case Action::Paste:
		{
			auto currentContainer = rootComponent->getInnerContainer(this, pos, nullptr);
			auto pastePos = currentContainer->getLocalPoint(this, pos);
			pasteSelection(pastePos);
			return true;
		}
		case Action::Duplicate:
			duplicateSelection();
			return true;
		case Action::Delete:
			deleteSelection();
			return true;
		case Action::ToggleVertical:
			for(auto s: createTreeListFromSelection(PropertyIds::Node))
				Helpers::setNodeProperty(s, PropertyIds::IsVertical, !Helpers::getNodeProperty(s, PropertyIds::IsVertical, true), &um);
			return true;
		case Action::ToggleEdit:
			toggleEditMode();
			return true;
		case Action::AddComment:

			for (auto s : selection.getItemArray())
			{
				if (s != nullptr && s->isNode())
					s->getValueTree().setProperty(PropertyIds::Comment, "Add comment...", &um);
			}

			return true;
		case Action::Undo:
			um.undo();
			return true;
		case Action::Redo:
			um.redo();
			return true;
		case Action::AutoLayout:
			for (auto s : selection.getItemArray())
			{
				if (s != nullptr && s->isNode())
					Helpers::resetLayout(s->getValueTree(), &um);
			}
			return true;
		case Action::AlignTop:
			LayoutTools::alignHorizontally(createTreeListFromSelection(PropertyIds::Node), &um);
			return true;
		case Action::AlignLeft:
			LayoutTools::alignVertically(createTreeListFromSelection(PropertyIds::Node), &um);
			return true;
		case Action::DistributeHorizontally:
			LayoutTools::distributeHorizontally(createTreeListFromSelection(PropertyIds::Node), &um);
			return true;
		case Action::DistributeVertically:
			LayoutTools::distributeVertically(createTreeListFromSelection(PropertyIds::Node), &um);
			return true;
		case Action::numActions:
			break;
		default:
			break;
		}
	}

	bool keyPressed(const KeyPress& k) override
	{
		if (k.getKeyCode() == 'D' && k.getModifiers().isCommandDown())
			return performAction(Action::Duplicate);
		if (k.getKeyCode() == 'Z' && k.getModifiers().isCommandDown())
			return performAction(Action::Undo);
		if (k.getKeyCode() == 'C' && k.getModifiers().isCommandDown())
			return performAction(Action::Copy);
		if (k.getKeyCode() == 'X' && k.getModifiers().isCommandDown())
			return performAction(Action::Cut);
		if(k.getKeyCode() == KeyPress::F4Key)
			return performAction(Action::ToggleEdit);
		if(k.getKeyCode() == KeyPress::escapeKey)
			return performAction(Action::DeselectAll);
		if(k.getKeyCode() == '#' && k.getModifiers().isCommandDown())
			return performAction(Action::AddComment);
		if (k.getKeyCode() == 'V' && k.getModifiers().isCommandDown())
			return performAction(Action::Paste);
		if (k.getKeyCode() == 'R' && k.getModifiers().isCommandDown())
			return performAction(Action::AutoLayout);
		if (k.getKeyCode() == 'Y' && k.getModifiers().isCommandDown())
			return performAction(Action::Redo);
		if (k.getKeyCode() == KeyPress::deleteKey)
			return performAction(Action::Delete);

		return false;
	}

	void findLassoItemsInArea(Array<SelectableComponent::WeakPtr>& itemsFound,
		const Rectangle<int>& area) override
	{
		callRecursive<SelectableComponent>(this, [&](SelectableComponent* nc)
		{
			if(auto c = dynamic_cast<CableBase*>(nc))
			{
				if(!itemsFound.contains(nc))
				{
					auto la = c->getLocalArea(this, area.toFloat());

					if (c->p.intersectsLine({ la.getTopLeft(), la.getBottomRight() }))
					{
						itemsFound.add(nc);
					}
				}
			}
			else
			{
				auto asComponent = dynamic_cast<Component*>(nc);
				auto nb = getLocalArea(asComponent, asComponent->getLocalBounds());

				if (area.intersects(nb) && !nb.contains(area))
				{
					auto p = asComponent->findParentComponentOfClass<SelectableComponent>();

					while (p != nullptr)
					{
						itemsFound.removeAllInstancesOf(p);
						asComponent = dynamic_cast<Component*>(p);
						p = asComponent->findParentComponentOfClass<SelectableComponent>();
					}

					itemsFound.addIfNotAlreadyThere(nc);
				}
			}

			return false;
		});
	}

	ValueTree data;

	ScopedPointer<ContainerComponent> rootComponent;
	valuetree::PropertyListener rootSizeListener;

	valuetree::RecursivePropertyListener foldListener;

	
};



}
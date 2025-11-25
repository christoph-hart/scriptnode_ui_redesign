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
		Helpers::createCustomizableCurve(p, s, e.translated(-3.0f, 0.0f), offset);
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

	float offset = 0.0f;
	float width = 0.333f;

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

	~DraggedCable();

	void paint(Graphics& g) override
	{
		CableBase::paint(g);

		String t;
		
		if(hoveredPin != nullptr)
		{
			t = "Connect to " + hoveredPin->getTargetParameterId();
		}
		else
		{
			t = "Create new parameter node";
		}

		auto f = GLOBAL_FONT();
		auto w = f.getStringWidth(t) + 20.0f;

		if(t.isNotEmpty())
		{
			auto b = Rectangle<float>(e, e).withSizeKeepingCentre(w, 20.0f).translated(-w * 0.5 - 20.0f, 0.0f);

			g.setColour(Colour(0xFF222222).withAlpha(0.7f));
			g.fillRoundedRectangle(b, 3.0f);
			g.setFont(f);
			g.setColour(Colours::white.withAlpha(0.7f));
			g.drawText(t, b, Justification::centred);
		}
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
		// TODO: Make better cable design functions (add curve points etc...)
		// drag upwards to make the cable more rectangly
		// drag left / right to move the center point
		// two properties for <Connection x="0.5", y="0.5">

		auto ids = UIPropertyIds::Helpers::getPositionIds();
		ids.add(PropertyIds::Folded);

		sourceListener.setCallback(src->getNodeTree(), ids, valuetree::AsyncMode::Asynchronously,
			VT_BIND_PROPERTY_LISTENER(updatePosition));

		targetListener.setCallback(dst->getNodeTree(), ids, valuetree::AsyncMode::Asynchronously,
			VT_BIND_PROPERTY_LISTENER(updatePosition));

		setInterceptsMouseClicks(false, false);
		setRepaintsOnMouseActivity(true);

		cableOffsetListener.setCallback(connectionTree, { UIPropertyIds::CableOffset }, Helpers::UIMode, VT_BIND_PROPERTY_LISTENER(onCableOffset));

		removeListener.setCallback(connectionTree, Helpers::UIMode, true, [this](const ValueTree& v)
		{
			auto ch = findParentComponentOfClass<CableHolder>();
			ch->rebuildCables();
		});
	}

	ValueTree connectionTree;

	void onCableOffset(const Identifier&, const var& newValue)
	{
		offset = (float)newValue;

		auto p = getParentComponent();

		if(p != nullptr)
		{
			rebuildPath(p->getLocalPoint(this, s), p->getLocalPoint(this, e), getParentComponent());
		}
	}

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
			
			rebuildPath(ns, ne.translated(-1.0f * (float)Helpers::ParameterMargin, 0.0f), parent);
		}
	}

	void paintOverChildren(Graphics& g) override
	{
		if(selected)
		{
			Path tp;
			PathStrokeType(4.0f).createStrokedPath(tp, p);

			g.setColour(Colour(SIGNAL_COLOUR));
			g.strokePath(tp, PathStrokeType(1));
		}
	}

	float downOffset = 0.0f;

	void mouseDrag(const MouseEvent& ev) override
	{
		auto deltaX = (float)ev.getDistanceFromDragStartX() / (float)getWidth();
		auto deltaY = (float)ev.getDistanceFromDragStartY() / (float)getHeight();

		if(e.getX() < s.getX())
		{
			if (s.getY() > e.getY())
				deltaY *= -1.0f;

			offset = jlimit(-1.0f, 1.0f, downOffset + deltaY);
		}
		else
			offset = jlimit(-0.5f, 0.5f, downOffset + deltaX);

		connectionTree.setProperty(UIPropertyIds::CableOffset, offset, src->um);
	}

	void mouseEnter(const MouseEvent& ev)
	{
		auto vertical = e.getX() < s.getX();

		setMouseCursor(vertical ? MouseCursor::UpDownResizeCursor : MouseCursor::LeftRightResizeCursor);
	}

	void mouseDown(const MouseEvent& e) override
	{
		downOffset = offset;

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
	valuetree::PropertyListener cableOffsetListener;

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
		CreateNew,
		ToggleVertical,
		ToggleEdit,
		AddComment,
		GroupSelection,
		Undo,
		Redo,
		AutoLayout,
		AlignTop,
		AlignLeft,
		SetColour,
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

		if(!currentBuildPath.isEmpty())
		{
			g.setColour(Colour(SIGNAL_COLOUR));
			g.strokePath(currentBuildPath, PathStrokeType(1.0f));
			g.setColour(Colour(SIGNAL_COLOUR).withAlpha(0.1f));
			g.fillPath(currentBuildPath);
		}
	}

	bool createSignalNodes = true;

	Point<int> pos;

	void mouseMove(const MouseEvent& e) override
	{
		pos = e.getPosition();
	}

	void mouseDown(const MouseEvent& e) override
	{
		CHECK_MIDDLE_MOUSE_DOWN(e);

		currentCreateNodePopup = nullptr;

		checkLassoEvent(e, SelectableComponent::LassoState::Down);
	}

	void mouseDrag(const MouseEvent& e) override
	{
		CHECK_MIDDLE_MOUSE_DRAG(e);

		if(ZoomableViewport::checkDragScroll(e, false))
			return;

		checkLassoEvent(e, SelectableComponent::LassoState::Drag);
	}

	void mouseDoubleClick(const MouseEvent& e) override
	{
		if (auto c = rootComponent->getInnerContainer(this, e.getPosition(), nullptr))
		{
			auto pos = c->getLocalPoint(this, e.position);
			c->renameGroup(pos);
		}
	}

	void mouseUp(const MouseEvent& e) override
	{
		CHECK_MIDDLE_MOUSE_UP(e);

		if(ZoomableViewport::checkDragScroll(e, true))
			return;

		checkLassoEvent(e, SelectableComponent::LassoState::Up);

		if (!e.mouseWasDraggedSinceMouseDown())
		{
			if (auto c = rootComponent->getInnerContainer(this, e.getPosition(), nullptr))
			{
				auto pos = c->getLocalPoint(this, e.position);

				if(!e.mods.isCommandDown())
					selection.deselectAll();

				if(c->selectGroup(pos))
					return;
			}

			selection.deselectAll();	
		}
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
		case Action::SetColour:
			colourSelection();
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
		case Action::GroupSelection:
			groupSelection();
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
		case Action::CreateNew:
		{
			if(auto first = selection.getItemArray().getFirst())
			{
				if(auto nc = dynamic_cast<NodeComponent*>(first.get()))
				{
					BuildHelpers::CreateData cd;

					Path buildPath;

					Point<int> pos;

					if(auto c = dynamic_cast<ContainerComponent*>(nc))
					{
						cd.containerToInsert = c->getValueTree();
						cd.pointInContainer = {};
						cd.signalIndex = c->childNodes.size();

						if(auto ab = c->addButtons.getLast())
						{
							buildPath = ab->hitzone;
							pos = getLocalPoint(ab, Point<int>());
							buildPath.applyTransform(AffineTransform::translation(pos.toFloat()));
						}
					}
					else
					{
						auto container = nc->findParentComponentOfClass<ContainerComponent>();

						cd.containerToInsert = container->getValueTree();
						cd.pointInContainer = nc->getBoundsInParent().getTopRight().translated(Helpers::NodeMargin / 2, 0);

						pos = getLocalPoint(container, cd.pointInContainer);

						findParentComponentOfClass<ZoomableViewport>()->scrollToRectangle(Rectangle<int>(pos, pos).withSizeKeepingCentre(400, 32), true);

						if (nc->hasSignal)
						{
							cd.signalIndex = nc->getValueTree().getParent().indexOf(nc->getValueTree()) + 1;

							if (auto ab = container->addButtons[cd.signalIndex])
							{
								buildPath = ab->hitzone;

								auto pos = getLocalPoint(ab, Point<float>());
								buildPath.applyTransform(AffineTransform::translation(pos));
							}
						}
						else
						{
							cd.source = nc->getFirstModOutput();
						}
					}

					showCreateNodePopup(pos, buildPath, cd);
				}
			}

			return true;
		}
		case Action::AddComment:

			for (auto s : selection.getItemArray())
			{
				String t = "Add comment...";

				if (s != nullptr && s->isNode())
				{
					auto desc = db.getDescriptions()[s->getValueTree()[PropertyIds::FactoryPath]];

					if(desc.isNotEmpty())
						t = desc;
				}
				
				s->getValueTree().setProperty(PropertyIds::Comment, t, &um);
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
		if (k.getKeyCode() == 'G' && k.getModifiers().isCommandDown())
			return performAction(Action::GroupSelection);
		if (k.getKeyCode() == KeyPress::deleteKey)
			return performAction(Action::Delete);
		if(k.getKeyCode() == 'N')
			return performAction(Action::CreateNew);

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

	struct CreateNodePopup: public Component,
							public TextEditorWithAutocompleteComponent,
							public ZoomableViewport::ZoomListener
	{
		struct Laf: public GlobalHiseLookAndFeel
		{
			void drawButtonBackground(Graphics& g, Button& b, const Colour& backgroundColour, bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override
			{
				Rectangle<float> r = b.getLocalBounds().toFloat().reduced(1.0f);

				Path p;

				auto f = b.getConnectedEdgeFlags();

				auto left = (f & Button::ConnectedOnLeft) != 0;
				auto right = (f & Button::ConnectedOnRight) != 0;

				p.addRoundedRectangle(r.getX(), r.getY(), r.getWidth(), r.getHeight(), r.getHeight() / 2.0f, r.getHeight() * 0.5f, !right, !left, !right, !left);
				g.setColour(b.findColour(TextButton::ColourIds::buttonColourId).withAlpha(1.0f));

				if(b.getToggleState())
					g.fillPath(p);
				else
					g.strokePath(p, PathStrokeType(1.0f));
			}

			void drawButtonText(Graphics &g, TextButton &button, bool , bool ) override
			{
				g.setColour(button.getToggleState() ? Colour(0xAA111111) : button.findColour(TextButton::ColourIds::buttonColourId).withAlpha(1.0f));
				g.setFont(GLOBAL_FONT());
				g.drawText(button.getButtonText(), button.getLocalBounds().toFloat(), Justification::centred);
			}
		} laf;

		std::map<String, String> descriptions;

		CreateNodePopup(DspNetworkComponent& parent, Point<int> pos, const BuildHelpers::CreateData& cd_) :
		  TextEditorWithAutocompleteComponent(),
		  cd(cd_),
		  root(&parent),
		  originalPosition(pos),
		  zp(parent.findParentComponentOfClass<ZoomableViewport>()),
		  descriptions(parent.db.getDescriptions())
		{
			zp->addZoomListener(this);

			addFactoryButton("All");

			for(auto f: parent.db.getFactoryIds(cd.source == nullptr))
			{
				addFactoryButton(f);
			}

			factoryButtons.getFirst()->setConnectedEdges(Button::ConnectedOnLeft);
			factoryButtons.getLast()->setConnectedEdges(Button::ConnectedOnRight);

			for(int i = 1; i < factoryButtons.size() - 1; i++)
			{
				factoryButtons[i]->setConnectedEdges(Button::ConnectedOnRight | Button::ConnectedOnLeft);
				factoryButtons[i]->setColour(TextButton::ColourIds::buttonColourId, Helpers::getFadeColour(i, factoryButtons.size()));
			}

			factoryButtons.getLast()->setColour(TextButton::ColourIds::buttonColourId, Helpers::getFadeColour(factoryButtons.size()-1, factoryButtons.size()));
			

			useDynamicAutocomplete = true;

			auto tl = dynamic_cast<Component*>(parent.findParentComponentOfClass<Parent>());

			tl->addAndMakeVisible(this);

			addAndMakeVisible(editor);
			initEditor();

			updateTextEditorOnItemChange = true;

			int h = 0;

			h += 10;
			h += 24;
			h += 32;
			h += 5;
			h += 24;
			h += 10;

			int w = 10;

			for(auto tb: factoryButtons)
				w += tb->getWidth();

			w += 10;

			setSize(jmax(w, 300), h);
			GlobalHiseLookAndFeel::setTextEditorColours(editor);
			editor.setColour(TextEditor::ColourIds::highlightedTextColourId, Colours::black);
			editor.setColour(CaretComponent::ColourIds::caretColourId, Colours::black);
			editor.grabKeyboardFocusAsync();
			editor.setTextToShowWhenEmpty("Type to create node", Colours::grey);

			



			zp->setSuspendWASD(true);
			//zp->scrollToRectangle(getBoundsInParent(), true);

			updatePosition();
		}

		Point<int> originalPosition;

		void updatePosition()
		{
			auto tl = dynamic_cast<Component*>(root->findParentComponentOfClass<Parent>());
			setTopLeftPosition(tl->getLocalPoint(root, originalPosition));
		}

		void addFactoryButton(const String& factoryId)
		{
			auto nb = new TextButton(factoryId);
			nb->setLookAndFeel(&laf);
			addAndMakeVisible(nb);
			factoryButtons.add(nb);

			nb->setClickingTogglesState(true);
			nb->setRadioGroupId(9012421);
			nb->onClick = [this, nb]()
			{
				auto t = nb->getButtonText();

				if(t == "All")
					t = {};
				else
					t << ".";

				setFactory(t, nb->findColour(TextButton::ColourIds::buttonColourId));
			};

			auto w = GLOBAL_FONT().getStringWidth(factoryId) + 20;

			nb->setSize(w, 24);
		}

		String currentDescription;
		Colour currentFactoryColour;
		String currentFactory;
		Rectangle<float> factoryLabel;

		void setFactory(const String& factory, Colour c)
		{
			if(factory != currentFactory)
			{
				currentFactory = factory;
				currentFactoryColour = c;
				resized();
				repaint();
				dismissAutocomplete();
				editor.setText("", dontSendNotification);
				showAutocomplete({});
				editor.setText(autocompleteItems[0], dontSendNotification);
				editor.grabKeyboardFocusAsync();
			}
		}

		OwnedArray<TextButton> factoryButtons;

		ZoomableViewport* zp;

		void zoomChanged(float) override {}

		void positionChanged(Point<int>) override
		{
			updatePosition();
		}

		~CreateNodePopup()
		{
			if(root != nullptr)
			{
				root->currentBuildPath = {};
				root->repaint();
			}
			
			if(zp != nullptr)
			{
				zp->removeZoomListener(this);
				zp->setSuspendWASD(false);
			}
				
		}

		void textEditorReturnKeyPressed(TextEditor& te) override;

		void dismiss()
		{
			if(root.getComponent() != nullptr)
				root->currentCreateNodePopup = nullptr;
		}

		void textEditorEscapeKeyPressed(TextEditor& te) override
		{
			TextEditorWithAutocompleteComponent::textEditorEscapeKeyPressed(te);
			dismiss();
		}

		void textEditorTextChanged(TextEditor& te) override
		{
			TextEditorWithAutocompleteComponent::textEditorTextChanged(te);

			if(autocompleteItems.contains(te.getText()))
				textEditorReturnKeyPressed(te);
		}

		TextEditor* getTextEditor() override { return &editor; }

		Identifier getIdForAutocomplete() const override 
		{ 
			if(currentFactory.isEmpty())
				return {};

			return Identifier(currentFactory); 
		}
		
		void autoCompleteItemSelected(int selectedIndex, const String& item) override
		{
			TextEditorWithAutocompleteComponent::autoCompleteItemSelected(selectedIndex, item);

			auto fullPath = currentFactory + item;

			if(descriptions.find(fullPath) != descriptions.end())
				currentDescription = descriptions.at(fullPath);
			else
				currentDescription = {};

			repaint();
		}

		void resized() override
		{
			auto b = getLocalBounds().reduced(10);

			auto w = b.getWidth() / factoryButtons.size();

			auto top = b.removeFromTop(24);

			for(auto fb: factoryButtons)
			{
				fb->setTopLeftPosition(top.getTopLeft());
				top.removeFromLeft((fb->getWidth()));
			}

			descriptionBounds = b.removeFromTop(32).toFloat();

			if(currentFactory.isNotEmpty())
			{
				auto intend = GLOBAL_BOLD_FONT().getStringWidth(currentFactory) + 10;
				factoryLabel = b.removeFromLeft(intend).toFloat();
			}

			editor.setBounds(b);
		}

		void paint(Graphics& g) override
		{
			auto b = getLocalBounds().toFloat();

			DropShadow sh;
			sh.colour = Colours::black.withAlpha(0.7f);
			sh.radius = 5;
			sh.drawForRectangle(g, b.toNearestInt().reduced(5));

			g.setColour(Colour(0xFF222222));
			g.fillRect(b.reduced(5.0f));
			g.setColour(Colours::white.withAlpha(0.5f));
			g.drawRoundedRectangle(b.reduced(2.0f), 5.0f, 2.0f); 

			if(currentDescription.isNotEmpty())
			{
				g.setFont(GLOBAL_FONT());
				g.setColour(Colours::white.withAlpha(0.7f));
				g.drawText(currentDescription, descriptionBounds, Justification::left);
			}

			if(currentFactory.isNotEmpty())
			{
				g.setColour(currentFactoryColour);
				g.setFont(GLOBAL_BOLD_FONT());
				g.drawText(currentFactory, factoryLabel, Justification::left);
			}
		}

		TextEditor editor;

		Rectangle<float> descriptionBounds;

		BuildHelpers::CreateData cd;

		Component::SafePointer<DspNetworkComponent> root;
	};

	void showCreateNodePopup(Point<int> position, const Path& buildPath, const BuildHelpers::CreateData& cd)
	{
		selection.deselectAll();

		currentCreateNodePopup = nullptr;

		createSignalNodes = cd.signalIndex != -1;

		currentBuildPath = buildPath;

		currentCreateNodePopup = new CreateNodePopup(*this, position, cd);
		currentCreateNodePopup->showAutocomplete({});

		repaint();
	}

	Path currentBuildPath;

	ScopedPointer<CreateNodePopup> currentCreateNodePopup;

	ValueTree data;

	ScopedPointer<ContainerComponent> rootComponent;
	valuetree::PropertyListener rootSizeListener;

	valuetree::RecursivePropertyListener foldListener;

	void setIsDragged(NodeComponent* nc);

	void clearDraggedComponents();

	void resetDraggedBounds();

	Component::SafePointer<ContainerComponent> currentlyHoveredContainer;

private:

	Array<Component::SafePointer<NodeComponent>> currentlyDraggedComponents;
	

	
};



}
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

struct CableBase: public Component,
				  public ChangeListener
{
	bool hitTest(int x, int y) override
	{
		Point<float> sp((float)x, (float)y);
		Point<float> tp;
		p.getNearestPoint(sp, tp);

		return sp.getDistanceFrom(tp) < 5.0f;
	}

	CableBase(SelectableComponent::Lasso* lassoToSync_):
	  lassoToSync(lassoToSync_)
	{
		if(lassoToSync != nullptr)
		{
			lassoToSync->getLassoSelection().addChangeListener(this);
		}
	}

	virtual ~CableBase()
	{
		if(lassoToSync.get() != nullptr)
			lassoToSync->getLassoSelection().removeChangeListener(this);
	}

	void changeListenerCallback(ChangeBroadcaster*) override
	{
		jassert(lassoToSync != nullptr);

		auto asSelectable = dynamic_cast<SelectableComponent*>(this);
		jassert(asSelectable != nullptr);

		auto isSelected = asSelectable->selected;
		auto shouldBeSelected = false;

		for(auto& s: lassoToSync->getLassoSelection().getItemArray())
		{
			if(s == asSelectable)
				return; // job done, nothing to do

			shouldBeSelected |= s != nullptr && s->getValueTree() == asSelectable->getValueTree();
		}

		if(!isSelected && shouldBeSelected)
			lassoToSync->getLassoSelection().addToSelection(asSelectable);
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

		arrow.clear();
		arrow.startNewSubPath(e);
		arrow.lineTo(e.translated(-7.0f, 5.0f));
		arrow.lineTo(e.translated(-7.0f, -5.0f));
		arrow.closeSubPath();

		repaint();
	}

	void paint(Graphics& g) override
	{
		if(colour1 == colour2)
			g.setColour(colour1);
		else
			g.setGradientFill(ColourGradient(colour1, s, colour2, e, false));
		g.strokePath(p, PathStrokeType(over ? 3.0f : 1.0f));

		
		g.fillEllipse(Rectangle<float>(s, s).withSizeKeepingCentre(6.0f, 6.0f));
		
		g.fillPath(arrow);
	}

	Path p;
	Path arrow;
	bool over = false;
	Point<float> s, e;

	float offset = 0.0f;
	float width = 0.333f;

	Colour colour1;
	Colour colour2;

	WeakReference<SelectableComponent::Lasso> lassoToSync;
};

struct DraggedCable: public CableBase
{
	DraggedCable(CablePinBase* src_):
	  CableBase(nullptr),
	  src(src_)
	{
		colour1 = Colour(SIGNAL_COLOUR);
		colour2 = colour1;
	}

	~DraggedCable();

	void paint(Graphics& g) override
	{
		CableBase::paint(g);

		auto f = GLOBAL_FONT();
		auto w = f.getStringWidth(label) + 20.0f;

		if(label.isNotEmpty() && getWidth() > w)
		{
			auto b = Rectangle<float>(e, e).withSizeKeepingCentre(w, 20.0f).translated(-w * 0.5 - 20.0f, 0.0f);

			g.setColour(Colour(0xFF222222).withAlpha(0.7f));
			g.fillRoundedRectangle(b, 3.0f);
			g.setFont(f);
			g.setColour(Colours::white.withAlpha(0.7f));
			g.drawText(label, b, Justification::centred);
		}
	}

	void setTargetPosition(Point<int> rootPosition);


	String label;
	bool ok = true;

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
					pos = pos.translated(-20, -10);
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
				g.setColour(Colour(0xdd222222));
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
		CableHolder(const ValueTree& v):
		  blinker(*this)
		{
			connectionListener.setTypesToWatch({
			PropertyIds::Connections,
			PropertyIds::ModulationTargets,
			PropertyIds::ReceiveTargets
				});

			hideConnectionListener.setCallback(v, { UIPropertyIds::HideCable }, Helpers::UIMode, VT_BIND_RECURSIVE_PROPERTY_LISTENER(onHideCable));

			connectionListener.setCallback(v, Helpers::UIMode, VT_BIND_CHILD_LISTENER(onConnectionChange));
			initialised = true;
		}

		virtual ~CableHolder() = default;

		bool initialised = false;

		void rebuildCables();

		void onHideCable(const ValueTree& v, const Identifier& id)
		{
			rebuildCables();
		}

		struct Stub: public SelectableComponent,
					 public CableBase
		{
			enum class Attachment
			{
				Source,
				Target,
				numAttachments
			};

			Stub(CableHolder& parent_, CablePinBase* source_, CablePinBase* target_, Attachment attachment_):
			  SelectableComponent(dynamic_cast<Lasso*>(&parent_)),
			  CableBase(dynamic_cast<Lasso*>(&parent_)),
			  parent(parent_),
			  source(source_),
			  originalTarget(target_),
			  attachment(attachment_)
			{
				auto targetPath = ParameterHelpers::getParameterPath(originalTarget->data);

				colour1 = ParameterHelpers::getParameterColour(source->data);
				colour2 = Helpers::getNodeColour(valuetree::Helpers::findParentWithType(originalTarget->data, PropertyIds::Node));

				setMouseCursor(MouseCursor::PointingHandCursor);

				valuetree::Helpers::forEach(source->data, [&](ValueTree& v)
				{
					if(v.getType() == PropertyIds::Connection)
					{
						if(ParameterHelpers::getParameterPath(v) == targetPath)
						{
							connection = v;
							return true;
						}
					}

					return false;
				});

				auto w = GLOBAL_FONT().getStringWidth(getTextToDisplay()) + 20;
				auto parentComponent = dynamic_cast<Component*>(&parent);

				Point<float> start, end;

				if(attachment == Attachment::Source)
				{
					auto lp = parentComponent->getLocalArea(source, source->getLocalBounds());
					start = Point<int>(lp.getRight() - 3.0f, lp.getCentreY()).toFloat();
					end = start.translated(50.0f, 0.0f);

					while (auto existing = dynamic_cast<Stub*>(parentComponent->getComponentAt(end)))
					{
						end = end.translated(0, lp.getHeight());
						offset -= 0.05f;
					}
				}
				else
				{

					auto lp = parentComponent->getLocalArea(originalTarget, originalTarget->getLocalBounds());

					end = Point<int>(lp.getX() - 3.0f - w, lp.getCentreY()).toFloat();
					start = end.translated(-50.0f, 0.0f);
				}

				dynamic_cast<Component*>(&parent)->addAndMakeVisible(this);
				toBack();
				rebuildPath(start, end, dynamic_cast<Component*>(&parent));
				setSize(getWidth() + w, getHeight());

				auto lEnd = getLocalPoint(parentComponent, end);

				textBounds = { lEnd.getX(), lEnd.getY() - 10.0f, (float)w, 20.0f };
				
				if(attachment == Attachment::Target)
				{
					s = s.translated(w-10.f, 0.0f);
					e = e.translated(w-10.f, 0.0f);
					textBounds = textBounds.withX(0.0f);
					p.applyTransform(AffineTransform::translation(w - 10.0f, 0.0f));
					arrow.applyTransform(AffineTransform::translation(w - 10.0f, 0.0f));
				}
			}

			ValueTree getValueTree() const override
			{
				return connection;
			}

			const Attachment attachment;

			String getTextToDisplay() const
			{
				if(attachment == Attachment::Source)
				{
					if (originalTarget != nullptr)
					{
						return "to " + ParameterHelpers::getParameterPath(originalTarget->data);
					}
				}
				else
				{
					if(source != nullptr)
					{
						return "from " + source->getSourceDescription();
					}
				}
				
				
				return {};
			}

			void mouseEnter(const MouseEvent& e) override
			{
				parent.blinker.blink(attachment == Attachment::Source ? originalTarget : source);
			}

			void mouseExit(const MouseEvent& e) override
			{
				parent.blinker.unblink(attachment == Attachment::Source ? originalTarget : source);
			}

			void mouseDoubleClick(const MouseEvent& event) override
			{
				auto ta = dynamic_cast<Component*>(&parent)->getLocalArea(originalTarget.get(), originalTarget->getLocalBounds());
				findParentComponentOfClass<ZoomableViewport>()->scrollToRectangle(ta, true, true);
			}

			void mouseDown(const MouseEvent& e) override
			{
				auto l = findParentComponentOfClass<SelectableComponent::Lasso>();
				l->getLassoSelection().addToSelectionBasedOnModifiers(this, e.mods);
			}

			bool hitTest(int x, int y) override
			{
				return textBounds.contains(Point<float>(x, y));
			}

			

			void paint(Graphics& g) override
			{
				CableBase::paint(g);

				auto c = attachment == Attachment::Source ? colour2 : colour1;

				g.setColour(c.withAlpha(0.1f));
				g.fillRoundedRectangle(textBounds.reduced(1.0f), 3.0f);

				g.setColour(c);

				if(selected)
				{
					g.setColour(Colour(SIGNAL_COLOUR));

					Path s;
					PathStrokeType(3.0f).createStrokedPath(s, p);
					g.strokePath(s, PathStrokeType(1.0f));
					g.fillPath(arrow);
				}
				
				g.setFont(GLOBAL_FONT());
				g.drawText(getTextToDisplay(), textBounds, Justification::centred);
			}

			CableHolder& parent;
			WeakReference<CablePinBase> source;
			WeakReference<CablePinBase> originalTarget;
			ValueTree connection;

			Rectangle<float> textBounds;

		};

		struct Blinker: public Timer
		{
			Blinker(CableHolder& p):
			  parent(p)
			{};

			void blink(WeakReference<CablePinBase> target)
			{
				if (target == nullptr)
					return;

				for(auto& s: blinkTargets)
				{
					if(s.first == target)
					{
						s.second = 10;
						startTimer(15);
						return;
					}
				}

				blinkTargets.add({ target, 10});
				startTimer(15);
			}

			void unblink(CablePinBase::WeakPtr target)
			{
				if(target == nullptr)
					return;

				for (int i = 0; i < blinkTargets.size(); i++)
				{
					if (blinkTargets[i].first == target)
						blinkTargets.remove(i--);
				}

				target->setBlinkAlpha(0.0f);
			}

			void timerCallback() override
			{
				for(auto& s: blinkTargets)
				{
					if(s.first != nullptr)
					{
						s.first->setBlinkAlpha((float)s.second / 10.0f);
						--s.second;
					}
				}

				for(int i = 0; i < blinkTargets.size(); i++)
				{
					if(blinkTargets[i].second == 0)
						blinkTargets.remove(i--);
				}

				if(blinkTargets.isEmpty())
					stopTimer();
			}

			Array<std::pair<CablePinBase::WeakPtr, int>> blinkTargets;

			CableHolder& parent;
		} blinker;

		OwnedArray<CableComponent> cables;
		OwnedArray<CableLabel> labels;
		OwnedArray<Stub> stubs;
		ScopedPointer<DraggedCable> currentlyDraggedCable;

		void onConnectionChange(const ValueTree& v, bool wasAdded)
		{
			rebuildCables();
		}

		valuetree::RecursivePropertyListener hideConnectionListener;
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
		CableBase(&l),
		SelectableComponent(&l),
		src(src_),
		dst(dst_),
		connectionTree(getConnectionTree(src_, dst_))
	{
		auto ids = UIPropertyIds::Helpers::getPositionIds();

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
		return src->getSourceDescription();
#if 0
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
#endif
	}

	void updatePosition(const Identifier& id, const var&)
	{
		auto parent = dynamic_cast<Component*>(findParentComponentOfClass<CableHolder>());

		if (parent == nullptr || src == nullptr || dst == nullptr)
			return;

		auto start = parent->getLocalArea(src, src->getLocalBounds()).toFloat();

		auto ns = Point<float>(start.getRight() - 3.0f, start.getCentreY());
		auto end = parent->getLocalArea(dst, dst->getLocalBounds()).toFloat();
		auto ne = Point<float>(end.getX(), end.getCentreY());

		rebuildPath(ns, ne.translated(-1.0f * (float)Helpers::ParameterMargin, 0.0f), parent);
	}

	void paintOverChildren(Graphics& g) override
	{
		if(selected)
		{
			Path tp;
			PathStrokeType(4.0f).createStrokedPath(tp, p);

			g.setColour(Colour(SIGNAL_COLOUR));
			g.strokePath(tp, PathStrokeType(1));
			g.fillPath(arrow);
		}
	}

	float downOffset = 0.0f;

	void mouseDrag(const MouseEvent& ev) override
	{
		auto deltaX = (float)ev.getDistanceFromDragStartX();
		auto deltaY = (float)ev.getDistanceFromDragStartY();

		if(e.getX() < s.getX())
		{
			if (s.getY() > e.getY())
				deltaY *= -1.0f;

			offset = downOffset + deltaY;
		}
		else
			offset = downOffset + deltaX;

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
	struct Parent: public TextEditorWithAutocompleteComponent::Parent
	{
		virtual ~Parent() = default;

		Component*  asComponent() { return dynamic_cast<Component*>(this); }

		static Parent* getParent(Component* c)
		{
			return c->findParentComponentOfClass<Parent>();
		}

		ZoomableViewport* getViewport()
		{
			ZoomableViewport* zp = nullptr;

			Component::callRecursive<ZoomableViewport>(asComponent(), [&](ZoomableViewport* zp_)
			{
				zp = zp_;
				return true;
			});

			return zp;
		}

		struct Map: public Component
		{
			struct Item
			{
				void draw(Graphics& g, const AffineTransform& tr)
				{
					auto rb = area.transformed(tr);

					g.setColour(c);

					g.drawRect(rb, 1);

					auto tb = rb.removeFromTop(13.0f);

					g.setColour(c.withAlpha(0.1f));
					g.fillRect(tb);
					g.setFont(GLOBAL_FONT().withHeight(10.0f));
					g.setColour(Colours::white);
					g.drawText(name, tb, Justification::centred);

				}

				Rectangle<float> area;
				String name;
				Colour c;
			};

			Map(const ValueTree& v, Parent& p_, Rectangle<int> viewPosition_):
			  p(p_)
			{
				setMouseCursor(MouseCursor::DraggingHandCursor);

				fullBounds = Helpers::getBounds(v, false).toFloat();
				fullBounds.setWidth((int)v[UIPropertyIds::width]);
				fullBounds.setHeight((int)v[UIPropertyIds::height]);
				sc = AffineTransform::scale(fullBounds.getWidth(), fullBounds.getHeight(), 0.0f, 0.0f).inverted();

				viewPosition = viewPosition_.toFloat().transformed(sc);

				Helpers::forEachVisibleNode(v, [&](const ValueTree& n)
				{
					auto x = Helpers::getBoundsInRoot(n, false).toFloat();

					if(n == v)
						x = fullBounds;

					x = x.translated(-fullBounds.getX(), -fullBounds.getY());
					x = x.transformed(sc);

					Item ni;
					ni.area = x;
					ni.name = Helpers::getHeaderTitle(n);
					ni.c = Helpers::getNodeColour(n);

					items.add(ni);
				}, true);

				auto w = 500;
				auto h = w / fullBounds.getWidth() * fullBounds.getHeight();
				setSize(w, h);

				tr = AffineTransform::scale((float)w, (float)h, 0.0f, 0.0f);
			}

			Array<Item> items;

			Rectangle<float> viewPosition;
			Rectangle<float> downPosition;

			void mouseDown(const MouseEvent& e) override
			{
				auto pos = e.position.transformedBy(tr.inverted());

				if(!viewPosition.contains(pos))
				{
					auto np = viewPosition;
					np.setCentre(pos.getX(), pos.getY());
					updatePosition(np);
				}

				downPosition = viewPosition;
				repaint();
			}

			void updatePosition(Rectangle<float> newPos)
			{
				newPos = newPos.constrainedWithin(Rectangle<float>(0.0f, 0.0f, 1.0f, 1.0f));

				if(newPos != viewPosition)
				{
					viewPosition = newPos;

					auto newArea = viewPosition.transformedBy(sc.inverted()).toNearestInt();
					p.getViewport()->scrollToRectangle(newArea, true, false);

					repaint();
				}
			}

			void mouseDrag(const MouseEvent& e) override
			{
				Point<float> delta(e.getDistanceFromDragStartX(), e.getDistanceFromDragStartY());

				delta = delta.transformedBy(tr.inverted());

				updatePosition(downPosition.translated(delta.getX(), delta.getY()));
			}

			void paint(Graphics& g) override
			{
				g.fillAll(Colour(0xEE333333));

				auto b = getLocalBounds().toFloat();
				
				g.setColour(Colours::white.withAlpha(0.2f));
				g.fillRect(viewPosition.transformed(tr));

				for(auto i: items)
				{
					i.draw(g, tr);
				}
			}

			Parent& p;
			AffineTransform tr; // NORM ->  MAP BOUNDS
			AffineTransform sc; // NORM -> FULL BOUNDS
			Rectangle<float> fullBounds;
		};

		void showMap(const ValueTree& v, Rectangle<int> viewPosition, Point<int> position)
		{
			if(map == nullptr)
			{
				asComponent()->addAndMakeVisible(map = new Map(v, *this, viewPosition));
				
				map->setCentrePosition(position);
			}
			else
			{
				map = nullptr;
			}
		}

		StringArray getAutocompleteItems(const Identifier& id) override
		{
			auto list = db.getNodeIds(createSignalNodes);

			if (id.isValid())
			{
				auto prefix = id.toString();

				StringArray matches;
				matches.ensureStorageAllocated(list.size());

				for (const auto& l : list)
				{
					if (l.startsWith(prefix))
						matches.add(l.fromFirstOccurrenceOf(prefix, false, false));
				}

				return matches;
			}

			return list;
		}

		bool createSignalNodes = false;
		NodeDatabase db;
		ScopedPointer<Map> map;
	};

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
		CollapseContainer,
		GroupSelection,
		Undo,
		Redo,
		AutoLayout,
		AlignTop,
		AlignLeft,
		SetColour,
		HideCable,
		DistributeHorizontally,
		DistributeVertically,
		ShowMap,
		numActions
	};

	DspNetworkComponent(const ValueTree& networkTree, const ValueTree& container) :
		CableHolder(networkTree),
		data(networkTree)
	{
		calloutLAF.getCurrentColourScheme().setUIColour(LookAndFeel_V4::ColourScheme::widgetBackground, Colour(0xFF353535));

		Helpers::migrateFeedbackConnections(data, true, nullptr);
		Helpers::updateChannelCount(data, false, nullptr);

		valuetree::Helpers::forEach(networkTree, [&](ValueTree& n)
		{
			if(n.getType() == PropertyIds::Node)
				n.setProperty(UIPropertyIds::CurrentRoot, n == container, &um);

			return false;
		});

		setWantsKeyboardFocus(true);

		addAndMakeVisible(lasso);

		lasso.setColour(LassoComponent<NodeComponent*>::ColourIds::lassoFillColourId, Colour(SIGNAL_COLOUR).withAlpha(0.05f));
		lasso.setColour(LassoComponent<NodeComponent*>::ColourIds::lassoOutlineColourId, Colour(SIGNAL_COLOUR));

		auto rootContainer = container;
		jassert(rootContainer.getType() == PropertyIds::Node);

		auto sortProcessNodes = !rootContainer.hasProperty(UIPropertyIds::width);

		Helpers::fixOverlap(rootContainer, &um, sortProcessNodes);

		addAndMakeVisible(rootComponent = new ContainerComponent(this, rootContainer, &um));

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
		SafeAsyncCall::call<DspNetworkComponent>(*this, [](DspNetworkComponent& d)
		{
			d.rebuildCables();
		});
	
		return;

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

		for(const auto& p: snapshotPositions)
		{
			Path p1, p2, p3, p4;

			auto tl = p.second.viewport.getTopLeft().toFloat();
			auto tr = p.second.viewport.getTopRight().toFloat();
			auto bl = p.second.viewport.getBottomLeft().toFloat();
			auto br = p.second.viewport.getBottomRight().toFloat();

			const float w = 15.0f;

			p1.startNewSubPath(tl.translated(0, w));
			p1.lineTo(tl);
			p1.lineTo(tl.translated(w, 0.0f));

			p2.startNewSubPath(tr.translated(0, w));
			p2.lineTo(tr);
			p2.lineTo(tr.translated(-w, 0.0f));

			p3.startNewSubPath(bl.translated(0, -w));
			p3.lineTo(bl);
			p3.lineTo(bl.translated(w, 0.0f));

			p4.startNewSubPath(br.translated(0, -w));
			p4.lineTo(br);
			p4.lineTo(br.translated(-w, 0.0f));

			PathStrokeType ps(2.0f);

			g.setColour(Colours::white.withAlpha(0.25f));
			
			g.strokePath(p1.createPathWithRoundedCorners(2.0f), ps);
			g.strokePath(p2.createPathWithRoundedCorners(2.0f), ps);
			g.strokePath(p3.createPathWithRoundedCorners(2.0f), ps);
			g.strokePath(p4.createPathWithRoundedCorners(2.0f), ps);

			g.setFont(GLOBAL_FONT());

			g.drawText(String(&p.first, 1), p.second.viewport.toFloat().reduced(4.0f, 2.0f), Justification::topRight);
			g.drawText(String(&p.first, 1), p.second.viewport.toFloat().reduced(4.0f, 2.0f), Justification::topLeft);
			g.drawText(String(&p.first, 1), p.second.viewport.toFloat().reduced(4.0f, 2.0f), Justification::bottomLeft);
			g.drawText(String(&p.first, 1), p.second.viewport.toFloat().reduced(4.0f, 2.0f), Justification::bottomRight);
		}
	}

	

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
		case Action::HideCable:
		{
			auto treeSelection = createTreeListFromSelection(PropertyIds::Connection);

			for (auto s : treeSelection)
				s.setProperty(UIPropertyIds::HideCable, !s[UIPropertyIds::HideCable], &um);

			MessageManager::callAsync([this, treeSelection]()
			{
				getLassoSelection().deselectAll();

				callRecursive<CableComponent>(this, [&](CableComponent* c)
				{
					if(treeSelection.contains(c->getValueTree()))
						getLassoSelection().addToSelection(c);

					return false;
				});
			});

			return true;
		}
		case Action::ShowMap:
		{
			auto p = Parent::getParent(this);
			auto lp = p->asComponent()->getLocalPoint(this, pos);
			p->showMap(rootComponent->getValueTree(), getCurrentViewPosition(), lp);

			return true;
		}
		case Action::ToggleEdit:
			toggleEditMode();
			return true;
		case Action::CollapseContainer:

			for(auto c: createTreeListFromSelection(PropertyIds::Node))
				c.setProperty(PropertyIds::Locked, !c[PropertyIds::Locked], &um);

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
				if(auto cc = dynamic_cast<CableComponent*>(first.get()))
				{
					auto pos = cc->getBoundsInParent().getCentre();

					auto delta = cc->getPosition();

					auto buildPath = cc->p;
					buildPath.applyTransform(AffineTransform::translation(delta));

					BuildHelpers::CreateData cd;

					auto container = cc->src->findParentComponentOfClass<ContainerComponent>();

					auto sourceBounds = Helpers::getBounds(cc->src->getNodeTree(), true);

					auto newX = sourceBounds.getRight() + Helpers::NodeMargin;
					auto newY = sourceBounds.getY();

					cd.containerToInsert = container->getValueTree();
					cd.pointInContainer = { newX, newY };
					cd.source = cc->src;
					cd.target = cc->dst;

					showCreateNodePopup(pos, buildPath, cd);
				}
			}
			else
			{
				showCreateConnectionPopup(pos);
			}

			return true;
		}
		case Action::AddComment:

			if(selection.getNumSelected() == 0)
			{
				if(auto c = rootComponent->getInnerContainer(rootComponent, pos, nullptr))
				{
					auto cTree = c->getValueTree().getOrCreateChildWithName(UIPropertyIds::Comments, &um);

					auto lp = c->getLocalPoint(this, pos);

					ValueTree nc(PropertyIds::Comment);

					nc.setProperty(PropertyIds::Comment, "Add comment", nullptr);
					nc.setProperty(UIPropertyIds::CommentOffsetX, lp.getX(), nullptr);
					nc.setProperty(UIPropertyIds::CommentOffsetY, lp.getY(), nullptr);
					nc.setProperty(UIPropertyIds::CommentWidth, 500, nullptr);
					
					cTree.addChild(nc, -1, &um);
				}
			}
			else
			{
				for (auto s : selection.getItemArray())
				{
					String t = "Add comment...";

					if (s != nullptr && s->isNode())
					{
						auto desc = db.getDescriptions()[s->getValueTree()[PropertyIds::FactoryPath]];

						if (desc.isNotEmpty())
							t = desc;
					}

					s->getValueTree().setProperty(PropertyIds::Comment, t, &um);
				}
			}

			return true;
		case Action::Undo:
			um.undo();
			return true;
		case Action::Redo:
			um.redo();
			return true;
		case Action::AutoLayout:
			if(selection.getNumSelected() == 0)
			{
				Helpers::resetLayout(rootComponent->getValueTree(), &um);
			}
			else
			{
				for (auto s : selection.getItemArray())
				{
					if (s != nullptr && s->isNode())
						Helpers::resetLayout(s->getValueTree(), &um);
				}
			}


			
			return true;
		case Action::AlignTop:
		{
		
			LayoutTools::alignHorizontally(createTreeListFromSelection(PropertyIds::Node), &um);

			Array<LayoutTools::CableData> cableList;

			for(auto s: selection.getItemArray())
			{
				if(auto c = dynamic_cast<CableComponent*>(s.get()))
				{
					LayoutTools::CableData cd;
					cd.con = s->getValueTree();
					cd.s = getLocalPoint(c, c->s);
					cd.e = getLocalPoint(c, c->e);
					cableList.add(cd);
				}
			}

			LayoutTools::alignCables(cableList, &um);
			return true;
		}
		case Action::AlignLeft:
			LayoutTools::alignVertically(createTreeListFromSelection(PropertyIds::Node), &um);
			return true;
		case Action::DistributeHorizontally:
			LayoutTools::distributeHorizontally(createTreeListFromSelection(PropertyIds::Node), &um);
			LayoutTools::distributeCableOffsets(createTreeListFromSelection(PropertyIds::Connection), &um);
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

	Rectangle<int> getCurrentViewPosition() const
	{
		auto fullBounds = getLocalBounds();
		fullBounds.removeFromTop(Helpers::HeaderHeight);
		auto parentBounds = getParentComponent()->getLocalBounds();
		parentBounds = getLocalArea(getParentComponent(), parentBounds);
		auto s = parentBounds.getIntersection(fullBounds.reduced(20));
		return s;
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
		if (k.getKeyCode() == 'L' && k.getModifiers().isCommandDown())
			return performAction(Action::CollapseContainer);
		if (k.getKeyCode() == KeyPress::deleteKey)
			return performAction(Action::Delete);
		if(k.getKeyCode() == 'N')
			return performAction(Action::CreateNew);
		if (k.getKeyCode() == 'M')
			return performAction(Action::ShowMap);
		if(k.getKeyCode() == 'H')
			return performAction(Action::HideCable);

		if(k.getKeyCode() == '1' || k.getKeyCode() == '2' || k.getKeyCode() == '3' || k.getKeyCode() == '4')
		{
			auto store = k.getModifiers().isCommandDown();

			if(store)
			{
				if(snapshotPositions.find(k.getKeyCode()) != snapshotPositions.end())
				{
					snapshotPositions.erase(k.getKeyCode());
					repaint();
					return true;
				}
				
				auto s = getCurrentViewPosition();

				

				snapshotPositions[(char)k.getKeyCode()] = { data, s.reduced(20) };
				repaint();
			}
			else
			{
				if (snapshotPositions.find(k.getKeyCode()) != snapshotPositions.end())
				{
					snapshotPositions.at(k.getKeyCode()).restore(*findParentComponentOfClass<ZoomableViewport>(), &um);

					//->zoomToRectangle(snapshotPositions[(char)k.getKeyCode()]);
				}
			}

			return true;
		}

		if(k.getKeyCode() == KeyPress::leftKey || k.getKeyCode() == KeyPress::rightKey || k.getKeyCode() == KeyPress::downKey || k.getKeyCode() == KeyPress::upKey)
		{
			return navigateSelection(k);
		}

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



	struct CreateConnectionPopup: public Component
	{
		struct Item: public Component,
					 public PathFactory
		{
			static constexpr int Height = 24;

			Item(CablePinBase::WeakPtr src, CreateConnectionPopup& parent):
			  source(src),
			  name(source->getSourceDescription()),
			  target(createPath("target")),
			  c(ParameterHelpers::getParameterColour(src->data))
			{
				parent.content.addAndMakeVisible(this);

				auto w = GLOBAL_BOLD_FONT().getStringWidth(name) + 20 + Height;

				setBounds(0, parent.items.size() * Height, w, Height);
				parent.items.add(this);
				parent.content.setSize(w, parent.items.size() * Height);
				setMouseCursor(MouseCursor::CrosshairCursor);
				setRepaintsOnMouseActivity(true);
			}

			Path createPath(const String& url) const override
			{
				Path p;
				LOAD_EPATH_IF_URL("target", ColumnIcons::targetIcon);
				return p;
			}

			void mouseDown(const MouseEvent& e) override
			{
				if(source != nullptr)
					source->mouseDown(e.getEventRelativeTo(source));
			}

			void mouseDrag(const MouseEvent& e) override
			{
				if (source != nullptr)
					source->mouseDrag(e.getEventRelativeTo(source));
			}

			void mouseUp(const MouseEvent& e) override
			{
				if (source != nullptr)
					source->mouseUp(e.getEventRelativeTo(source));

				findParentComponentOfClass<DspNetworkComponent>()->currentCreateConnectionPopup = nullptr;
			}

			void paint(Graphics& g) override
			{
				auto b = getLocalBounds().toFloat();

				g.setColour(Colours::black.withAlpha(0.2f));
				g.fillRoundedRectangle(b.reduced(1.0f), 3.0f);

				float alpha = 0.7f;

				if(isMouseOver())
					alpha += 0.1f;

				if(isMouseOverOrDragging())
					alpha += 0.1f;


				g.setColour(c.withAlpha(alpha));
				g.fillPath(target);
				g.setFont(GLOBAL_FONT());
				g.drawText(name, tb, Justification::left);
			}

			void resized() override
			{
				auto b = getLocalBounds().toFloat();

				if(!b.isEmpty())
				{
					scalePath(target, b.removeFromRight(b.getHeight()).reduced(3.0f));
					b.removeFromLeft(5.0f);
					tb = b;
					repaint();
				}
			}

			Colour c;
			CablePinBase::WeakPtr source;
			Rectangle<float> tb;
			Path target;
			String name;
		};

		void paint(Graphics& g) override
		{
			auto b = getLocalBounds();
			g.setColour(Colour(0xEE252525));
			g.fillRoundedRectangle(b.reduced(3).toFloat(), 10.0f);

			g.setColour(Colours::white.withAlpha(0.6f));
			g.drawRoundedRectangle(b.reduced(1.0f).toFloat(), 3.0f, 2.0f);

			auto tb = b.removeFromTop(Helpers::HeaderHeight).toFloat();
			g.setFont(GLOBAL_BOLD_FONT());
			g.setColour(Colours::white);
			g.drawText("Create connection from source", tb, Justification::centred);
		}

		void resized() override
		{
			auto b = getLocalBounds();

			b = b.reduced(10);
			b.removeFromTop(Helpers::HeaderHeight);

			vp.setBounds(b);

			auto w = b.getWidth() - vp.getScrollBarThickness();

			for(auto i: items)
				i->setSize(w, Item::Height);

			content.setSize(w, content.getHeight());
		}

		Component content;

		CreateConnectionPopup(DspNetworkComponent& parent_):
		  parent(parent_)
		{
			addAndMakeVisible(vp);

			vp.setViewedComponent(&content, false);
			sf.addScrollBarToAnimate(vp.getVerticalScrollBar());
			
			std::map<String, CablePinBase::WeakPtr> sources;

			callRecursive<CablePinBase>(&parent, [&](CablePinBase* p)
			{
				if(p->isDraggable())
					sources[p->getSourceDescription()] = p;

				return false;
			});

			auto w = 200;

			for(auto s: sources)
			{
				new Item(s.second, *this);
				w = jmax(w, items.getLast()->getWidth());
			}

			setSize(w, jmin(items.size() * Item::Height, 300) + Helpers::HeaderHeight + 20);
		}

		OwnedArray<Item> items;
		ScrollbarFader sf;
		DspNetworkComponent& parent;
		Viewport vp;
	};

	struct CreateNodePopup: public Component,
							public TextEditorWithAutocompleteComponent,
							public ZoomableViewport::ZoomListener
	{
		struct Laf: public GlobalHiseLookAndFeel,
					public TextEditorWithAutocompleteComponent::LookAndFeelMethods
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

			void drawAutocompleteItem(Graphics& g, TextEditorWithAutocompleteComponent& parent, const String& itemName, Rectangle<float> itemBounds, bool selected) override
			{
				NodeDatabase db;

				if(selected)
				{
					g.setColour(Colours::white.withAlpha(0.05f));
					g.fillRoundedRectangle(itemBounds.translated(-5.0f, 0.0f), 4.0f);
				}

				auto description = db.getDescriptions()[itemName];

				auto bf = GLOBAL_BOLD_FONT();
				auto f = GLOBAL_FONT();

				auto factory = itemName.upToFirstOccurrenceOf(".", false, false);
				auto nodeId = itemName.fromFirstOccurrenceOf(".", true, false);

				auto fIds = db.getFactoryIds(true);

				auto idx = fIds.indexOf(factory);

				g.setFont(GLOBAL_BOLD_FONT());
				g.setColour(Helpers::getFadeColour(idx, fIds.size()).withAlpha(1.0f));

				g.drawText(factory, itemBounds.removeFromLeft(bf.getStringWidthFloat(factory) + 2.0f), Justification::left);
				
				g.setColour(Colours::white.withAlpha(0.8f));

				g.drawText(nodeId, itemBounds.removeFromLeft(bf.getStringWidthFloat(nodeId) + 2.0f), Justification::left);
				
				g.setColour(Colours::white.withAlpha(0.5f));

				itemBounds.removeFromLeft(5.0f);

				g.setFont(GLOBAL_FONT());
				g.drawText(description, itemBounds, Justification::left);
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
			setLookAndFeel(&laf);

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
			
			itemsToShow = 7;
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

			setSize(jmax(w, 500), h);
			GlobalHiseLookAndFeel::setTextEditorColours(editor);
			editor.setColour(TextEditor::ColourIds::highlightedTextColourId, Colours::black);
			editor.setColour(CaretComponent::ColourIds::caretColourId, Colours::black);
			editor.grabKeyboardFocusAsync();
			editor.setTextToShowWhenEmpty("Type to create node", Colours::grey);

			factoryButtons[0]->setToggleState(true, dontSendNotification);

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

		Parent::getParent(this)->createSignalNodes = cd.signalIndex != -1;

		currentBuildPath = buildPath;

		currentCreateNodePopup = new CreateNodePopup(*this, position, cd);
		currentCreateNodePopup->showAutocomplete({});

		repaint();
	}

	Path currentBuildPath;

	ScopedPointer<CreateNodePopup> currentCreateNodePopup;
	ScopedPointer<CreateConnectionPopup> currentCreateConnectionPopup;

	ValueTree data;

	ScopedPointer<ContainerComponent> rootComponent;
	valuetree::PropertyListener rootSizeListener;

	valuetree::RecursivePropertyListener foldListener;

	void setIsDragged(NodeComponent* nc);

	static void showPopup(Component* popupComponent, Component* target)
	{
		auto root = target->findParentComponentOfClass<DspNetworkComponent>();

		std::unique_ptr<Component> pc;

		pc.reset(popupComponent);

		auto b = root->getLocalArea(target, target->getLocalBounds());

		auto& cb = CallOutBox::launchAsynchronously(std::move(pc), b, root);

		cb.setLookAndFeel(&root->calloutLAF);

		MessageManager::callAsync([root]()
		{
			root->rebuildCables();
		});

	}

	void clearDraggedComponents();

	void resetDraggedBounds();

	Component::SafePointer<ContainerComponent> currentlyHoveredContainer;

	static void switchRootNode(Component* c, ValueTree& newRoot)
	{
		if(auto vp = c->findParentComponentOfClass<ZoomableViewport>())
		{
			auto root = valuetree::Helpers::findParentWithType(newRoot, PropertyIds::Network);
			auto n = new DspNetworkComponent(root,newRoot);
			vp->setNewContent(n, nullptr);
		}
	}

private:

	LookAndFeel_V4 calloutLAF;

	struct DraggedNode
	{
		void addToParent(DspNetworkComponent& root)
		{
			if(originalParent != nullptr && node != nullptr)
			{
				node->setAlpha(1.0f);

				root.removeChildComponent(node);
				originalParent->addChildComponent(node);
				originalParent->cables.updatePins(*originalParent);
				node->setBounds(originalBounds);
			}
		}

		void removeFromParent(DspNetworkComponent& root)
		{
			if (originalParent != nullptr && node != nullptr)
			{
				originalBounds = node->getBoundsInParent();

				root.currentlyDraggedComponents.add(*this);
				originalParent->removeChildComponent(node);
				root.addChildComponent(node);

				originalParent->cables.updatePins(*originalParent);
				node->setAlpha(0.5f);
				auto rootBounds = root.getLocalArea(originalParent, originalBounds);
				node->setBounds(rootBounds);
			}
		}

		Rectangle<int> originalBounds;
		Component::SafePointer<ContainerComponent> originalParent;
		Component::SafePointer<NodeComponent> node;
	};

	Array<DraggedNode> currentlyDraggedComponents;
	
	struct SnapShot
	{
		SnapShot() = default;

		SnapShot(const ValueTree& rootTree, Rectangle<int> viewport_):
		  viewport(viewport_)
		{
			valuetree::Helpers::forEach(rootTree, [&](const ValueTree& n)
			{
				if(n.getType() == PropertyIds::Node)
				{
					Item ni;
					ni.v = n;
					ni.pos = Helpers::getBounds(n, false);
					ni.folded = n[PropertyIds::Folded];
					items.push_back(ni);
				}

				return false;
			});
		}

		void restore(ZoomableViewport& zp, UndoManager* um)
		{
			for(auto& i: items)
			{
				i.v.setProperty(PropertyIds::Folded, i.folded, um);
				Helpers::updateBounds(i.v, i.pos, um);
			}

			ZoomableViewport* z = &zp;
			auto pos = viewport;

			MessageManager::callAsync([z, pos]()
			{
				z->zoomToRectangle(pos.expanded(20));
			});
		}

		struct Item
		{
			ValueTree v;
			Rectangle<int> pos;
			bool folded;
		};

		Rectangle<int> viewport;
		std::vector<Item> items;
	};

	std::map<char, SnapShot> snapshotPositions;

	JUCE_DECLARE_WEAK_REFERENCEABLE(DspNetworkComponent);
	
	void showCreateConnectionPopup(Point<int> pos);
};



}
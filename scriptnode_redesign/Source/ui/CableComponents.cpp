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

namespace scriptnode {
using namespace hise;
using namespace juce;

CableBase::CableBase(SelectableComponent::Lasso* lassoToSync_) :
	lassoToSync(lassoToSync_)
{
	if (lassoToSync != nullptr)
	{
		lassoToSync->getLassoSelection().addChangeListener(this);
	}
}



CableBase::~CableBase()
{
	if (lassoToSync.get() != nullptr)
		lassoToSync->getLassoSelection().removeChangeListener(this);
}

bool CableBase::hitTest(int x, int y)
{
	Point<float> sp((float)x, (float)y);
	Point<float> tp;
	p.getNearestPoint(sp, tp);

	return sp.getDistanceFrom(tp) < 5.0f;
}

void CableBase::changeListenerCallback(ChangeBroadcaster*)
{
	jassert(lassoToSync != nullptr);

	auto asSelectable = dynamic_cast<SelectableComponent*>(this);
	jassert(asSelectable != nullptr);

	auto isSelected = asSelectable->selected;
	auto shouldBeSelected = false;

	for (auto& s : lassoToSync->getLassoSelection().getItemArray())
	{
		if (s == asSelectable)
			return; // job done, nothing to do

		shouldBeSelected |= s != nullptr && s->getValueTree() == asSelectable->getValueTree();
	}

	if (!isSelected && shouldBeSelected)
		lassoToSync->getLassoSelection().addToSelection(asSelectable);
}

void CableBase::rebuildPath(Point<float> newStart, Point<float> newEnd, Component* parent)
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

void CableBase::paint(Graphics& g)
{
	auto lod = LODManager::getLOD(*this);

	if (colour1 == colour2)
		g.setColour(colour1);
	else
		g.setGradientFill(ColourGradient(colour1, s, colour2, e, false));

	auto strokeDepth = LayoutTools::getCableThickness(lod);

	g.strokePath(p, PathStrokeType((over ? 3.0f : 1.0f) * strokeDepth));

	if (lod == 0)
		g.fillEllipse(Rectangle<float>(s, s).withSizeKeepingCentre(6.0f, 6.0f));

	g.fillPath(arrow);
}

DraggedCable::DraggedCable(CablePinBase* src_) :
	CableBase(nullptr),
	src(src_)
{
	colour1 = Colour(SIGNAL_COLOUR);
	colour2 = colour1;
}

DraggedCable::~DraggedCable()
{
	auto root = src->findParentComponentOfClass<DspNetworkComponent>();
	auto pos = root->getLocalPoint(this, e.toInt());

	if(hoveredAddButton != nullptr)
	{
		BuildHelpers::CreateData cd;

		auto container = hoveredAddButton->findParentComponentOfClass<ContainerComponent>();

		cd.containerToInsert = container->getValueTree();
		cd.isCableNode = false;
		cd.source = src;
		cd.pointInContainer = container->getLocalPoint(root, pos);
		cd.signalIndex = hoveredAddButton->index;

		auto p = hoveredAddButton->hitzone;

		auto delta = root->getLocalPoint(hoveredAddButton, Point<int>());
		p.applyTransform(AffineTransform::translation(delta));

		root->showCreateNodePopup(pos, this, p, cd);

		return;
	}

	if (hoveredPin == nullptr && ok)
	{
		BuildHelpers::CreateData cd;

		auto container = root->rootComponent->getInnerContainer(root, pos, nullptr);

		cd.containerToInsert = container->getValueTree();
		cd.pointInContainer = container->getLocalPoint(root, pos);
		cd.source = src;
		cd.isCableNode = true;

		auto delta = getPosition();


		p.applyTransform(AffineTransform::translation(delta));

		root->showCreateNodePopup(pos, this, p, cd);
	}
	
}

void DraggedCable::paint(Graphics& g)
{
	CableBase::paint(g);

	auto f = GLOBAL_FONT();
	auto w = f.getStringWidth(label) + 20.0f;

	if (label.isNotEmpty() && getWidth() > w)
	{
		auto b = Rectangle<float>(e, e).withSizeKeepingCentre(w, 20.0f).translated(-w * 0.5 - 20.0f, 0.0f);

		g.setColour(Colour(0xFF222222).withAlpha(0.7f));
		g.fillRoundedRectangle(b, 3.0f);
		g.setFont(f);
		g.setColour(Colours::white.withAlpha(0.7f));
		g.drawText(label, b, Justification::centred);
	}
}

void DraggedCable::setTargetPosition(Point<int> rootPosition)
{
	if(hoveredAddButton != nullptr)
	{
		hoveredAddButton->active = false;
		hoveredAddButton->repaint();
		hoveredAddButton = nullptr;
	}

	ok = true;
	auto parent = getParentComponent();

	auto start = parent->getLocalArea(src, src->getLocalBounds()).toFloat();
	auto ns = Point<float>(start.getRight(), start.getCentreY());

	auto root = findParentComponentOfClass<DspNetworkComponent>();

	CablePinBase* currentPin = nullptr;
	ContainerComponent::AddButton* currentAddButton = nullptr;

	Rectangle<int> pinArea;

	callRecursive<CablePinBase>(root, [&](CablePinBase* pin)
	{
		auto area = root->getLocalArea(pin, pin->getLocalBounds());

		if (area.contains(rootPosition))
		{
			currentPin = pin;
			pinArea = area;
			return true;
		}

		return false;
	});

	callRecursive<ContainerComponent::AddButton>(root, [&](ContainerComponent::AddButton* b)
	{
		auto area = root->getLocalArea(b, b->getLocalBounds());

		if(area.contains(rootPosition))
		{
			currentAddButton = b;
			return true;
		}
			

		return false;
	});

	auto ne = rootPosition;
	over = false;
	colour1 = Colours::white.withAlpha(0.5f);
	colour2 = colour1;

	hoveredPin = nullptr;

	if (currentPin != nullptr && currentPin != src.get() && currentPin->canBeTarget())
	{
		if (currentPin->matchesConnectionType(src->getConnectionType()))
		{
			auto result = currentPin->getTargetErrorMessage(src->data);

			if (!result.wasOk())
			{
				label = result.getErrorMessage();
				colour1 = Colour(HISE_ERROR_COLOUR);
				ok = false;
			}
			else
			{
				colour1 = Colour(SIGNAL_COLOUR);
				hoveredPin = currentPin;
				label = "Connect to " + hoveredPin->getTargetParameterId();
				over = true;
			}
		}
		else
		{
			label = "Invalid connection type";
			colour1 = Colour(HISE_ERROR_COLOUR);
		}

		colour2 = colour1;

		ne = Point<int>(pinArea.getX(), pinArea.getCentreY());
	}
	else if (currentAddButton != nullptr)
	{
		currentAddButton->active = true;
		currentAddButton->repaint();

		hoveredAddButton = currentAddButton;

		label = "Insert node to signal path";
	}
	else
	{
		

		label = "Create new parameter node";
	}

	rebuildPath(ns, ne.toFloat(), parent);
}

struct CableComponent::CableHolder::Stub : public SelectableComponent,
	public CableBase
{
	enum class Attachment
	{
		Source,
		Target,
		numAttachments
	};

	Stub(CableHolder& parent_, CablePinBase* source_, CablePinBase* target_, Attachment attachment_) :
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
				if (v.getType() == PropertyIds::Connection)
				{
					if (ParameterHelpers::getParameterPath(v) == targetPath)
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

		if (attachment == Attachment::Source)
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

		// Don't add cable for targets that are not visible
		auto sourceVisible = !source->isFoldedAway();
		auto targetVisible = !originalTarget->isFoldedAway();

		auto lEnd = getLocalPoint(parentComponent, end);

		textBounds = { lEnd.getX(), lEnd.getY() - 10.0f, (float)w, 20.0f };

		if (attachment == Attachment::Target)
		{
			setVisible(targetVisible);
			s = s.translated(w - 10.f, 0.0f);
			e = e.translated(w - 10.f, 0.0f);
			textBounds = textBounds.withX(0.0f);
			p.applyTransform(AffineTransform::translation(w - 10.0f, 0.0f));
			arrow.applyTransform(AffineTransform::translation(w - 10.0f, 0.0f));
		}
		else
		{
			setVisible(sourceVisible);
		}
	}

	ValueTree getValueTree() const override
	{
		return connection;
	}

	UndoManager* getUndoManager() const override { jassertfalse; return nullptr; }
	InvertableParameterRange getParameterRange(int index) const override { jassertfalse; return {}; }
	double getParameterValue(int index) const override { jassertfalse; return {}; }

	const Attachment attachment;

	String getTextToDisplay() const
	{
		if (attachment == Attachment::Source)
		{
			if (originalTarget != nullptr)
			{
				return "to " + ParameterHelpers::getParameterPath(originalTarget->data);
			}
		}
		else
		{
			if (source != nullptr)
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
		NetworkParent::scrollToRectangle(this, ta, true);
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

		LODManager::LODGraphics lg(g, *this);

		lg.fillRoundedRectangle(textBounds.reduced(1.0f), 3.0f);

		g.setColour(c);

		if (selected)
		{
			g.setColour(Colour(SIGNAL_COLOUR));

			Path s;
			PathStrokeType(3.0f).createStrokedPath(s, p);
			g.strokePath(s, PathStrokeType(1.0f));
			g.fillPath(arrow);
		}

		g.setFont(GLOBAL_FONT());
		lg.drawText(getTextToDisplay(), textBounds, Justification::centred);
	}

	CableHolder& parent;
	WeakReference<CablePinBase> source;
	WeakReference<CablePinBase> originalTarget;
	ValueTree connection;

	Rectangle<float> textBounds;

};

CableComponent::CableHolder::CableHolder(const ValueTree& v) :
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

void CableComponent::CableHolder::rebuildCables()
{
	if (!initialised)
		return;

	auto ok = false;

	OwnedArray<CableComponent> newCables;

	std::map<CablePinBase*, CablePinBase::Connections> allConnections;

	Array<CablePinBase::WeakPtr> lockedRootSources;

	auto asComponent = dynamic_cast<Component*>(this);

	Component::callRecursive<ContainerComponentBase>(asComponent, [&](ContainerComponentBase* cc)
	{
		auto cTree = cc->asNodeComponent().getValueTree();
		auto isRoot = Helpers::isRootNode(cTree);

		cc->rebuildOutsideParameters();

		auto isLockedRoot = isRoot && cTree.getParent().getType() != PropertyIds::Network;

		if (isLockedRoot)
		{
			for (int i = 0; i < cc->getNumOutsideParameters(); i++)
			{
				auto p = cc->getOutsideParameter(i);
				auto con = p->rebuildConnections(asComponent);

				if (!con.empty())
					allConnections[p] = std::move(con);

				lockedRootSources.addIfNotAlreadyThere(p);
			}
		}

		return false;
	});

	Component::callRecursive<ModOutputComponent>(asComponent, [&](ModOutputComponent* mc)
	{
		auto con = mc->rebuildConnections(asComponent);

		if (!con.empty())
			allConnections[mc] = std::move(con);

		return false;
	});

	Component::callRecursive<ProcessNodeComponent::RoutableSignalComponent>(asComponent, [&](ProcessNodeComponent::RoutableSignalComponent* rs)
	{
		if (!rs->canBeTarget())
		{
			auto con = rs->rebuildConnections(asComponent);

			if (!con.empty())
				allConnections[rs] = std::move(con);
		}

		return false;
	});

	Component::callRecursive<ContainerComponent>(asComponent, [&](ContainerComponent* c)
	{
		for (int i = 0; i < c->getNumParameters(); i++)
		{
			auto p = c->getDraggableParameterComponent(i);
			
			auto con = p->rebuildConnections(asComponent);

			if (!con.empty())
				allConnections[p] = std::move(con);
		}

		return false;
	});



	std::map<CablePinBase*, CablePinBase::Connections> localConnections;

	stubs.clear();

	for (auto& c : allConnections)
	{
		for (auto it = c.second.begin(); it != c.second.end();)
		{
			CablePinBase* src = c.first;
			CablePinBase* dst = *it;

			auto con = ParameterHelpers::getConnection(dst->data);

			// Check whether to add a cable
			auto shouldHide = con.isValid() && con[UIPropertyIds::HideCable];

			// Force a cable for sources that are the root of a locked container
			auto forceCable = lockedRootSources.contains(src);

			if (!forceCable && (shouldHide))
			{
				auto targetContainer = dst->findParentComponentOfClass<ContainerComponent>();

				if (targetContainer != nullptr)
				{
					if (auto localSource = targetContainer->getOutsideParameter(c.first->data))
					{
						localConnections[localSource].push_back(dst);

						stubs.add(new Stub(*this, src, dst, Stub::Attachment::Source));

						it = c.second.erase(it);
						continue;
					}
				}

				stubs.add(new Stub(*this, src, dst, Stub::Attachment::Source));

				stubs.add(new Stub(*this, src, dst, Stub::Attachment::Target));

				it = c.second.erase(it);
				continue;
			}

			++it;
		}
	}

	allConnections.merge(localConnections);

	cables.clear();
	labels.clear();

	for (const auto& c : allConnections)
	{
		auto src = c.first;

		for (const auto& dst : c.second)
		{
			if (src->isFoldedAway() || dst->isFoldedAway())
				continue;

			auto asLasso = dynamic_cast<SelectableComponent::Lasso*>(this);

			auto nc = new CableComponent(*asLasso, src, dst);

			cables.add(nc);
			asComponent->addAndMakeVisible(nc);

			//auto sourceVisible = !Helpers::isFoldedRecursive(src->getNodeTree());
			//auto targetVisible = !Helpers::isFoldedRecursive(dst->getNodeTree());

			//nc->setVisible(sourceVisible && targetVisible);

			nc->updatePosition({}, {});

			nc->colour1 = ParameterHelpers::getParameterColour(src->data);

			auto n = dst->data;

			if (n.getType() != PropertyIds::Node)
				n = Helpers::findParentNode(n);

			nc->colour2 = Helpers::getNodeColour(n);

			nc->toBack();
		}
	}

	for (auto c : cables)
	{
		asComponent->addChildComponent(labels.add(new CableLabel(c)));
		labels.getLast()->updatePosition();
	}
}

CableComponent::CableLabel::CableLabel(CableComponent* c) :
	ComponentMovementWatcher(c),
	attachedCable(c),
	currentText(attachedCable->getLabelText())
{
	auto w = GLOBAL_FONT().getStringWidth(currentText) + 10;
	setSize(w, 20);
	updatePosition();
}

void CableComponent::CableLabel::updatePosition()
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

void CableComponent::CableLabel::componentVisibilityChanged()
{
	triggerAsyncUpdate();
}

void CableComponent::CableLabel::paint(Graphics& g)
{
	LODManager::LODGraphics lg(g, *this);

	if (attachedCable.getComponent() != nullptr)
	{
		g.setFont(GLOBAL_FONT());
		g.setColour(Colour(0xdd222222));

		lg.fillRoundedRectangle(getLocalBounds().toFloat(), (float)getHeight() * 0.5f);
		g.setColour(attachedCable->colour2);
		lg.drawText(currentText, getLocalBounds().toFloat().reduced(5.0f, 0.0f), Justification::right);
	}
}

CableComponent::CableHolder::Blinker::Blinker(CableHolder& p) :
	parent(p)
{

}

void CableComponent::CableHolder::Blinker::blink(WeakReference<CablePinBase> target)
{
	if (target == nullptr)
		return;

	for (auto& s : blinkTargets)
	{
		if (s.first == target)
		{
			s.second = 10;
			startTimer(15);
			return;
		}
	}

	blinkTargets.add({ target, 10 });
	startTimer(15);
}

void CableComponent::CableHolder::Blinker::unblink(CablePinBase::WeakPtr target)
{
	if (target == nullptr)
		return;

	for (int i = 0; i < blinkTargets.size(); i++)
	{
		if (blinkTargets[i].first == target)
			blinkTargets.remove(i--);
	}

	target->setBlinkAlpha(0.0f);
}

void CableComponent::CableHolder::Blinker::timerCallback()
{
	for (auto& s : blinkTargets)
	{
		if (s.first != nullptr)
		{
			s.first->setBlinkAlpha((float)s.second / 10.0f);
			--s.second;
		}
	}

	for (int i = 0; i < blinkTargets.size(); i++)
	{
		if (blinkTargets[i].second == 0)
			blinkTargets.remove(i--);
	}

	if (blinkTargets.isEmpty())
		stopTimer();
}

juce::ValueTree CableComponent::getConnectionTree(CablePinBase* src, CablePinBase* dst)
{
	auto nodeId = dst->getNodeTree()[PropertyIds::ID].toString();
	auto parameterId = dst->getTargetParameterId();

	auto conTree = src->getConnectionTree();

	for (auto c : conTree)
	{
		if (c[PropertyIds::NodeId].toString() == nodeId &&
			c[PropertyIds::ParameterId].toString() == parameterId)
			return c;
	}

	jassertfalse;
	return {};
}

CableComponent::CableComponent(Lasso& l, CablePinBase* src_, CablePinBase* dst_) :
	CableBase(&l),
	SelectableComponent(&l),
	SimpleTimer(l.getUpdater()),
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

void CableComponent::onCableOffset(const Identifier&, const var& newValue)
{
	offset = (float)newValue;

	auto p = getParentComponent();

	if (p != nullptr)
	{
		rebuildPath(p->getLocalPoint(this, s), p->getLocalPoint(this, e), getParentComponent());
	}
}

void CableComponent::updatePosition(const Identifier& id, const var&)
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

juce::ValueTree CableComponent::getValueTree() const
{
	return connectionTree;
}

juce::String CableComponent::getLabelText() const
{
	return src->getSourceDescription();
}

void CableComponent::paintOverChildren(Graphics& g)
{
	if(changeAlpha > 0)
	{
		auto alpha = (float)changeAlpha / 10.0f;
		g.setColour(Colours::white.withAlpha(alpha));
		g.strokePath(p, PathStrokeType(1.0f + alpha));
		g.fillPath(arrow);
	}

	if (selected)
	{
		Path tp;
		PathStrokeType(4.0f).createStrokedPath(tp, p);

		g.setColour(Colour(SIGNAL_COLOUR));
		g.strokePath(tp, PathStrokeType(1));
		g.fillPath(arrow);
	}
}

void CableComponent::mouseDrag(const MouseEvent& ev)
{
	auto deltaX = (float)ev.getDistanceFromDragStartX();
	auto deltaY = (float)ev.getDistanceFromDragStartY();

	if (e.getX() < s.getX())
	{
		if (s.getY() > e.getY())
			deltaY *= -1.0f;

		offset = downOffset + deltaY;
	}
	else
		offset = downOffset + deltaX;

	connectionTree.setProperty(UIPropertyIds::CableOffset, offset, src->um);
}

void CableComponent::mouseEnter(const MouseEvent& ev)
{
	auto vertical = e.getX() < s.getX();

	setMouseCursor(vertical ? MouseCursor::UpDownResizeCursor : MouseCursor::LeftRightResizeCursor);
}

void CableComponent::mouseDown(const MouseEvent& e)
{
	if(e.mods.isRightButtonDown())
	{
		auto updater = findParentComponentOfClass<SelectableComponent::Lasso>()->getUpdater();

		auto p = NetworkParent::getNetworkParent(this); 
		p->showPopup(new CablePopup(updater, src, dst), this, {});
	}

	downOffset = offset;

	auto s = findParentComponentOfClass<SelectableComponent::Lasso>();
	s->getLassoSelection().addToSelectionBasedOnModifiers(this, e.mods);
}

void CableComponent::mouseMove(const MouseEvent& e)
{
	over = contains(e.getPosition());
	hoverPoint = e.position;
	repaint();
}

void CableComponent::mouseExit(const MouseEvent& e)
{
	over = false;
	repaint();
}

}
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

ContainerComponent::CableSetup::CableSetup(ContainerComponent& parent_, const ValueTree& v) :
	parent(parent_)
{
	pin = Helpers::createPinHole();
	init(v);

	if (type == ContainerType::Branch)
		branchListener.setCallback(v.getChildWithName(PropertyIds::Parameters).getChild(0), { PropertyIds::Value }, Helpers::UIMode, VT_BIND_PROPERTY_LISTENER(onBranch));
}

bool ContainerComponent::CableSetup::isSerialType() const
{
	return type == ContainerType::Serial || type == ContainerType::ModChain;
}

void ContainerComponent::CableSetup::onBranch(const Identifier&, const var& newValue)
{
	activeBranch = (int)newValue;

	connections.clear();

	for (int i = 0; i < numNodes; i++)
	{
		MultiChannelConnection mc;

		for (int c = 0; c < numChannels; c++)
			mc.push_back({ c, c, i == activeBranch });

		connections.push_back(mc);
	}

	updatePins(parent);
}

void ContainerComponent::CableSetup::updatePins(ContainerComponent& p)
{
	
	pinPositions.containerStart = { 0.0f, (float)Helpers::HeaderHeight };
	pinPositions.containerEnd = pinPositions.containerStart.translated((float)p.getWidth(), 0.0f);
	pinPositions.containerStart = pinPositions.containerStart.translated(Helpers::ParameterMargin + Helpers::ContainerParameterWidth - 25.0f, 0.0f);

	pinPositions.childPositions.clear();

	if(Helpers::isFoldedOrLockedContainer(p.getValueTree()))
	{
		p.repaint();
		return;
	}

	int idx = 0;

	Array<NodeComponent*> processNodes;

	for(auto cn: p.childNodes)
	{
		if (cn == nullptr || cn->getParentComponent() != &parent) // probably dragged
			continue;

		if(Helpers::isProcessingSignal(cn->getValueTree()))
		{
			processNodes.add(cn);
		}
	}

	for(int i = 0; i < processNodes.size(); i++)
	{
		auto cn = processNodes[i];
		

		if (isForcedVertical)
		{
			auto sb = cn->getBoundsInParent().toFloat();

			Point<float> t1(sb.getCentreX(), sb.getY());
			Point<float> t2(sb.getCentreX(), sb.getBottom());

			auto first = i == 0;
			auto last = i == processNodes.size()-1;

			if (first)
				t1 = { sb.getX(), sb.getY() + Helpers::HeaderHeight };

			if (last)
				t2 = { sb.getRight(), sb.getY() + Helpers::HeaderHeight };

			pinPositions.childPositions.push_back({ t1, t2, i });
		}
		else
		{
			Rectangle<float> sb(0.0f, (float)Helpers::HeaderHeight, (float)cn->getWidth(), (float)Helpers::SignalHeight);
			sb = p.getLocalArea(cn, sb);
			pinPositions.childPositions.push_back({ sb.getTopLeft(), sb.getTopRight(), i });
		}
	}

	if(!isSerialType() && pinPositions.childPositions.size() != connections.size())
	{
		init(parent.getValueTree());
	}

	parent.addButtons.clear();

	std::vector<std::pair<Path, int>> hitzonePaths;

	auto addPath = [&](Point<float> s, Point<float> e, int dropIndex, bool isVertical)
	{
		Path p;

		auto sh = (float)Helpers::SignalHeight;
		auto offset = getCableOffset(dropIndex, s, e);

		if(isVertical)		
		{
			auto s1 = s.translated(sh * -0.5f, 0.0f);
			auto s2 = s.translated(sh * 0.5f, 0.0f);

			auto e1 = e.translated(sh * -0.5f, 0.0f);
			auto e2 = e.translated(sh * 0.5f, 0.0f);

			p.startNewSubPath(s1);
			p.lineTo(s2);
			p.lineTo(e2);
			p.lineTo(e1);
			p.closeSubPath();
		}
		else
		{
			auto mx = (e.getX() - s.getX()) * 0.5f;
			auto my = (e.getY() - s.getY());
			

			auto ms = sh * 0.5f;

			if(e.getY() < s.getY())
				ms *= -1.0f;

			p.startNewSubPath(s);

			auto pos = s.translated(mx + ms + offset, 0.0f);
			p.lineTo(pos);
			pos = pos.translated(0.0, my);
			p.lineTo(pos);
			pos = e;
			p.lineTo(pos);
			pos = pos.translated(0.0f, sh);
			p.lineTo(pos);
			pos = pos.translated(-mx - ms + offset, 0.0f);
			p.lineTo(pos);
			pos = pos.translated(0.0f, -my);
			p.lineTo(pos);
			pos = s.translated(0.0f, sh);
			p.lineTo(pos);
			p.closeSubPath();

			p = p.createPathWithRoundedCorners(4.0f);
		}
		
		hitzonePaths.push_back({p, dropIndex});
	};

	if(pinPositions.childPositions.empty())
	{
		addPath(pinPositions.containerStart.translated(10.0f, 0.0f), pinPositions.containerEnd.translated(-10.0f, 0.0f), 0, false);
	}
	else
	{
		addPath(pinPositions.containerStart.translated(10.0f, 0.0f), pinPositions.childPositions[0].first, 0, false);

		for(int i = 0; i < pinPositions.childPositions.size()-1; i++)
			addPath(pinPositions.childPositions[i].second, pinPositions.childPositions[i+1].first, pinPositions.childPositions[i].dropIndex + 1, isForcedVertical);

		addPath(pinPositions.childPositions.back().second, pinPositions.containerEnd.translated(-10.0f, 0.0f), pinPositions.childPositions.back().dropIndex+1, false);
	}

	for(const auto& hz: hitzonePaths)
	{
		parent.addButtons.add(new AddButton(parent, hz.first, hz.second));
	};

	p.repaint();
}

void ContainerComponent::CableSetup::init(const ValueTree& v)
{
	connections.clear();

	numNodes = v.getChildWithName(PropertyIds::Nodes).getNumChildren();
	numChannels = Helpers::getNumChannels(v);

	jassert(Helpers::isContainerNode(v));

	auto n = Helpers::getFactoryPath(v).second;

	if (n == "split")
	{
		type = ContainerType::Split;

		MultiChannelConnection mc;

		for (int i = 0; i < numChannels; i++)
			mc.push_back({ i, i, true });

		for (int i = 0; i < numNodes; i++)
			connections.push_back(mc);
	}
	else if (n == "multi")
	{
		type = ContainerType::Multi;

		auto numPerNode = numNodes == 0 ? numChannels : numChannels / numNodes;

		for (int i = 0; i < numNodes; i++)
		{
			MultiChannelConnection mc;

			for (int c = 0; c < numPerNode; c++)
				mc.push_back({ i * numPerNode + c, c, true });

			connections.push_back(mc);
		}
	}
	else if (n == "branch")
	{
		type = ContainerType::Branch;
	}
	else if (n == "modchain")
	{
		type = ContainerType::ModChain;
		numChannels = 1;

		MultiChannelConnection mc;

		mc.push_back({ 0, 0, true });

		connections.push_back(mc);
	}
	else if (n == "offline")
	{
		type = ContainerType::Offline;
		numChannels = 0;
	}
	else
	{
		type = ContainerType::Serial;

		MultiChannelConnection mc;

		for (int i = 0; i < numChannels; i++)
			mc.push_back({ i, i, true });

		connections.push_back(mc);
	}

	updateVerticalState();
}

bool ContainerComponent::CableSetup::routeThroughChildNodes() const
{
	return !pinPositions.childPositions.empty();
}

void ContainerComponent::CableSetup::setDragPosition(Rectangle<int> currentlyDraggedNodeBounds, Point<int> downPos)
{
	dragPosition = currentlyDraggedNodeBounds;
	dropIndex = -1;

	for (auto ab : parent.addButtons)
	{
		auto lp = ab->getLocalPoint(&parent, downPos).toFloat();
		dropIndex = jmax(dropIndex, ab->onNodeDrag(lp));
	}

	parent.repaint();
}

float ContainerComponent::CableSetup::getCableOffset(int nodeIndex, Point<float> p1, Point<float> p2) const
{
	return getCableOffset(nodeIndex, std::abs(p2.getX() - p1.getX()));
}

float ContainerComponent::CableSetup::getCableOffset(int nodeIndex, float maxWidth) const
{
	ValueTree n;

	if (nodeIndex == 0)
		n = parent.getValueTree().getChildWithName(PropertyIds::Nodes);
	else
		n = parent.getValueTree().getChildWithName(PropertyIds::Nodes).getChild(nodeIndex - 1);

	if (n.isValid())
	{
		auto v = (float)n[UIPropertyIds::CableOffset];
		return jlimit(-maxWidth * 0.4f, maxWidth * 0.4f, v);
	}

	return 0.0f;
}

void ContainerComponent::CableSetup::draw(Graphics& g)
{
	auto colour = Helpers::getNodeColour(parent.getValueTree());

	if (!isSerialType() && pinPositions.childPositions.size() != connections.size())
		return;

	const auto delta = (float)(Helpers::SignalHeight) / (float)numChannels;

	CableDrawData cd;
	cd.lod = LODManager::getLOD(parent);
	cd.numChannels = numChannels;
	cd.c = colour;

	pinPositions.containerStart.setX(cd.lod == 0 ? Helpers::ParameterMargin + Helpers::ContainerParameterWidth - 25.0f : 0);

	auto xOffset = 10.0f;

	for (int c = 0; c < numChannels; c++)
	{
		Rectangle<float> s(pinPositions.containerStart, pinPositions.containerStart);
		Rectangle<float> e(pinPositions.containerEnd, pinPositions.containerEnd);

		auto pinSize = 8.0f;
		s = s.withSizeKeepingCentre(pinSize, pinSize).translated(xOffset, c * delta + delta * 0.5f);
		e = e.withSizeKeepingCentre(pinSize, pinSize).translated(-xOffset, c * delta + delta * 0.5f);

		if (cd.lod == 0)
		{
			g.setColour(Colours::grey);
			PathFactory::scalePath(pin, s);
			g.fillPath(pin);

			if (type != ContainerType::ModChain)
			{
				PathFactory::scalePath(pin, e);
				g.fillPath(pin);
			}
		}



		if (!routeThroughChildNodes())
		{
			cd.setCoordinates(s.getCentre(), e.getCentre(), 0.0f, c);
			cd.draw(g, true);
		}
	}

	if (!routeThroughChildNodes())
		return;

	g.setColour(colour);

	if (isSerialType())
	{
		if (pinPositions.childPositions.size() > 0)
		{
			// draw first

			auto firstPos = pinPositions.childPositions[0];
			float offset = delta * 0.5f;

			for (int c = 0; c < numChannels; c++)
			{
				auto p1 = pinPositions.containerStart.translated(xOffset, 0.0f);
				auto p2 = firstPos.first;
				auto o = getCableOffset(0, p1, p2);

				cd.setCoordinates(p1.translated(0, offset), p2.translated(0, offset), o, c);
				cd.draw(g, false);
				offset += delta;
			}
		}

		for (int i = 0; i < pinPositions.childPositions.size() - 1; i++)
		{
			auto p1 = pinPositions.childPositions[i].second;
			auto p2 = pinPositions.childPositions[i + 1].first;

			if (isForcedVertical)
			{
				float offset = (float)Helpers::SignalHeight * -0.25f;

				for (int c = 0; c < numChannels; c++)
				{
					auto o = getCableOffset(i + 1, p1, p2);

					cd.setCoordinates(p1.translated(offset, 0), p2.translated(offset, 0), o, c);
					cd.draw(g, true);
					offset += delta;
				}
			}
			else
			{
				float offset = delta * 0.5f;

				for (int c = 0; c < numChannels; c++)
				{
					auto o = getCableOffset(i + 1, p1, p2);
					cd.setCoordinates(p1.translated(0, offset), p2.translated(0, offset), o, c);
					cd.draw(g, false);
					offset += delta;
				}
			}
		}

		if (type != ContainerType::ModChain && pinPositions.childPositions.size() > 0)
		{
			// draw last
			auto lastPos = pinPositions.childPositions.back();
			float offset = delta * 0.5f;

			for (int c = 0; c < numChannels; c++)
			{
				auto p1 = lastPos.second;
				auto p2 = pinPositions.containerEnd;

				auto o = getCableOffset(pinPositions.childPositions.size(), p1, p2);
				cd.setCoordinates(p1.translated(0, offset), p2.translated(-xOffset, offset), o, c);
				cd.draw(g, false);
				offset += delta;
			}
		}
	}
	else
	{
		int cIndex = 0;

		for (int i = 0; i < connections.size(); i++)
		{
			std::vector<Connection> con = connections[i];

			for (auto cc : con)
			{
				auto numTargets = con.size();
				auto targetDelta = Helpers::SignalHeight / numTargets;

				float offset = delta * 0.5f + (float)cc.sourceIndex * delta;
				float targetOffset = targetDelta * 0.5f + (float)cc.targetIndex * targetDelta;
				auto p1 = pinPositions.containerStart.translated(xOffset, offset);
				auto p2 = pinPositions.childPositions[i].first.translated(0, targetOffset);
				auto p3 = pinPositions.childPositions[i].second.translated(0, targetOffset);
				auto p4 = pinPositions.containerEnd.translated(-xOffset, offset);

				cd.channelIndex = cIndex;
				cd.cableOffset = 0.0f;
				cd.active = cc.active;

				cd.p1 = p1;
				cd.p2 = p2;

				cd.draw(g, false);

				cd.p1 = p3;
				cd.p2 = p4;

				cd.draw(g, false);
				cIndex++;
			}
		}
	}
}

ContainerComponent::ContainerComponent(Lasso* l, const ValueTree& v, UndoManager* um_) :
	NodeComponent(l, v, um_, true),
	cables(*this, v),
	resizer(this),
	lockButton("lock", this, *this)
{
	addAndMakeVisible(resizer);
	addAndMakeVisible(lockButton);

	lockButton.setVisible(!Helpers::isRootNode(v));

	lockButton.setToggleModeWithColourChange(true);

	lockButton.setToggleStateAndUpdateIcon((bool)v[UIPropertyIds::LockPosition]);

	lockButton.onClick = [this]()
	{
		toggle(UIPropertyIds::LockPosition);
	};

	setInterceptsMouseClicks(false, true);
	nodeListener.setCallback(data.getChildWithName(PropertyIds::Nodes),
		Helpers::UIMode,
		VT_BIND_CHILD_LISTENER(onChildAddRemove));

	resizeListener.setCallback(data,
		{ UIPropertyIds::width, UIPropertyIds::height, PropertyIds::IsVertical },
		Helpers::UIMode,
		VT_BIND_PROPERTY_LISTENER(onResize));

	verticalListener.setCallback(data,
		{ PropertyIds::Value }, Helpers::UIMode, VT_BIND_RECURSIVE_PROPERTY_LISTENER(onIsVertical));

	lockListener.setCallback(data.getChildWithName(PropertyIds::Nodes), { PropertyIds::Locked }, Helpers::UIMode, VT_BIND_RECURSIVE_PROPERTY_LISTENER(onChildLock));

	resizer.setAlwaysOnTop(true);

	childPositionListener.setCallback(v.getChildWithName(PropertyIds::Nodes),
		UIPropertyIds::Helpers::getPositionIds(),
		Helpers::UIMode,
		VT_BIND_RECURSIVE_PROPERTY_LISTENER(onChildPositionUpdate));

	auto commentTree = getValueTree().getOrCreateChildWithName(UIPropertyIds::Comments, um);

	freeCommentListener.setCallback(commentTree, Helpers::UIMode, VT_BIND_CHILD_LISTENER(onFreeComment));

	channelListener.setCallback(v, { PropertyIds::CompileChannelAmount }, Helpers::UIMode, VT_BIND_PROPERTY_LISTENER(onChannel));

	parameterListener.setCallback(v.getChildWithName(PropertyIds::Parameters), Helpers::UIMode, VT_BIND_CHILD_LISTENER(onParameterAddRemove));

	auto gt = getValueTree().getOrCreateChildWithName(UIPropertyIds::Groups, um);
	groupListener.setCallback(gt, Helpers::UIMode, VT_BIND_CHILD_LISTENER(onGroup));

	nodeListener.handleUpdateNowIfNeeded();
	resizeListener.handleUpdateNowIfNeeded();
	groupListener.handleUpdateNowIfNeeded();

	if (!Helpers::hasDefinedBounds(v))
	{
		Rectangle<int> nb(getPosition(), getPosition().translated(500, 100));

		Helpers::updateBounds(getValueTree(), nb, um);
		setSize(500, 100);
		resized();
		cables.updatePins(*this);
	}

	bypassListener.setCallback(getValueTree(), { PropertyIds::Bypassed }, Helpers::UIMode, VT_BIND_RECURSIVE_PROPERTY_LISTENER(onBypass));

	commentListener.setCallback(getValueTree(), { PropertyIds::Comment }, Helpers::UIMode, VT_BIND_RECURSIVE_PROPERTY_LISTENER(onCommentChange));

	breakoutListener.setCallback(getValueTree().getChildWithName(PropertyIds::Parameters), { UIPropertyIds::x, UIPropertyIds::y }, Helpers::UIMode, VT_BIND_RECURSIVE_PROPERTY_LISTENER(onParameterPosition));

	rebuildDescription();
}



void ContainerComponent::onGroup(const ValueTree& v, bool wasAdded)
{
	if (wasAdded)
	{
		groups.add(new Group(*this, v));
	}
	else
	{
		for (auto g : groups)
		{
			if (*g == v)
			{
				groups.removeObject(g);
				break;
			}
		}
	}

	repaint();
}

void ContainerComponent::onFreeComment(const ValueTree& v, bool wasAdded)
{
	if (wasAdded)
	{
		comments.add(new Comment(*this, v));
	}
	else
	{
		for (auto c : comments)
		{
			if (c->data == v)
			{
				comments.removeObject(c);
				break;
			}
		}
	}
}

void ContainerComponent::onChannel(const Identifier&, const var& newValue)
{
	cables.init(getValueTree());
	cables.updatePins(*this);
}

void ContainerComponent::onChildLock(const ValueTree& v, const Identifier& id)
{
	if (Helpers::isImmediateChildNode(v, getValueTree()))
	{
		onChildAddRemove(v, false);
		onChildAddRemove(v, true);
	}
}

void ContainerComponent::onCommentChange(const ValueTree& v, const Identifier& id)
{
	if (Helpers::isImmediateChildNode(v, getValueTree()))
	{
		auto t = v[id].toString();

		auto add = t.isNotEmpty();

		if (add)
		{
			for (auto& c : comments)
			{
				if (c->data == v)
					return;
			}

			for (auto cn : childNodes)
			{
				if (cn->getValueTree() == v)
				{
					comments.add(new Comment(*this, cn));
					Helpers::fixOverlap(getValueTree(), um, false);
					comments.getLast()->showEditor();

					return;
				}
			}
		}
		else
		{
			if (v.getType() == PropertyIds::Comment)
			{
				v.getParent().removeChild(v, um);
				return;
			}

			for (auto c : comments)
			{
				if (c->data == v)
				{
					comments.removeObject(c);
					Helpers::fixOverlap(getValueTree(), um, false);
					return;
				}
			}
		}
	}
}

void ContainerComponent::onBypass(const ValueTree& v, const Identifier&)
{
	if (Helpers::isImmediateChildNode(v, getValueTree()))
	{
		cables.updatePins(*this);
	}
	if (v == getValueTree())
	{
		rebuildDescription();
		repaint();
	}
}

void ContainerComponent::onFold(const Identifier& id, const var& newValue)
{
	NodeComponent::onFold(id, newValue);

	auto folded = (bool)newValue && !Helpers::isRootNode(getValueTree());

	for (auto c : childNodes)
		c->setVisible(!folded);

	cables.updatePins(*this);
}

void ContainerComponent::onChildPositionUpdate(const ValueTree& v, const Identifier& id)
{
	if (v.getParent() == getValueTree().getChildWithName(PropertyIds::Nodes))
	{
		if (Helpers::isProcessNode(v) || Helpers::isContainerNode(v))
		{
			// only update cables when the nodes is not being dragged around...
			for (auto cn : childNodes)
			{
				if (cn->getValueTree() == v)
				{
					if (cn->getParentComponent() == this)
					{
						SafeAsyncCall::call<ContainerComponent>(*this, [](ContainerComponent& c)
							{
								c.cables.updatePins(c);
							});
					}

					break;
				}
			}
		}
	}
}

void ContainerComponent::onChildAddRemove(const ValueTree& v, bool wasAdded)
{
	if (wasAdded)
	{
		NodeComponent* nc;

		if (Helpers::isContainerNode(v))
		{
			if(v[PropertyIds::Locked])
				nc = new LockedContainerComponent(lasso, v, um);
			else
				nc = new ContainerComponent(lasso, v, um);
		}
		else if (Helpers::isProcessNode(v))
			nc = new ProcessNodeComponent(lasso, v, um);
		else
			nc = new NoProcessNodeComponent(lasso, v, um);

		addAndMakeVisible(nc);

		struct Sorter
		{
			static int compareElements(NodeComponent* n1, NodeComponent* n2)
			{
				auto v1 = n1->getValueTree();
				auto v2 = n2->getValueTree();

				auto idx1 = v1.getParent().indexOf(v1);
				auto idx2 = v2.getParent().indexOf(v2);

				if (idx1 < idx2)
					return -1;
				if (idx1 > idx2)
					return 1;

				return 0;
			}
		} sorter;

		childNodes.addSorted(sorter, nc);

		if(v[PropertyIds::Comment].toString().isNotEmpty())
		{
			comments.add(new Comment(*this, nc));
		}
	}
	else
	{
		for (auto c : childNodes)
		{
			if (c->getValueTree() == v)
			{
				childNodes.removeObject(c);
				break;
			}
		}

		for(auto c: comments)
		{
			if(c->data == v)
			{
				comments.removeObject(c);
				break;
			}
		}
	}

	if (auto d = findParentComponentOfClass<CableComponent::CableHolder>())
		d->rebuildCables();

	cables.updatePins(*this);
}

void ContainerComponent::onResize(const Identifier& id, const var& newValue)
{
	if (id == UIPropertyIds::width)
		setSize((int)newValue, getHeight());
	else if (id == UIPropertyIds::height)
		setSize(getWidth(), (int)newValue);
}

void ContainerComponent::onIsVertical(const ValueTree& v, const Identifier& id)
{
	if (v[PropertyIds::ID] == PropertyIds::IsVertical.toString())
	{
		Helpers::resetLayout(getValueTree(), um);

		MessageManager::callAsync([this]()
			{
				cables.updateVerticalState();
				cables.updatePins(*this);
			});
	}
}

void ContainerComponent::onParameterPosition(const ValueTree& v, const Identifier& id)
{
	auto pos = Helpers::getPosition(v);
	auto shouldDelete = pos.isOrigin();
	

	for(auto b: breakoutParameters)
	{
		if(b->data == v)
		{
			if(shouldDelete)
			{
				for(auto p: parameters)
				{
					if(p->data == v)
					{
						p->setEnableDragging(true);
						break;
					}
				}

				breakoutParameters.removeObject(b);

				if (auto f = findParentComponentOfClass<CableComponent::CableHolder>())
					f->rebuildCables();
			}
			else
			{
				b->setTopLeftPosition(pos);
			}
				
			return;
		}
	}

	if(!shouldDelete)
	{
		for (auto p : parameters)
		{
			if (p->data == v)
			{
				p->setEnableDragging(false);
				break;
			}
		}

		auto nb = new BreakoutParameter(*this, v, um);
		addAndMakeVisible(nb);
		nb->setTopLeftPosition(pos);
		breakoutParameters.add(nb);

		if(auto f = findParentComponentOfClass<CableComponent::CableHolder>())
			f->rebuildCables();
	}
}

void ContainerComponent::rebuildDescription()
{
	description.clear();
	auto desc = Helpers::getSignalDescription(getValueTree());
	description.setJustification(Justification::centredLeft);
	description.append(desc, GLOBAL_FONT(), Helpers::getNodeColour(getValueTree()));
}

void ContainerComponent::expandParentsRecursive(Component& componentToShow, Rectangle<int> firstBoundsMightBeUnion, bool addMarginToFirst)
{
	Component* c = &componentToShow;
	auto pc = componentToShow.findParentComponentOfClass<ContainerComponent>();

	while (pc != nullptr)
	{
		auto parentBounds = pc->getLocalBounds();
		auto boundsInParent = c->getBoundsInParent();

		auto first = c == &componentToShow;

		if (first && !firstBoundsMightBeUnion.isEmpty())
			boundsInParent = firstBoundsMightBeUnion;

		auto w = boundsInParent.getRight();
		auto h = boundsInParent.getBottom();

		if (addMarginToFirst || !first)
		{
			w += Helpers::NodeMargin;
			h += Helpers::NodeMargin;
		}

		w = jmax<int>(parentBounds.getWidth(), w);
		h = jmax<int>(parentBounds.getHeight(), h);

		pc->getValueTree().setProperty(UIPropertyIds::width, w, pc->um);
		pc->getValueTree().setProperty(UIPropertyIds::height, h, pc->um);

		c = pc;
		pc = pc->findParentComponentOfClass<ContainerComponent>();
	}
}



void ContainerComponent::paint(Graphics& g)
{
	auto nodeColour = Helpers::getNodeColour(data);

	g.setColour(Colour(0x20000000));
	g.fillRect(getLocalBounds().removeFromLeft(Helpers::ContainerParameterWidth));

	g.drawVerticalLine(Helpers::ContainerParameterWidth-1, 0.0f, (float)getHeight());

	if (Helpers::isFoldedOrLockedContainer(getValueTree()) || !header.downPos.isOrigin())
	{
		g.fillAll(Colour(0xFF353535));
	}
	else
	{
		g.fillAll(Colour(0xFF222222).withAlpha(0.03f));
	}

	{
		auto b = getLocalBounds().toFloat();

		g.setColour(Colours::black.withAlpha(0.1f));

		b.removeFromTop(Helpers::HeaderHeight);

		b = b.removeFromTop(5.0f);

		for (int i = 0; i < 3; i++)
		{
			g.fillRect(b);
			b.removeFromBottom(1.0f);
		}
	}

	auto lod = LODManager::getLOD(*this);

	g.setColour(nodeColour);
	g.drawRect(getLocalBounds(), (int)LayoutTools::getCableThickness(lod));

	if(lod != 0)
	{
		g.setColour(nodeColour.withAlpha(0.1f));
		g.drawRect(getLocalBounds(), 20);
	}
	
	if(getValueTree()[PropertyIds::Folded])
		return;

	auto tb = getLocalBounds();
	tb.removeFromTop(Helpers::HeaderHeight);
	tb = tb.removeFromTop(Helpers::SignalHeight).removeFromLeft(Helpers::ParameterMargin + Helpers::ContainerParameterWidth - 25).reduced(0, 2);

	if(lod == 0)
	{
		g.setColour(nodeColour);
		tb.removeFromLeft(Helpers::ParameterMargin);
		description.draw(g, tb.toFloat());
	}
	
	auto root = findParentComponentOfClass<DspNetworkComponent>();

	if (root->currentlyHoveredContainer != nullptr)
	{
		if(root->currentlyHoveredContainer == this)
		{
			if (cables.dropIndex == -1)
			{
				g.fillAll(Colour(SIGNAL_COLOUR).withAlpha(0.05f));
			}
		}
		else
		{
			g.fillAll(Colour(0xFF222222));
		}
	}

	if(lod == 0)
		drawOutsideLabel(g);

	
	g.setColour(Helpers::getNodeColour(data));
	cables.draw(g);

	for(auto bn: breakoutParameters)
	{
		if(bn->selected)
		{
			auto tb = bn->getBoundsInParent().expanded(3.0f).toFloat();
			g.setColour(Colour(SIGNAL_COLOUR).withAlpha(0.8f));
			g.drawRoundedRectangle(tb, 3.0f, 1.0f);
		}
	}

	for(auto cn: childNodes)
	{
		if(cn->getParentComponent() != this)
			continue;

		if(cn->selected)
		{
			auto tb = cn->getBoundsInParent().expanded(3.0f).toFloat();
			g.setColour(Colour(SIGNAL_COLOUR).withAlpha(0.8f));
			g.drawRoundedRectangle(tb, 3.0f, 1.0f);
		}

		if(lod == 0)
		{
			auto b = cn->getBoundsInParent().toFloat().expanded(1.0f);
			g.setColour(Colours::black.withAlpha(0.2f));

			std::array<float, 5> alphas = { 0.25f, 0.15f, 0.1f, 0.05f, 0.02f };

			auto isContainer = Helpers::isContainerNode(cn->getValueTree());

			for (int i = 0; i < 2; i++)
			{
				g.setColour(Colours::black.withAlpha(alphas[i]));

				if(isContainer)
					g.drawRect(b, 2.0f);
				else
					g.fillRect(b);

				b = b.translated(0.0f, 1.0f).expanded(0.5f);
			}
		}
	}

	for(auto gr: groups)
		gr->draw(g);
}

void ContainerComponent::resized()
{
	NodeComponent::resized();

	auto showBottomTools = !Helpers::isFoldedOrLockedContainer(getValueTree());

	lockButton.setVisible(showBottomTools);
	resizer.setVisible(showBottomTools);

	auto b = getLocalBounds();
	auto bar = b.removeFromBottom(15);
	resizer.setBounds(bar.removeFromRight(15));
	cables.updatePins(*this);
	lockButton.setBounds(bar.removeFromRight(15));

	auto yOffset = jmax(Helpers::HeaderHeight + Helpers::SignalHeight, 2 * Helpers::ParameterMargin);

	for (auto p : parameters)
	{
		yOffset += Helpers::ParameterMargin;
		p->setTopLeftPosition(p->getX(), yOffset);
		yOffset += p->getHeight();
	}

	positionOutsideParameters(yOffset);
}

void ContainerComponent::renameGroup(Point<float> position)
{
	for (auto g : groups)
	{
		if (g->lastBounds.contains(position))
		{
			auto copy = g->lastBounds;

			addAndMakeVisible(groupEditor = new TextEditor());
			groupEditor->setBounds(copy.removeFromTop(20).toNearestInt());

			GlobalHiseLookAndFeel::setTextEditorColours(*groupEditor);
			groupEditor->getTextValue().referTo(g->v.getPropertyAsValue(PropertyIds::ID, um, false));
			groupEditor->grabKeyboardFocusAsync();

			groupEditor->onFocusLost = [this]()
			{
				groupEditor = nullptr;
			};
		}
	}
}

bool ContainerComponent::selectGroup(Point<float> position)
{
	auto ok = false;

	for (auto g : groups)
	{
		if (g->lastBounds.contains(position))
		{
			auto& selection = findParentComponentOfClass<Lasso>()->getLassoSelection();

			for (auto cn : childNodes)
			{
				if (g->idList.contains(cn->getValueTree()[PropertyIds::ID].toString()))
				{
					selection.addToSelection(cn);
					ok = true;
				}
			}
		}
	}

	return ok;
}

scriptnode::ParameterComponent* ContainerComponent::getDraggableParameterComponent(int index)
{
	if (auto p = getParameter(index))
	{
		if (p->isDraggable())
			return p;
	}

	for (auto b : breakoutParameters)
	{
		if (b->index == index)
			return &b->parameter;
	}

	return nullptr;
}







void ContainerComponent::CableSetup::CableDrawData::draw(Graphics& g, bool preferLines)
{
	Path p;

	if (preferLines)
	{
		p.startNewSubPath(p1);
		p.lineTo(p2);
	}
	else
	{
		float offset = 0.0f;

		auto sShape = p2.getX() < p1.getX();

		if (numChannels > 1)
		{
			if (sShape)
			{
				auto fullOffset = -6.0f;

				auto sOffset = (1.0f - (float)channelIndex / (float)numChannels) * fullOffset;

				p1 = p1.translated(sOffset, 0.0f);
				p2 = p2.translated(-sOffset, 0.0f);
			}
			else
			{
				auto maxWidth = jmin((float)Helpers::SignalHeight * 0.25f, 0.8f * std::abs((p2.getX() - p1.getX())));

				float normalisedChannelIndex = (float)channelIndex / (float)(numChannels - 1);
				offset = maxWidth * (-0.5f + normalisedChannelIndex);

				if (p2.getY() > p1.getY())
					offset *= -1.0f;
			}
		}

		offset += cableOffset;

		p.startNewSubPath(p1);

		Helpers::createCustomizableCurve(p, p1, p2, offset, 5.0f);
	}


	PathStrokeType::EndCapStyle sp = PathStrokeType::EndCapStyle::butt;

	auto strokeDepth = LayoutTools::getCableThickness(lod);

	if (lod == 0 && active)
	{

		g.setColour(Colours::black.withAlpha(0.7f));
		g.fillEllipse(Rectangle<float>(p1, p1).withSizeKeepingCentre(4.0f, 4.0f));
		g.fillEllipse(Rectangle<float>(p2, p2).withSizeKeepingCentre(4.0f, 4.0f));
		g.strokePath(p, PathStrokeType(3.0f));
		sp = PathStrokeType::rounded;
	}

	g.setColour(active ? c : c.withAlpha(0.2f));
	g.strokePath(p, PathStrokeType(strokeDepth, PathStrokeType::beveled, sp));
}

void ContainerComponent::Group::draw(Graphics& g)
{
	RectangleList<int> bounds;

	LODManager::LODGraphics lg(g, parent);

	auto nodeTree = parent.getValueTree().getChildWithName(PropertyIds::Nodes);

	Colour c;

	for (auto n : nodeTree)
	{
		if (idList.contains(n[PropertyIds::ID].toString()))
		{
			c = Helpers::getNodeColour(n);
			bounds.addWithoutMerging(Helpers::getBounds(n, true));
		}
	}

	auto b = bounds.getBounds().expanded(10).toFloat();

	auto h = 20.0f + lg.getCurrentLOD() * 10;

	b = b.withTop(b.getY() - h);

	g.setColour(c.withAlpha(0.05f));
	lg.fillRoundedRectangle(b, 5.0f);
	g.setColour(c.withAlpha(0.6f));
	lg.drawRoundedRectangle(b, 5.0f, -1.0f);

	lastBounds = b;

	auto f = GLOBAL_BOLD_FONT().withHeight(h - 5.0f);

	name = v[PropertyIds::ID].toString();

	g.setColour(c);
	g.setFont(f);
	g.drawText(name, b.removeFromTop(h), Justification::centred);

	
}

void ContainerComponent::Comment::showEditor()
{
	NetworkParent::PopupCodeEditor::show(this, data, PropertyIds::Comment, parent.um);
}

void ContainerComponent::AddButton::mouseMove(const MouseEvent& e)
{
	auto isActive = true;

	if (isActive != active)
	{
		active = isActive;
		repaint();
	}
}

void ContainerComponent::AddButton::mouseUp(const MouseEvent& e)
{
	if(SelectableComponent::Lasso::checkLassoEvent(e, SelectableComponent::LassoState::Up))
		return;

	if(e.mouseWasDraggedSinceMouseDown())
	{
		parent.cables.updatePins(parent);
		return;
	}

	if (active)
	{
		BuildHelpers::CreateData cd;
		cd.pointInContainer = e.getEventRelativeTo(&parent).getPosition();
		cd.containerToInsert = parent.getValueTree();
		cd.source = nullptr;
		cd.signalIndex = index;

		auto root = findParentComponentOfClass<DspNetworkComponent>();
		
		auto globalPos = root->getLocalPoint(&parent, cd.pointInContainer);

		auto copy = hitzone;

		auto delta = root->getLocalPoint(this, hitzone.getBounds().getTopLeft().toInt());

		copy.applyTransform(AffineTransform::translation(delta));
		
		root->showCreateNodePopup(globalPos, this, copy, cd);
	}
}

ContainerComponent::BreakoutParameter::BreakoutParameter(ContainerComponent& realParent_, const ValueTree& data_, UndoManager* um_) :
	SelectableComponent(realParent_.lasso),
	realParent(realParent_),
	parameter(realParent_.lasso->getUpdater(), data_, um_),
	data(data_),
	um(um_),
	index(data_.getParent().indexOf(data_)),
	gotoButton("goto", nullptr, *this),
	closeButton("close", nullptr, *this)
{
	addAndMakeVisible(parameter);
	addAndMakeVisible(gotoButton);
	addAndMakeVisible(closeButton);

	closeButton.onClick = [this]
	{
		data.removeProperty(UIPropertyIds::x, um);
		data.removeProperty(UIPropertyIds::y, um);
	};

	gotoButton.setClickingTogglesState(false);

	gotoButton.onClick = [this]()
	{
		auto c = findParentComponentOfClass<ContainerComponent>();

		for (int i = 0; i < c->getNumParameters(); i++)
		{
			auto p = c->getParameter(i);
			if (p->data == data)
			{
				auto lb = findParentComponentOfClass<DspNetworkComponent>()->getLocalArea(p, p->getLocalBounds());
				NetworkParent::scrollToRectangle(this, lb, true);
				break;
			}
		}
	};

	setSize(Helpers::ContainerParameterWidth + 2 * Helpers::ParameterMargin, Helpers::HeaderHeight + Helpers::ParameterHeight + 2 * Helpers::ParameterMargin);
	parameter.setEnableDragging(true);
}

void ContainerComponent::BreakoutParameter::mouseDrag(const MouseEvent& e)
{
	dragger.dragComponent(this, e, nullptr);
	findParentComponentOfClass<CableComponent::CableHolder>()->rebuildCables();


}

void ContainerComponent::BreakoutParameter::mouseUp(const MouseEvent& e)
{
	auto pos = getPosition();
	

	data.setProperty(UIPropertyIds::x, jmax(0, pos.getX()), um);
	data.setProperty(UIPropertyIds::y, jmax(0, pos.getY()), um);
}

}
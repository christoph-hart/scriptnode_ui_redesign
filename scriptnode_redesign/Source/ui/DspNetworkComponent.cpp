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



DraggedCable::~DraggedCable()
{
	if (hoveredPin == nullptr && ok)
	{
		BuildHelpers::CreateData cd;
		
		auto root = src->findParentComponentOfClass<DspNetworkComponent>();
		auto pos = root->getLocalPoint(this, e.toInt());

		auto container = root->rootComponent->getInnerContainer(root, pos, nullptr);

		cd.containerToInsert = container->getValueTree();
		cd.pointInContainer = container->getLocalPoint(root, pos);
		cd.source = src;

		auto delta = getPosition();

		
		p.applyTransform(AffineTransform::translation(delta));

		root->showCreateNodePopup(pos, p, cd);
	}
}

void DraggedCable::setTargetPosition(Point<int> rootPosition)
{
	ok = true;
	auto parent = getParentComponent();

	auto start = parent->getLocalArea(src, src->getLocalBounds()).toFloat();
	auto ns = Point<float>(start.getRight(), start.getCentreY());

	auto root = findParentComponentOfClass<DspNetworkComponent>();

	CablePinBase* currentPin = nullptr;
	
	Rectangle<int> pinArea;

	callRecursive<CablePinBase>(root, [&](CablePinBase* pin)
	{
		auto area = root->getLocalArea(pin, pin->getLocalBounds());

		if(area.contains(rootPosition))
		{
			currentPin = pin;
			pinArea = area;
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
		if(currentPin->matchesConnectionType(src->getConnectionType()))
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
	else
	{
		label = "Create new parameter node";
	}

	rebuildPath(ns, ne.toFloat(), parent);
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
			for(int i = 0; i < cc->getNumOutsideParameters(); i++)
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
			auto p = c->getParameter(i);
			auto con = p->rebuildConnections(asComponent);

			if (!con.empty())
				allConnections[p] = std::move(con);
		}

		return false;
	});

	

	std::map<CablePinBase*, CablePinBase::Connections> localConnections;

	stubs.clear();

	for(auto& c: allConnections)
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

			if(!forceCable && (shouldHide))
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
			if(src->isFoldedAway() || dst->isFoldedAway())
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

			if(n.getType() != PropertyIds::Node)
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

void DspNetworkComponent::setIsDragged(NodeComponent* nc)
{
	DraggedNode dn;
	dn.originalParent = dynamic_cast<ContainerComponent*>(nc->getParentComponent());
	dn.node = nc;

	dn.removeFromParent(*this);

#if 0
	auto currentBounds = nc->getBoundsInParent();

	currentlyDraggedComponents.add(nc);
	currentParent->removeChildComponent(nc);
	addChildComponent(nc);

	currentParent->cables.updatePins(*currentParent);

	nc->setAlpha(0.5f);

	auto rootBounds = getLocalArea(currentParent, currentBounds);
	nc->setBounds(rootBounds);
#endif
}

void DspNetworkComponent::clearDraggedComponents()
{
	for(auto& c: currentlyDraggedComponents)
		c.addToParent(*this);

	currentlyDraggedComponents.clear();

	if(currentlyHoveredContainer != nullptr)
		currentlyHoveredContainer->repaint();

	currentlyHoveredContainer = nullptr;
}

void DspNetworkComponent::resetDraggedBounds()
{
	if (currentlyHoveredContainer != nullptr)
	{
		for (auto nc : currentlyDraggedComponents)
		{
			auto containerBounds = currentlyHoveredContainer->getLocalArea(this, nc.node->getBoundsInParent());
			Helpers::updateBounds(nc.node->getValueTree(), containerBounds, &um);
		}
	}
}

void DspNetworkComponent::showCreateConnectionPopup(Point<int> pos)
{
	currentCreateNodePopup = nullptr;

	if(currentCreateConnectionPopup == nullptr)
	{
		addAndMakeVisible(currentCreateConnectionPopup = new CreateConnectionPopup(*this));
		currentCreateConnectionPopup->setCentrePosition(pos);
	}
	else
	{
		currentCreateConnectionPopup = nullptr;
	}

	

}

void DspNetworkComponent::CreateNodePopup::textEditorReturnKeyPressed(TextEditor& te)
{
	timerCallback();

	TextEditorWithAutocompleteComponent::textEditorReturnKeyPressed(te);
	auto text = editor.getText();

	if(currentFactory.isNotEmpty())
		text = currentFactory + text;


	Array<ValueTree> newNodes;

	auto v = BuildHelpers::createNode(root->db, text, cd, &root->um);
	
	if(v.isValid())
	{
		newNodes.add(v);

		auto r = root;

		MessageManager::callAsync([newNodes, r]()
		{
			r->setSelection(newNodes);
			r->grabKeyboardFocusAsync();
		});

		dismiss();
	}
}

}
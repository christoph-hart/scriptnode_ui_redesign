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



void DraggedCable::setTargetPosition(Point<int> rootPosition)
{
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
			colour1 = Colour(SIGNAL_COLOUR);
			hoveredPin = currentPin;
			over = true;
		}
			
		else
			colour1 = Colour(HISE_ERROR_COLOUR);
		
		colour2 = colour1;

		ne = Point<int>(pinArea.getX(), pinArea.getCentreY());
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

	auto asComponent = dynamic_cast<Component*>(this);

	Component::callRecursive<ContainerComponent>(asComponent, [&](ContainerComponent* cc)
	{
		cc->rebuildOutsideParameters();
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

	for(auto& c: allConnections)
	{
		if(c.first->data.getType() != PropertyIds::Parameter)
			continue;

		for (auto it = c.second.begin(); it != c.second.end();)
		{
			CablePinBase* dst = *it;

			auto targetContainer = dst->findParentComponentOfClass<ContainerComponent>();

			if(targetContainer != nullptr)
			{
				if (auto localSource = targetContainer->getOutsideParameter(c.first->data))
				{
					localConnections[localSource].push_back(dst);
					it = c.second.erase(it);
				}
				else
					++it;
			}
			else
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
			

			

			
			
			auto asLasso = dynamic_cast<SelectableComponent::Lasso*>(this);

			auto nc = new CableComponent(*asLasso, src, dst);

			cables.add(nc);
			asComponent->addChildComponent(nc);

			auto sourceVisible = !Helpers::isFoldedRecursive(src->getNodeTree());
			auto targetVisible = !Helpers::isFoldedRecursive(dst->getNodeTree());

			nc->setVisible(sourceVisible && targetVisible);

			nc->updatePosition({}, {});
			
			nc->colour1 = Helpers::getParameterColour(src->data);
			nc->colour2 = Helpers::getNodeColour(dst->data.getParent().getParent());

			nc->toBack();
		}
	}

	for (auto c : cables)
	{
		asComponent->addChildComponent(labels.add(new CableLabel(c)));
		labels.getLast()->updatePosition();
	}
}

}
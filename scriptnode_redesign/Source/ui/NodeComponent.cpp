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



void NodeComponent::HeaderComponent::mouseDown(const MouseEvent& e)
{
	CHECK_MIDDLE_MOUSE_DOWN(e);

	if(Helpers::isRootNode(parent.getValueTree()))
		return;

	downPos = e.getPosition();
	parent.toFront(false);

	auto ls = findParentComponentOfClass<SelectableComponent::Lasso>();

	auto& selection = ls->getLassoSelection();

	if (!selection.isSelected(&parent))
		selection.addToSelectionBasedOnModifiers(&parent, e.mods);

	for (auto s : ls->getLassoSelection().getItemArray())
	{
		if (s != nullptr && s->isNode())
			selectionPositions[s] = dynamic_cast<Component*>(s.get())->getBoundsInParent();
	}

	dragger.startDraggingComponent(&parent, e);

	originalParent = parent.getParentComponent();
	originalBounds = parent.getBoundsInParent();
}

void NodeComponent::HeaderComponent::mouseDrag(const MouseEvent& e)
{
	CHECK_MIDDLE_MOUSE_DRAG(e);

	if (Helpers::isRootNode(parent.getValueTree()))
		return;

	dragger.dragComponent(&parent, e, this);

	auto thisBounds = parent.getBoundsInParent();

	auto deltaX = thisBounds.getX() - originalBounds.getX();
	auto deltaY = thisBounds.getY() - originalBounds.getY();

	auto ls = findParentComponentOfClass<LassoSource<NodeComponent*>>();

	Rectangle<int> selectionBounds = parent.getBoundsInParent();

	for(auto s: selectionPositions)
	{
		auto nb = s.second.translated(deltaX, deltaY);
		Helpers::updateBounds(s.first->getValueTree(), nb, parent.um);
		selectionBounds = selectionBounds.getUnion(nb);
	}

	Helpers::updateBounds(parent.getValueTree(), parent.getBoundsInParent(), parent.um);

	ContainerComponent::expandParentsRecursive(parent, selectionBounds, true);

	parent.findParentComponentOfClass<Component>()->repaint();

	auto root = findParentComponentOfClass<DspNetworkComponent>();

	if (DspNetworkComponent::isEditModeEnabled(e))
	{
		if (!swapDrag)
		{
			swapDrag = true;
			
			root->clearDraggedComponents();

			for(auto s: root->getLassoSelection().getItemArray())
			{
				if(auto nc = dynamic_cast<NodeComponent*>(s.get()))
				{
					root->setIsDragged(nc);
				}
			}

			callRecursive<ContainerComponent>(root, [](ContainerComponent* c)
			{
				c->repaint();
				return false;
			});
		}
		else
		{
			if (root->currentlyHoveredContainer != nullptr)
			{
				root->currentlyHoveredContainer->cables.setDragPosition({}, {});
			}

			auto rootContainer = dynamic_cast<ContainerComponent*>(root->rootComponent.get());
			root->currentlyHoveredContainer = rootContainer->getInnerContainer(root, parent.getBoundsInParent().getCentre(), &parent);

			if (parent.hasSignal && root->currentlyHoveredContainer != nullptr)
			{
				auto c = root->currentlyHoveredContainer.getComponent();
				auto dragPos = c->getLocalBounds().getIntersection(c->getLocalArea(root, parent.getBoundsInParent()));
				
				c->cables.setDragPosition(dragPos, c->getLocalPoint(this, downPos));
			}

			if (root->currentlyHoveredContainer != nullptr)
				root->currentlyHoveredContainer->repaint();
		}
	}
	else
	{
		if(swapDrag)
		{
			root->currentlyHoveredContainer = nullptr;
			swapDrag = false;
			callRecursive<ContainerComponent>(root, [](ContainerComponent* c){ c->cables.setDragPosition({}, {}); return false; });
		}
	}
}

void NodeComponent::HeaderComponent::mouseUp(const MouseEvent& e)
{
	downPos = {};

	CHECK_MIDDLE_MOUSE_UP(e);

	if (Helpers::isRootNode(parent.getValueTree()))
		return;

	auto root = findParentComponentOfClass<DspNetworkComponent>();

	selectionPositions.clear();

	if (swapDrag)
	{
		swapDrag = false;

		root->resetDraggedBounds();

		

		if (root->currentlyHoveredContainer != nullptr)
		{
			auto dropIndex = root->currentlyHoveredContainer->cables.dropIndex;

			if (dropIndex != -1)
			{
				//auto newBounds = root->currentlyHoveredContainer->cables.dragPosition;
				//Helpers::updateBounds(parent.getValueTree(), newBounds, parent.um);
				
				auto newParent = root->currentlyHoveredContainer->getValueTree().getChildWithName(PropertyIds::Nodes);
				auto um = parent.um;

				root->currentlyHoveredContainer->repaint();
				root->repaint();

				root->currentlyHoveredContainer->cables.dragPosition = {};
				root->currentlyHoveredContainer = nullptr;

				root->clearDraggedComponents();

				std::vector<ValueTree> nodesToMove;

				for(auto l: root->getLassoSelection().getItemArray())
				{
					if(l != nullptr)
					{
						auto vt = l->getValueTree();
						if (vt.getType() == PropertyIds::Node)
							nodesToMove.push_back(l->getValueTree());
					}
				}
				
				for(const auto& n: nodesToMove)
				{
					n.getParent().removeChild(n, um);
					newParent.addChild(n, dropIndex++, um);
				}

				Helpers::fixOverlap(root->data.getChildWithName(PropertyIds::Node), um, false);

				callRecursive<ContainerComponent>(root, [](ContainerComponent* c){ c->cables.setDragPosition({}, {}); return false; });
				root->rebuildCables();
				return;
			}
		}

		root->clearDraggedComponents();

		root->removeChildComponent(&parent);
		originalParent->addChildComponent(&parent);
		parent.setBounds(originalBounds);

		Helpers::updateBounds(parent.getValueTree(), parent.getBoundsInParent(), parent.um);
		Helpers::fixOverlap(root->data.getChildWithName(PropertyIds::Node), &root->um, false);
		
	}
	else
	{
		auto bounds = parent.getBoundsInParent();
		Helpers::updateBounds(parent.getValueTree(), bounds, parent.um);
		Helpers::fixOverlap(root->data.getChildWithName(PropertyIds::Node), &root->um, false);
	}

	callRecursive<ContainerComponent>(root, [](ContainerComponent* c){ c->cables.setDragPosition({}, {}); return false; });
	root->rebuildCables();
}


void SelectableComponent::Lasso::groupSelection()
{
	if (selection.getNumSelected() == 0)
		return;

	auto sc = dynamic_cast<Component*>(selection.getSelectedItem(0).get());

	auto container = sc->findParentComponentOfClass<ContainerComponent>();

	auto groupTree = container->getValueTree().getOrCreateChildWithName(UIPropertyIds::Groups, &um);

	StringArray sa;

	for (auto n : selection.getItemArray())
	{
		if (n != nullptr)
		{
			auto id = n->getValueTree()[PropertyIds::ID].toString();

			if (id.isNotEmpty())
				sa.add(id);
				
		}
	}

	sa.sort(false);

	auto s = sa.joinIntoString(";");

	auto existingGroup = groupTree.getChildWithProperty(PropertyIds::Value, s);

	if (existingGroup.isValid())
		groupTree.removeChild(existingGroup, &um);
	else
	{
		ValueTree ng(UIPropertyIds::Group);
		ng.setProperty(PropertyIds::ID, "GROUP", nullptr);
		ng.setProperty(PropertyIds::Value, s, nullptr);
		groupTree.addChild(ng, -1, &um);
	}
}

}